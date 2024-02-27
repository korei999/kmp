#pragma once
#include "ultratypes.h"

#include <cmath>
#include <ncurses.h>

#include <format>
#include <iostream>
#include <string_view>

#define Printf std::cout << std::format
#define Printe std::cerr << std::format

#define Die(...)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)fprintf(stderr, "DIED: %s(%u): ", __FILE__, __LINE__);                                                   \
        (void)fprintf(stderr, __VA_ARGS__);                                                                            \
        endwin();                                                                                                      \
    } while (0)

#define length(a) (sizeof(a) / sizeof(a[0]))

inline
f64
LinearToDB(const f64 linear)
{
    return 10.f * log10(linear);
}

template <typename T>
inline
T
Clamp(T& value, T& min, T& max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;

    return value;
}

inline
std::string
load_file(const std::string_view path)
{
    FILE* fp = fopen(path.data(), "rb");
    if (!fp)
        return {};

    fseek(fp, 0L, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    std::string s(file_size + 1, 0);
    fread(s.data(), 1L, file_size, fp);

    fclose(fp);
    return s;
}
