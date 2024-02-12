#pragma once
#include "ultratypes.h"

#include <ncurses.h>

#include <vector>
#include <string_view>

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

    std::vector<std::string_view> songList {};
    long songInQ = 0;
};

extern state State;
extern std::mutex printMtx;

