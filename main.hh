#pragma once
#include "ultratypes.h"

#include <ncurses.h>

#include <condition_variable>
#include <mutex>
#include <string_view>
#include <vector>

/* Printing to ncurses screen is not thread safe, use global printMtx mutex lock */
void PrintVolume();
void PrintCharDebug(char c);
void PrintSongList();
void RefreshWindows();
void PrintSongName();

namespace g {
    extern unsigned sampleRate;
    extern unsigned channels;

    extern unsigned bufferTime;       /* ring buffer length in us */
    extern unsigned periodTime;       /* period time in us */

    extern unsigned step;
}

struct state {
    f64 volume = 1.002f;
    f64 minVolume = 1.000f;
    f64 maxVolume = 1.201f;

    bool paused = false;
    bool exit = false;

    bool repeatOnEnd = false;

    bool next = false;
    bool prev = false;

    bool left = false;
    bool right = false;

    bool pressedEnter = false;

    std::vector<std::string_view> songList {};
    long scrolloff = 3;

    /* first index of song to draw in the list */
    long firstToDraw = 0;
    long inQ = 0;
    long inQSelected = 0;

    bool goDown = 0;
    bool goUp = 0;
};

extern WINDOW* songListWin;
extern WINDOW* songListSubWin;
extern WINDOW* bottomRow;

extern state State;
extern std::mutex printMtx;
extern std::mutex playMtx;
extern std::condition_variable playCnd;
