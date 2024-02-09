#include "wav.hh"
#include "main.hh"

#include <vector>

pointer<s16>
WavLoad(const std::string_view path, size_t align)
{
    return file_load<s16>(path, align);
}

WavFile::WavFile(const std::string_view path, size_t allign)
    : ptr { WavLoad(path, allign) } {}

WavFile::~WavFile()
{
    delete[] this->ptr.data;
}

void
WavFile::play(snd_pcm_t* pcm)
{
    s16* samples = this->data();
    long file_size = this->size();

	size_t sample_count = file_size / sizeof(s16);

    std::vector<s16> chunk(g::periodTime);
    chunk.reserve(g::periodTime);

    for (long offset = 0; offset < file_size; offset += g::periodTime) {
        if (State.next || State.exit) {
            State.next = State.exit = false;
            return;
        }
        memcpy(chunk.data(), samples + offset, sizeof(s16) * g::periodTime);

        for (size_t i = 0; i < g::periodTime; i++) {
            chunk[i] *= g::volume;
        }
        
        snd_pcm_writei(pcm, chunk.data(), g::periodTime / this->channels);
    }
}

void
WavFile::load_wav(std::string_view path)
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
