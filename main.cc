#include "main.hh"
#include "player.hh"
#include "util.hh"
#include "wav.hh"

#include <alsa/asoundlib.h>
#include <locale.h>
#include <ncurses.h>
#include <opus/opusfile.h>
#include <printf.h>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <print>

/* gloabls */
namespace g {
    unsigned sampleRate = 48000;
    unsigned channels = 2;

    unsigned bufferTime = 100'000;       /* ring buffer length in us */ /* 500'000 us == 0.5 s */
    unsigned periodTime = 20'000;       /* period time in us */

    unsigned step = 200;
}

state State {
    .volume = 1.002f,
    .minVolume = 1.000f,
    .maxVolume = 1.201f,

    .paused = false,
    .exit = false,

    .repeatOnEnd = false,

    .next = false,
    .prev = false,

    .left = false,
    .right = false,

    .songInQ = 0,
};

WINDOW* listWindow;

std::mutex printMtx;
std::mutex playMutex;
std::condition_variable playCV;

void
PrintMinSec(size_t timeInSec)
{
    std::lock_guard lock(printMtx);

    f32 minF = timeInSec / 60.f;
    size_t minutes = minF;
    int frac = 60 * (minF - minutes);

    if (State.paused)
        mvprintw(0, 0, "(paused) %lu:%02d min", minutes, frac);
    else
        mvprintw(0, 0, "%lu:%02d min", minutes, frac);

    refresh();
}

void
PrintVolume()
{
    std::lock_guard lock(printMtx);

    long d = State.volume;
    long frac = 1000 * (State.volume - d);

    char volfmt[] {"volume: %03ld"};

    mvprintw(0, (stdscr->_maxx - Length(volfmt)), volfmt, frac >> 1);
    refresh();
}

void
PrintSongName()
{
    std::lock_guard pl(printMtx);

    clear();
    mvprintw(stdscr->_maxy - 1, 0, "%lu: playing:", State.songInQ);
    mvprintw(stdscr->_maxy, 0, "%s", State.songList[State.songInQ].data()); 
    refresh();
}

#ifndef NDEBUG
void
PrintCharPressed(char c)
{
    std::lock_guard pl(printMtx);

    char fmt[] {"pressed: %c(%d)"};
    mvprintw(stdscr->_maxy, (stdscr->_maxx - Length(fmt)), fmt, c, c);
}
#endif

void
ReadInput(void)
{
    char c;
    bool volume_changed = false;
    bool lockChanged = false;
    while ( (c = getch()) ) {
        switch (c) {
            case 'q':
                State.exit = true;

                if (State.paused)
                    playCV.notify_one();

                return;

            case 'n':
                State.next = true;
                break;

            case 'p':
                State.prev = true;
                break;

            case '0':
                State.volume += 0.005;
                volume_changed = true;
                break;
            case ')':
                State.volume += 0.001;
                volume_changed = true;
                break;

            case '9':
                State.volume -= 0.005;
                volume_changed = true;
                break;
            case '(':
                State.volume -= 0.001;
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

            case 12:
                PrintSongName();
                PrintVolume();
                break;

            default:
#ifndef NDEBUG
                PrintCharPressed(c);
#endif
                break;
        }

        State.volume = Clamp(State.volume, State.minVolume, State.maxVolume);

        if (volume_changed) {
            volume_changed = false;
            PrintVolume();
        }

        if (lockChanged && !State.paused) {
            lockChanged = false;
            playCV.notify_one();
        }
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

    PrintSongName();
    PrintMinSec(lengthInS);
    PrintVolume();

    /* some songs give !2 channels, and speedup playback */
    player::Alsa p("default", channels, ChunkSize);
    // p.Print();

    s16* chunk = new s16[ChunkSize];

    if (parser) {
        int err;
        long counter = 0;
        f64 rampVol = 0;
        while ((err = op_read_stereo(parser, chunk, p.periodTime) > 0)) {
            if (State.exit) {
                break;
            }

            if (!err) {
                // ...
                std::lock_guard pl(printMtx);
                mvprintw(0, stdscr->_maxx >> 1, "some error...\n");
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
                if (State.next)
                    State.songInQ++;

                if (State.songInQ == (long)State.songList.size())
                    State.songInQ = 0;

                break;
            }

            if (State.prev) {
                State.songInQ--;

                if (State.songInQ < 0)
                    State.songInQ = State.songList.size() - 1;

                break;
            }

            if (counter++ % 100) {
                now = op_pcm_tell(parser);
                PrintMinSec(now / p.sampleRate);
            }

            /* modify chunk */
            f64 vol = LinearToDB(State.volume);
            
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
                std::lock_guard pl(printMtx);
                mvprintw(0, stdscr->_maxx >> 1, "playback error\n");
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
    setlocale(LC_ALL, "");

    initscr();
    // start_color();
    curs_set(0);
    noecho();
    refresh();

    std::thread input(ReadInput);
    input.detach();

    for (int i = 1; i < argc; i++) {
        std::string_view songName = argv[i];
        if (songName.ends_with(".opus")) {
            State.songList.push_back(songName);
        }
    }

    while (State.songInQ < (long)State.songList.size()) {
        if (State.exit)
            break;

        OpusPlay(State.songList[State.songInQ]);
        if (State.prev) {
            State.prev = false;
            continue;
        }

        if (State.next) {
            State.next = false;
            continue;
        }

        State.songInQ++;
    }

    endwin();
    return 0;
}
