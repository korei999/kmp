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
#include <format>

enum Clr : int {
    blackGreen = 1,
    blackYellow = 2,
    blackBlue = 3,
};

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
    .maxVolume = 1.261f,

    .paused = false,
    .exit = false,

    .repeatOnEnd = false,

    .next = false,
    .prev = false,

    .left = false,
    .right = false,

    .pressedEnter = false,

    .songInQ = 0,
    .selected = 0
};

WINDOW* songListWin;
WINDOW* songListSubWin;

std::mutex printMtx;
std::mutex playMutex;
std::condition_variable playCV;

void
PrintMinSec(size_t timeInSec, size_t len)
{
    std::lock_guard lock(printMtx);

    f32 minF = timeInSec / 60.f;
    size_t minutes = minF;
    int frac = 60 * (minF - minutes);

    f32 minFmax = len / 60.f;
    size_t minutesmax = minFmax;
    int fracmax = 60 * (minFmax - minutesmax);

    move(0, 0);
    clrtoeol();
    if (State.paused)
        printw("(paused) %lu:%02d / %lu:%02d min", minutes, frac, minutesmax, fracmax);
    else
        printw("%lu:%02d / %lu:%02d min         ", minutes, frac, minutesmax, fracmax);

    refresh();
}

void
PrintVolume()
{
    std::lock_guard lock(printMtx);

    long d = State.volume;
    long frac = 1000 * (State.volume - d);

    char volfmt[] {"volume: %ld"};

    move(1, 0);
    clrtoeol();

    attron(COLOR_PAIR(Clr::blackGreen));
    printw(volfmt, frac / 2);
    attroff(COLOR_PAIR(Clr::blackGreen));
    refresh();
}

void
PrintSongList()
{
    std::lock_guard lock(printMtx);

    auto maxNumLen = std::to_string(State.songList.size()).size();
    std::string_view selfmt {"selected: %*u\tinQ: %*lu"};

    auto diff = songListSubWin->_maxy - State.songInQ;
    for (size_t i = 0; i < State.songList.size() && i < (size_t)songListSubWin->_maxy - 1; i++) {
        std::string delpath {State.songList[i]};
        delpath = delpath.substr(delpath.find_last_of("/") + 1, delpath.size());

        size_t off = i + 1;

        wmove(songListSubWin, off, 1);
        wclrtoeol(songListSubWin);

        if ((long)i == State.selected) {
            // wattron(songListSubWin, A_REVERSE);
            wattron(songListSubWin, COLOR_PAIR(Clr::blackBlue));
        }

        mvwprintw(songListSubWin, 1, songListSubWin->_maxx - selfmt.size() - maxNumLen*2 ,selfmt.data(), maxNumLen, State.selected, maxNumLen, State.songInQ);
        mvwprintw(songListSubWin, off, songListSubWin->_begx + 1, "%.*s",  songListSubWin->_maxx - 2, delpath.data());

        // wattroff(songListSubWin, A_REVERSE);
        wattroff(songListSubWin, COLOR_PAIR(Clr::blackBlue));
    }


    wrefresh(songListSubWin);
}

void
PrintSongName()
{
    std::lock_guard pl(printMtx);

    move(4, 0);
    clrtoeol();
    printw("%lu / %lu playing:", State.songInQ + 1, State.songList.size());

    move(5, 0);
    clrtoeol();
    attron(COLOR_PAIR(Clr::blackYellow));
    printw("%s", State.songList[State.songInQ].data()); 

    attroff(COLOR_PAIR(Clr::blackYellow));

    refresh();
}

void
RefreshWindows()
{
    std::lock_guard pl(printMtx);

    mvprintw(stdscr->_maxy, 0, "maxy: %d\tmaxx: %d", stdscr->_maxy, stdscr->_maxx);
    wresize(songListWin, stdscr->_maxy - 6, stdscr->_maxx);
    wresize(songListSubWin, songListWin->_maxy - 1, songListWin->_maxx - 1);

    wmove(songListSubWin, songListSubWin->_begy, songListSubWin->_begx);
    /* TODO: borders and test residues on resize */
    // wclrtoeol(songListSubWin);

    box(songListWin, 0, 0);
    wrefresh(songListWin);
}

#ifdef DEBUG
static void
PrintCharPressed(char c)
{
    std::lock_guard pl(printMtx);

    std::string_view fmt {"pressed: %c(%d)"};
    move(stdscr->_maxy, (stdscr->_maxx - fmt.size()));
    clrtoeol();
    printw(fmt.data(), c, c);
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
                State.volume += 0.003;
                volume_changed = true;
                break;
            case ')':
                State.volume += 0.001;
                volume_changed = true;
                break;

            case '9':
                State.volume -= 0.003;
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

            case 'j':
                State.selected++;

                // if (State.selected > std::min((long)State.songList.size(), (long)songListSubWin->_maxy - 2))
                    // State.selected = 0;

                if (State.selected > (long)State.songList.size())
                    State.selected = 0;

                PrintSongList();
                break;

            case 'k':
                State.selected--;

                if (State.selected < 0)
                    State.selected = std::min((long)(State.songList.size() - 1), (long)songListSubWin->_maxy - 2);

                PrintSongList();
                break;

            case '\n':
                State.pressedEnter = true;
                State.songInQ = State.selected;
                break;

            case 12:
                PrintSongName();
                PrintVolume();
                RefreshWindows();
                PrintSongList();
                break;

            default:
#ifdef DEBUG
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

    PrintMinSec(lengthInS, lengthInS);
    PrintSongName();
    PrintVolume();
    RefreshWindows();
    PrintSongList();

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
                PrintMinSec(now / p.sampleRate, lengthInS);

                p.Pause();

                std::unique_lock lock(playMutex);
                playCV.wait(lock);

                p.Resume();
            }

            if (State.pressedEnter) {
                break;
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
                PrintMinSec(now / p.sampleRate, lengthInS);
            }

            /* modify chunk */
            f64 vol = LinearToDB(State.volume);
            
            /* minimize crack at the very beggining of playback */
            if (rampVol <= 1.0) {
                vol *= rampVol;
                rampVol += 0.03; /* 34 increments */
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
    start_color();
    use_default_colors();
    curs_set(0);
    noecho();
    cbreak();
    refresh();

    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_BLACK, COLOR_BLUE);

    /* TODO: hardcoded numbers */
    songListWin = newwin(stdscr->_maxy - 6, stdscr->_maxx, 6, 0);
    box(songListWin, 0, 0);
    songListSubWin = subwin(songListWin,
                            songListWin->_maxy - 1,
                            songListWin->_maxx - 1,
                            songListWin->_begy + 1,
                            songListWin->_begx + 1);


    wrefresh(songListWin);

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

        if (State.pressedEnter) {
            State.pressedEnter = false;
            continue;
        }

        State.songInQ++;
    }

    delwin(songListSubWin);
    delwin(songListWin);
    endwin();
    return 0;
}
