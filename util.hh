#pragma once
#include "ultratypes.h"

#include <cmath>
#include <ncurses.h>

#include <format>
#include <iostream>
#include <string_view>

#define Printf std::cout << std::format
#define Printe std::cerr << std::format

#define Die(...)                                            \
    do {                                                    \
        /* severity: ERROR/WARNING */                       \
        (void)printf("DIED: %s(%u): ", __FILE__, __LINE__); \
        (void)printf(__VA_ARGS__);                          \
        endwin();                                           \
    } while (0)

#define Length(a) (sizeof(a) / sizeof(a[0]))

inline constexpr f64
LinearToDB(const f64 linear)
{
    return 10.f * log10(linear);
}

template <class T>
inline constexpr T
Clamp(T& value, T& min, T& max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;

    return value;
}

template <class T>
struct pointer {
    T* data;
    size_t size;

    pointer() = default;

    pointer(T* ptr, size_t n)
        : data { ptr }, size { n } {}
};

template <class T>
inline pointer<T>
file_load(const std::string_view path, size_t align = 1)
{
    FILE* fp = fopen(path.data(), "rb");
    if (!fp)
        return pointer<T> {};

    fseek(fp, 0L, SEEK_END);
    size_t file_size = ftell(fp);
    rewind(fp);

    size_t alligned_size = align * ceil(file_size / (f32)align);
    file_size = alligned_size;

    T* n = new T[file_size]();
    fread(n, 1L, file_size, fp);

    fclose(fp);
    return pointer(n, file_size);
}
