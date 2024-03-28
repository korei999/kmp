#pragma once
#include "ultratypes.h"

#include <ncurses.h>

#include <condition_variable>
#include <mutex>
#include <string_view>
#include <vector>

/* Printing to ncurses screen is not thread safe, use global printMtx mutex lock */
void print_volume();
void print_song_list();
void refresh_windows();
void print_song_name();

enum clr : int
{
    termdef = -1, /* -1 should preserve terminal default color due to use_default_colors() */
    green = 1,
    yellow = 2,
    blue = 3,
    cyan = 4,
    red = 5
};

namespace def
{

extern unsigned sample_rate;
extern unsigned channels;

extern unsigned buffer_time; /* ring buffer length in us */
extern unsigned period_time; /* period time in us */

extern unsigned step;

}


struct program_state
{
    f64 volume = 1.005f;
    f64 min_volume = 1.000f;
    f64 max_volume = 1.350f;

    bool paused = false;
    bool exit = false;

    bool repeat_on_end = false;

    bool next = false;
    bool prev = false;

    bool left = false;
    bool right = false;

    bool pressed_enter = false;
    bool searching = false;

    std::vector<std::string_view> song_list {};
    long scrolloff = 0;

    /* first index of song to draw in the list */
    long first_to_draw = 0;
    long in_q = 0;
    long in_q_selected = 0;

    bool go_down = 0;
    bool go_up = 0;

    long size() const { return song_list.size(); }
    std::string_view song_in_q() const { return song_list[in_q]; }
};

extern WINDOW* song_list_win;
extern WINDOW* song_list_sub_win;
extern WINDOW* bottom_row;

extern program_state state;
extern std::mutex print_mtx;
extern std::mutex play_mtx;
extern std::condition_variable play_cnd;
