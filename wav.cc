#include "main.hh"
#include "wav.hh"

#include <vector>

pointer<s16>
wav_load(const std::string_view path, size_t align)
{
    return file_load<s16>(path, align);
}

wav_file::wav_file(const std::string_view path, size_t allign) : ptr {wav_load(path, allign)}
{
}

wav_file::~wav_file()
{
    delete[] this->ptr.data;
}

void
wav_file::play(snd_pcm_t* pcm)
{
    s16* samples = this->data();
    long file_size = this->size();

    size_t sample_count = file_size / sizeof(s16);

    std::vector<s16> chunk(g::period_time);
    chunk.reserve(g::period_time);

    for (long offset = 0; offset < file_size; offset += g::period_time)
    {
        if (state.next || state.exit)
        {
            state.next = state.exit = false;
            return;
        }
        memcpy(chunk.data(), samples + offset, sizeof(s16) * g::period_time);

        for (size_t i = 0; i < g::period_time; i++)
        {
            chunk[i] *= state.volume;
        }

        snd_pcm_writei(pcm, chunk.data(), g::period_time / this->channels);
    }
}

void
wav_file::load_wav(std::string_view path)
{
}

// WAVFile
// WAV_ParseFileData(const u8* data)
// {
// WAVFile file;
// const u8* data_ptr = data;

// bytes_to_string(data_ptr, file.header.file_id, 4);
// data_ptr += 4;

// file.header.file_size = little2big_u32(data_ptr);
// data_ptr += 4;

// bytes_to_string(data_ptr, file.header.format, 4);
// data_ptr += 4;

// bytes_to_string(data_ptr, file.header.subchunk_id, 4);
// data_ptr += 4;

// file.header.subchunk_size = little2big_u32(data_ptr);
// data_ptr += 4;

// file.header.audio_format = little2big_u16(data_ptr);
// data_ptr += 2;

// file.header.number_of_channels = little2big_u16(data_ptr);
// data_ptr += 2;

// file.header.sample_rate = little2big_u32(data_ptr);
// data_ptr += 4;

// file.header.byte_rate = little2big_u32(data_ptr);
// data_ptr += 4;

// file.header.block_align = little2big_u16(data_ptr);
// data_ptr += 2;

// file.header.bits_per_sample = little2big_u16(data_ptr);
// data_ptr += 2;

// bytes_to_string(data_ptr, file.header.data_id, 4);
// data_ptr += 4;

// file.header.data_size = little2big_u32(data_ptr);
// data_ptr += 4;

// file.data = data_ptr;
// file.data_length = file.header.data_size;

// return file;
// }
