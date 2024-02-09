#pragma once
#include "ultratypes.h"

namespace g {
    extern unsigned sampleRate;
    extern unsigned channels;

    extern unsigned bufferTime;       /* ring buffer length in us */
    extern unsigned periodTime;       /* period time in us */

    extern f64 volume;
    extern f64 minVolume;
    extern f64 maxVolume;

    extern unsigned step;
}

struct state {
    bool paused = false;
    bool exit = false;

    bool repeatOnEnd = false;

    bool next = false;
    bool prev = false;

    bool left = false;
    bool right = false;

    int songInQ = 1;
    int songsTotal = 1;
};

extern state State;
