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

    extern bool exit;

    extern unsigned step;

    extern bool next;
    extern bool prev;

    extern bool left;
    extern bool right;

    extern bool repeatOnEnd;

    extern int songInQ;
    extern int songsTotal;

    extern bool paused;
}
