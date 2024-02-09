#include "util.hh"
#include "wav.hh"
#include "main.hh"
#include "player.hh"

#include <termios.h>
#include <alsa/asoundlib.h>
#include <cmath>
#include <string>
#include <vector>
#include <thread>
#include <opus/opusfile.h>
#include <mutex>
#include <condition_variable>

/* gloabls */
namespace g {
    unsigned sampleRate = 48000;
    unsigned channels = 2;

    unsigned bufferTime = 100'000;       /* ring buffer length in us */ /* 500'000 us == 0.5 s */
    unsigned periodTime = 20'000;       /* period time in us */

    f64 volume = 1.002f;
    f64 minVolume = 1.000f;
    f64 maxVolume = 1.201f;

    unsigned step = 200;
}

state State {
    .paused = false,
    .exit = false,

    .repeatOnEnd = false,

    .next = false,
    .prev = false,

    .left = false,
    .right = false,

    .songInQ = 1,
    .songsTotal = 1
};

std::mutex playMutex;
std::condition_variable playCV;

void
PrintMinSec(size_t timeInSec)
{
    f32 minF = timeInSec / 60.f;
    size_t minutes = minF;
    f32 frac = 60 * (minF - minutes);

    if (State.paused)
        Printf("\r(paused) {}:{:02.0f} min", minutes, frac);
    else
        Printf("\r{}:{:02.0f} min         ", minutes, frac);

    fflush(stdout);
}

void
TermRawMode(void)
{
	termios raw;
	tcgetattr(STDIN_FILENO, &raw);
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void
ReadInput(void)
{
    int c;
    bool volume_changed = false;
    bool lockChanged = false;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        switch (c) {
            case 'q':
                State.exit = true;

                if (State.paused)
                    playCV.notify_one();

                exit(1);

            case 'n':
                State.next = true;
                break;

            case 'p':
                State.prev = true;
                break;

            case '0':
                g::volume += 0.005;
                volume_changed = true;
                break;
            case ')':
                g::volume += 0.001;
                volume_changed = true;
                break;

            case '9':
                g::volume -= 0.005;
                volume_changed = true;
                break;
            case '(':
                g::volume -= 0.001;
                volume_changed = true;
                break;

            case 'l':
            case 'L':
                State.right = true;
                break;

            case 'h':
            case 'H':
                State.left = true;
                break;

            case 'c':
            case ' ':
                State.paused = !State.paused;
                lockChanged = true;
                break;

            default:
                break;
        }

        g::volume = Clamp(g::volume, g::minVolume, g::maxVolume);

        if (volume_changed) {
            volume_changed = false;
            Printf("\r                                             ");
            long d = g::volume;
            long frac = 1000 * (g::volume - d);
            Printf("\r                              volume: {}", frac);
        }

        if (lockChanged && !State.paused) {
            lockChanged = false;
            playCV.notify_one();
        }

        fflush(stdout);
    }
}

void
OpusPlay(const std::string_view s)
{
    OggOpusFile* parser = op_open_file(s.data(), NULL);
    auto link = op_current_link(parser);
    auto channels = op_channel_count(parser, link);
    auto pcmtotal = op_pcm_total(parser, link);

    /* sampleRate of opus is always 48KHz */
    auto lengthInS = pcmtotal / 48000;

    /* opus can do 1920 max haven't figured why tho */
    u32 ChunkSize = 1920;

    PrintMinSec(lengthInS);
    putchar('\n');

    /* some songs give !2 channels, and speedup playback */
    player::Alsa p("default", channels, ChunkSize);
    p.Print();

    s16* chunk = new s16[ChunkSize];

    if (parser) {
        int err;
        long counter = 0;
        f64 rampVol = 0;
        while ((err = op_read_stereo(parser, chunk, p.periodTime) > 0)) {
            if (State.exit) {
                State.exit = false;
                break;
            }

            if (!err) {
                // ...
                Printf("some error...\n");
            }
            
            u64 now = op_pcm_tell(parser);

            if (State.paused) {
                PrintMinSec(now / p.sampleRate);

                p.Pause();

                std::unique_lock lock(playMutex);
                playCV.wait(lock);

                p.Resume();
            }

            if (State.right) {
                op_pcm_seek(parser, now + p.periodTime * g::step);
                State.right = false;
                continue;
            }

            if (State.left) {
                op_pcm_seek(parser, now - p.periodTime * g::step);
                State.left = false;
                continue;
            }

            if (State.next || State.repeatOnEnd) {
                if (State.songInQ == State.songsTotal) {
                    State.songInQ = 0;
                }

                if (State.next) {
                    State.next = false;
                    break;
                }
            }

            if (State.prev) {
                State.prev = false;
                State.songInQ -= 2;

                if (State.songInQ < 0)
                    State.songInQ = State.songsTotal - 1;

                break;
            }

            if (counter++ % 100) {
                now = op_pcm_tell(parser);
                PrintMinSec(now / p.sampleRate);
            }

            /* modify chunk */
            f64 vol = LinearToDB(g::volume);
            
            /* minimize crack at the very beggining of playback */
            if (rampVol <= 1.0) {
                vol *= rampVol;
                rampVol += 0.02; /* 50 increments */
            }

            for (size_t i = 0; i < p.periodTime; i += 2) { /* 2 channels hardcoded */
                chunk[i    ] *= vol; /* right */
                chunk[i + 1] *= vol; /* left */
            }

            if (p.Play(chunk, ChunkSize) == -1) {
                Printf("playback error\n");
                break;
            }
        }
    }

    delete[] chunk;
    op_free(parser);
}

void
WavPlay(const std::string_view s)
{
    player::Alsa p("default", 2, g::periodTime, 48000);
    WavFile wav(s, STRIDE);
    wav.channels = p.channels;

    if (wav.data())
        wav.play(p.handle);
}

int
main(int argc, char* argv[])
{
    TermRawMode();

    std::jthread input(ReadInput);

    State.songsTotal = argc - 1;
    Printf("songs queued: {}\n", State.songsTotal);
    for (State.songInQ = 1; State.songInQ < argc; State.songInQ++) {
        if (argv[State.songInQ]) {
            std::string_view songName = argv[State.songInQ];

            if (songName.ends_with(".opus") || songName.ends_with(".ogg")) {
                Printf("\n{}: playing: {}\n", State.songInQ, argv[State.songInQ]);
                OpusPlay(songName);
            } else if (songName.ends_with(".wav")) {
                Printf("\n{}: playing: {}\n", State.songInQ, argv[State.songInQ]);
                WavPlay(songName);
            } else {
                Printf("\nskipping: {}\n", argv[State.songInQ]);
            }
        }
    }

    exit(0);
}
