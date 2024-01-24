#pragma once
#include "util.hh"
#include <alsa/asoundlib.h>

#define STRIDE (g::sampleRate * g::channels * sizeof(s16))

pointer<s16> WavLoad(const std::string_view path, size_t idk);

struct WavFile {
    pointer<s16> ptr;
    unsigned sample_rate;
    unsigned period_time;
    s8 channels;
    f32 volume;

    WavFile(const std::string_view path, size_t allign);
    ~WavFile();
    constexpr s16* data() { return ptr.data; }
    constexpr size_t size() { return ptr.size; }
    void play(snd_pcm_t* pcm);
    void load_wav(std::string_view path);
};

#define RIFF_CODE(a, b, c, d) ( (((u32)a) << 0) | (((u32)b) << 8) | (((u32)c) << 16) | (((u32)d) << 24) )

enum : u32 {
    wave_chunk_id_fmt = RIFF_CODE('f', 'm', 't', ' '),
    wave_chunk_id_data = RIFF_CODE('d', 'a', 't', 'a'),
    wave_chunk_id_riff = RIFF_CODE('R', 'I', 'F', 'F'),
    wave_chunk_id_wave = RIFF_CODE('W', 'A', 'V', 'E')
};

// Convert 32-bit unsigned little-endian value to big-endian from byte array
constexpr inline uint32_t
little2big_u32(uint8_t const* data)
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Convert 16-bit unsigned little-endian value to big-endian from byte array
constexpr inline uint16_t
little2big_u16(uint8_t const* data)
{
    return data[0] | (data[1] << 8);
}

// Copy n bytes from source to destination and terminate the destination with
// null character. Destination must be at least (amount + 1) bytes big to
// account for null character.
inline void
bytes_to_string(uint8_t const* source, char* destination, size_t amount)
{
    memcpy(destination, source, amount);
    destination[amount] = '\0';
}
