#include "main.hh"
#include "player.hh"
#include "util.hh"
#include "wav.hh"
#include "input.hh"

#include <alsa/asoundlib.h>
#include <locale.h>
#include <ncurses.h>
#include <opus/opusfile.h>
#include <printf.h>

#include <mutex>
#include <thread>
#include <format>

enum Clr : int {
    blackGreen = 1,
    blackYellow = 2,
    blackBlue = 3,
    blackRed = 4,
    redBlack = 5
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
    .volume = 1.010f,
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

    .inQ = 0,
    .inQSelected = 0,
    .selected = 0
};

WINDOW* songListWin;
WINDOW* songListSubWin;

std::mutex printMtx;
std::mutex playMtx;
std::condition_variable playCnd;

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

    int maxNumLen = std::to_string(State.songList.size()).size();
    std::string_view selfmt {"inQSelected: %*ld | selected: %*ld | inQ: %*ld | maxlines: %*ld | first: %*ld | second: %*ld]"};

    long maxlines = songListSubWin->_maxy + 1;

    long max = State.songList.size();
    long first = 0 + State.selected;
    long second = std::min(maxlines + State.selected, max);

    for (long i = 0; i < maxlines; i++) {
        long sel = i + State.selected;

        if (i + State.selected < (long)State.songList.size()) {
            std::string delpath {State.songList[sel]};
            delpath = delpath.substr(delpath.find_last_of("/") + 1, delpath.size());

            wmove(songListSubWin, i, 0);
            wclrtoeol(songListSubWin);

            if (sel == State.inQ)
                wattron(songListSubWin, COLOR_PAIR(Clr::redBlack));

            if (sel == State.inQSelected)
                wattron(songListSubWin, A_REVERSE);

            mvwprintw(songListSubWin, i, 1, "%.*s",  songListSubWin->_maxx - 2, delpath.data());

            wattroff(songListSubWin, A_REVERSE | COLOR_PAIR(Clr::redBlack));

            mvwprintw(songListSubWin,
                      0,
                      songListSubWin->_maxx - selfmt.size(),
                      selfmt.data(),
                      maxNumLen,
                      State.inQSelected,
                      maxNumLen,
                      State.selected,
                      maxNumLen,
                      State.inQ,
                      maxNumLen,
                      songListSubWin->_maxy - 1,
                      maxNumLen,
                      first,
                      maxNumLen,
                      second);
        } else {
            wmove(songListSubWin, i, 0);
            wclrtoeol(songListSubWin);
        }
    }

    wrefresh(songListSubWin);
}

void
PrintSongName()
{
    std::lock_guard pl(printMtx);

    move(4, 0);
    clrtoeol();
    printw("%lu / %lu playing:", State.inQ + 1, State.songList.size());

    move(5, 0);
    clrtoeol();
    attron(COLOR_PAIR(Clr::blackYellow));
    printw("%s", State.songList[State.inQ].data()); 

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

    /* TODO: borders and text residues on resize */
    // wmove(songListSubWin, songListSubWin->_begy, songListSubWin->_begx);
    // wclrtoeol(songListSubWin);

    box(songListWin, 0, 0);
    wrefresh(songListWin);
}

#ifdef DEBUG
void
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
    p.Print();

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

                std::unique_lock lock(playMtx);
                playCnd.wait(lock);

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
                    State.inQ++;

                if (State.inQ == (long)State.songList.size())
                    State.inQ = 0;

                break;
            }

            if (State.prev) {
                State.inQ--;

                if (State.inQ < 0)
                    State.inQ = State.songList.size() - 1;

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
    init_pair(4, COLOR_BLACK, COLOR_RED);
    init_pair(5, COLOR_RED, COLOR_BLACK);

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

    while (State.inQ < (long)State.songList.size()) {
        if (State.exit)
            break;

        OpusPlay(State.songList[State.inQ]);
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

        State.inQ++;
    }

    delwin(songListSubWin);
    delwin(songListWin);
    endwin();
    return 0;
}
