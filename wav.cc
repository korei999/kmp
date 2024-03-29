#include "wav.hh"
#include "main.hh"
#include "util.hh"

int
wav_file::open_file(const std::string_view path)
{
    std::string file_data = load_file(path);

    auto rd = file_data.data();
    size_t o = 0; /* offset */

    auto read_bytes_to_str = [&](size_t n) -> std::string
    {
        std::string r(n, 0);
        memcpy(r.data(), &rd[o], n);
        o += n;
        return r;
    };

    auto read_type_bytes = [&]<typename T>() -> T
    {
        T r = *(T*)(&rd[o]);
        o += sizeof(T);
        return r;
    };

    std::string riff_chunk_id {read_bytes_to_str(4)};
    if (riff_chunk_id != "RIFF")
    {
        Die("is this(%s) file not wave? (%s)\n", path.data(), riff_chunk_id.data());
        return 1;
    }

    s32 riff_chunk_size = read_type_bytes.operator()<decltype(riff_chunk_size)>();
    std::string wave_format {read_bytes_to_str(4)};
    std::string fmt {read_bytes_to_str(4)};
    // s32 fmt_id = read_type_bytes.operator()<decltype(fmt_id)>();
    s32 fmt_chunk_size = read_type_bytes.operator()<decltype(fmt_chunk_size)>();
    /* PCM = 1, anything else than 1 means compression */
    s16 audio_format = read_type_bytes.operator()<decltype(audio_format)>();
    s16 nchannels = read_type_bytes.operator()<decltype(nchannels)>();
    /* 8000, 44100, 48000 ... */
    s32 samples_per_second = read_type_bytes.operator()<decltype(samples_per_second)>();
    /* byte rate = sample rate * channels * bits per sample / 8 */
    s32 avg_byte_per_second = read_type_bytes.operator()<decltype(avg_byte_per_second)>();
    /* nchannels * bits_per_sample / 8 */
    s16 block_align = read_type_bytes.operator()<decltype(block_align)>();

    s16 bits_per_sample = read_type_bytes.operator()<decltype(bits_per_sample)>();

    /* TODO: figure out how to handle 24 bit Wave */
    this->format = SND_PCM_FORMAT_S16_LE;
    format_size = sizeof(s16);

    // switch (bits_per_sample)
    // {
        // case 24:
            // this->format = SND_PCM_FORMAT_S24_LE;
            // break;

        // case 16:
        // default:
            // this->format = SND_PCM_FORMAT_S16_LE;
            // break;
    // }

    // if (fmt_chunk_size != 16)
    // {
        // Die("fmt_chunk_size: '%d' != 16\n", fmt_chunk_size);
        // // exit(1);
    // }

    s16 extention_size = 0; /* size of the extension: 22 */
    s16 valid_bits_per_sample = 0; /* should be lower or equal to bits per sample */
    s32 channel_mask = 0; /* speaker position mask */
    std::string sub_format {}; /* GUID (first two bytes are the data format code) */
    switch (fmt_chunk_size)
    {
        case 18:
            extention_size = read_type_bytes.operator()<decltype(extention_size)>();
            break;

        case 40:
            extention_size = read_type_bytes.operator()<decltype(extention_size)>();
            valid_bits_per_sample = read_type_bytes.operator()<decltype(valid_bits_per_sample)>();
            channel_mask = read_type_bytes.operator()<decltype(channel_mask)>();
            sub_format = read_bytes_to_str(16);
            break;

        default:
            break;
    }

    std::string data_chunk_id = read_bytes_to_str(4);

    s32 list_size = 0;
    std::string list_info;
    if (data_chunk_id == "LIST" || data_chunk_id != "data")
    {
        list_size = read_type_bytes.operator()<decltype(list_size)>();
#ifdef DEBUG_WAVE
        Printe("list_size: {}\n", list_size);
#endif
        list_info = read_bytes_to_str(list_size);
        data_chunk_id = read_bytes_to_str(4);
    }

    /* number of bytes in the data */
    s32 data_chunk_size = read_type_bytes.operator()<decltype(data_chunk_size)>();
    /* now load data_chunk_size of the rest of the file */
#ifdef DEBUG_WAVE
    Printe("data_chunk_size: {}\n", data_chunk_size);
    Printe("riff_chunk_id: {}\n", riff_chunk_id);
    Printe("riff_chunk_size: {}\n", riff_chunk_size);
    Printe("wave_format: {}\n", wave_format);
    Printe("fmt: {}\n", fmt);
    // Printe("fmt_id: {}\n", fmt_id);
    Printe("fmt_chunk_size: {}\n", fmt_chunk_size);
    Printe("audio_format: {}\n", audio_format);
    Printe("nchannels: {}\n", nchannels);
    Printe("samples_per_second: {}\n", samples_per_second);
    Printe("block_align: {}\n", block_align);
    Printe("bits_per_sample: {}\n", bits_per_sample);
    Printe("extention_size: {}\n", extention_size);
    Printe("valid_bits_per_sample: {}\n", valid_bits_per_sample);
    Printe("channel_mask: {}\n", channel_mask);
    Printe("sub_format: {}\n", sub_format);
    Printe("data_chunk_id: {}\n", data_chunk_id);
    Printe("list_info: {}\n", list_info);
    Printe("data_chunk_size: {}\n", data_chunk_size);
#endif
    sample_rate = samples_per_second;
    channels = nchannels;
    this->block_align = block_align;
    this->riff_chunk_size = riff_chunk_size;

    song_data.resize(data_chunk_size * format_size, 0);
    memcpy(song_data.data(), &rd[o], data_chunk_size);

    return 0;
}

int
wav_file::next_chunk()
{
    /* FIXME: mul by 2, because if song has only 1 channel this check will be twice bigger then needed */
    if ((offset * 2) < (long)song_data.size())
    {
        memcpy(chunk_ptr->data(), &song_data[offset], period_time * format_size);
        offset += period_time * format_size;
        return 1;
    }

    return 0;
}

long
wav_file::now()
{
    return offset / channels;
}

long
wav_file::pcmtotal()
{
    return song_data.size() / sizeof(s16) / channels;
}

void
wav_file::left()
{
    offset -= (period_time * (def::step / 10));
    if (offset < 0)
        offset = 0;
}

void
wav_file::right()
{
    offset += (period_time * (def::step / 10));
}
