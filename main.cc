#include "main.hh"
#include "player.hh"
#include "util.hh"
#include "input.hh"

#include <alsa/asoundlib.h>
#include <locale.h>
#include <ncurses.h>
#include <opus/opusfile.h>

#include <mutex>
#include <thread>

/* gloabls defaults */
namespace g
{

unsigned sample_rate = 48000;
unsigned channels = 2;

unsigned buffer_time = 50'000; /* ring buffer length in us */ /* 500'000 us == 0.5 s */
unsigned period_time = 25'000;                                /* period time in us */

unsigned step = 200;

} // namespace g

program_state state
{
    .volume = 1.010f,
    // .min_volume = 1.000f,
    // .max_volume = 1.261f,
};

WINDOW* song_list_win;
WINDOW* song_list_sub_win;
WINDOW* bottom_row;

std::mutex print_mtx;
std::mutex play_mtx;
std::condition_variable play_cnd;

void
print_min_sec(size_t time_in_sec, size_t len)
{
    std::lock_guard lock(print_mtx);

    f32 min_f = time_in_sec / 60.f;
    size_t minutes = min_f;
    int frac = 60 * (min_f - minutes);

    f32 min_f_max = len / 60.f;
    size_t minutes_max = min_f_max;
    int frac_max = 60 * (min_f_max - minutes_max);

    move(0, 0);
    clrtoeol();
    if (state.paused)
        printw("(paused) %lu:%02d / %lu:%02d min", minutes, frac, minutes_max, frac_max);
    else
        printw("%lu:%02d / %lu:%02d min         ", minutes, frac, minutes_max, frac_max);

    refresh();
}

void
print_volume()
{
    std::lock_guard lock(print_mtx);

    long d = state.volume;
    long frac = 1000 * (state.volume - d);

    char volfmt[] {"volume: %ld"};

    move(1, 0);
    clrtoeol();

    attron(A_BOLD | COLOR_PAIR(clr::green));
    printw(volfmt, frac / 2);
    attroff(A_BOLD | COLOR_PAIR(clr::green));
    refresh();
}

inline static
void
print_song_list_in_range(long first, long last)
{
    long maxlines = song_list_sub_win->_maxy + 1;
    long size = state.song_list.size();

    if (size < maxlines)
        first = 0;
    else if (last > size && (size - maxlines) >= 0)
        first = size - maxlines;

    for (long i = 0; i < maxlines; i++)
    {
        long sel = i + first;
        if (sel < size)
        {
            std::string delpath {state.song_list[sel]};
            delpath = delpath.substr(delpath.find_last_of("/") + 1, delpath.size());

            wmove(song_list_sub_win, i, 0);
            wclrtoeol(song_list_sub_win);

            if (sel == state.in_q)
                wattron(song_list_sub_win, COLOR_PAIR(clr::yellow));
            if (sel == state.in_q_selected)
                wattron(song_list_sub_win, A_REVERSE);

            mvwprintw(song_list_sub_win, i, 1, "%.*s", song_list_sub_win->_maxx - 1, delpath.data());
            wattroff(song_list_sub_win, A_REVERSE | COLOR_PAIR(clr::yellow));
        }
        else
        {
            wmove(song_list_sub_win, i, 0);
            wclrtoeol(song_list_sub_win);
        }
    }

    wrefresh(song_list_sub_win);
}

void
print_song_list()
{
    std::lock_guard lock(print_mtx);

    long size = state.song_list.size();
    int maxNumLen = std::to_string(size).size();
    // std::string_view selfmt {"inQSelected: %*ld | inQ: %*ld | maxlines: %*ld | first: %*ld | second: %*ld]"};

    long last = state.first_to_draw + song_list_sub_win->_maxy + 1;

    /* pressing j */
    if (state.go_down && state.in_q_selected >= (last - state.scrolloff))
    {
        state.go_down = false;

        if (state.in_q_selected < (size - state.scrolloff))
            state.first_to_draw++;
    }

    /* pressing k */
    if (state.go_up && state.in_q_selected < (state.first_to_draw + state.scrolloff))
    {
        state.go_up = false;

        if (state.in_q_selected >= state.scrolloff)
            state.first_to_draw--;
    }

    print_song_list_in_range(state.first_to_draw, last);
}

void
print_song_name()
{
    std::lock_guard pl(print_mtx);

    move(4, 0);
    clrtoeol();
    if (!state.repeat_on_end)
        printw("%lu / %lu playing:    ", state.in_q + 1, state.song_list.size());
    else
        printw("%lu / %lu playing: (R)", state.in_q + 1, state.song_list.size());

    move(5, 0);
    clrtoeol();
    attron(A_BOLD | COLOR_PAIR(clr::yellow));
    printw("%s", state.song_list[state.in_q].data()); 

    attroff(A_BOLD | COLOR_PAIR(clr::yellow));

    refresh();
}

void
refresh_windows()
{
    std::lock_guard pl(print_mtx);

    wresize(song_list_win, stdscr->_maxy - 6, stdscr->_maxx);
    wresize(song_list_sub_win, song_list_win->_maxy - 1, song_list_win->_maxx - 1);
    mvwin(bottom_row, stdscr->_maxy, 0);

    box(song_list_win, 0, 0);
    move(stdscr->_maxy, 0);
    clrtoeol();
    wrefresh(song_list_win);
}

void
play_file(const std::string_view s)
{
    int err;

    /* some songs give !2 channels, and speedup playback */
    player::alsa p(s);

    auto length_in_s = p.pcmtotal / p.sample_rate;

    p.print();
    print_min_sec(length_in_s, length_in_s);
    print_song_name();
    print_volume();
    refresh_windows();
    print_song_list();

    long counter = 0;
    f64 rampVol = 0;
    while ((err = p.next_chunk() > 0))
    {
        if (state.exit)
            break;

        if (!err)
        {
            // ...
            std::lock_guard pl(print_mtx);
            mvprintw(0, stdscr->_maxx >> 1, "some error...\n");
        }

        if (state.paused)
        {
            print_min_sec(p.now / p.sample_rate, length_in_s);

            p.pause();

            std::unique_lock lock(play_mtx);
            play_cnd.wait(lock);

            p.resume();
        }

        if (state.pressed_enter)
            break;

        if (state.right)
        {
            p.right();
            state.right = false;
            continue;
        }

        if (state.left)
        {
            p.left();
            state.left = false;
            continue;
        }

        if (state.next)
        {
            if (state.next)
                state.in_q++;

            if (state.in_q == state.size())
                state.in_q = 0;

            break;
        }

        if (state.prev)
        {
            state.in_q--;

            if (state.in_q < 0)
                state.in_q = state.size() - 1;

            break;
        }

        if (counter++ % 50 && !state.searching)
        {
            print_min_sec(p.now / p.sample_rate, length_in_s);
        }

        /* modify chunk */
        f64 vol = LinearToDB(state.volume);

        /* minimize crack at the very beggining of playback */
        if (rampVol <= 1.0)
        {
            vol *= rampVol;
            rampVol += 0.1;
        }

        for (size_t i = 0; i < p.chunk.size(); i += p.channels) /* 2 channels hardcoded */
        {
            for (size_t j = 0; j < p.channels; j++)
                p.chunk[i + j] *= vol;
        }

        if (p.play_chunk() == -1)
        {
            Die("playback error\n");
            break;
        }
    }
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
    keypad(stdscr, true);
    keypad(bottom_row, true);
    refresh();

    /* -1 to preserve default */
    int td = (int)clr::termdef;
    init_pair(clr::green, COLOR_GREEN, td);
    init_pair(clr::yellow, COLOR_YELLOW, td);
    init_pair(clr::blue, COLOR_BLUE, td);
    init_pair(clr::cyan, COLOR_CYAN, td);
    init_pair(clr::red, COLOR_RED, td);

    /* TODO: hardcoded numbers */
    song_list_win = newwin(stdscr->_maxy - 6, stdscr->_maxx, 6, 0);
    box(song_list_win, 0, 0);
    song_list_sub_win = subwin(song_list_win,
                               song_list_win->_maxy - 1,
                               song_list_win->_maxx - 1,
                               song_list_win->_begy + 1,
                               song_list_win->_begx + 1);

    bottom_row = newwin(0, 0, stdscr->_maxy, 0);
    wrefresh(bottom_row);

    wrefresh(song_list_win);

    std::thread input(read_input);
    input.detach();

    for (long i = 1; i < argc; i++)
    {
        std::string_view song_name = argv[i];
        if (song_name.ends_with(".opus") || song_name.ends_with(".wav")) {
            state.song_list.push_back(song_name);
        }
    }

    long size = state.size();
    while (state.in_q < size)
    {
        if (state.exit)
            break;

        std::string_view inq_str = state.song_in_q();
        play_file(inq_str);

        if (state.prev)
        {
            state.prev = false;
            continue;
        }

        if (state.next)
        {
            state.next = false;
            continue;
        }

        if (state.pressed_enter)
        {
            state.pressed_enter = false;
            continue;
        }

        if (state.repeat_on_end && state.in_q == size - 1)
        {
            state.in_q = 0;
            continue;
        }

        state.in_q++;
    }

    delwin(song_list_sub_win);
    delwin(song_list_win);
    endwin();
    fflush(stderr);
    return 0;
}
