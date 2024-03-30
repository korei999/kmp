#include "mp3.hh"
#include "main.hh"
#include "util.hh"

mp3_file::~mp3_file()
{
    mpg123_close(handle);
    mpg123_delete(handle);
}

int
mp3_file::open_file(std::string_view path)
{
    int ret = 0;
    handle = mpg123_new(nullptr, &ret);
    if (!handle)
    {
        Die("cannot create handle: %s\n", mpg123_plain_strerror(ret));
        return 1;
    }

    ret = mpg123_open_fixed(handle, path.data(), MPG123_STEREO, MPG123_ENC_SIGNED_16);
    if (ret != MPG123_OK)
    {
        Die("mpg123_open_fixed failed: %s\n", mpg123_plain_strerror(ret));
        return 1;
    }
    ret = mpg123_param2(handle, MPG123_VERBOSE, 0, 0);
    if (ret != MPG123_OK)
    {
        Die("mpg123_param2 failed: %s\n", mpg123_plain_strerror(ret));
        return 1;
    }

    int enc;
    ret = mpg123_getformat(handle, &sample_rate, &channels, &enc);
    if (ret != MPG123_OK)
    {
        Die("mpg123_getformat failed: %s\n", mpg123_plain_strerror(ret));
        return 1;
    }
    total = mpg123_length(handle);

#ifdef DEBUG
    Printe("sample_rate: {}, channels: {}, enc: {}, total: {}\n", sample_rate, channels, enc, total);
#endif

    return 0;
}

int
mp3_file::next_chunk()
{
    size_t done;
    int ret = 0;

    ret = mpg123_read(handle, chunk_ptr->data(), period_time * sizeof(s16), &done);

    if (ret == MPG123_OK)
        return 1;
    else
        return 0;
}

long 
mp3_file::now()
{
    return mpg123_tell(handle);
}

void
mp3_file::left(u64 pos)
{
    mpg123_seek(handle, pos - period_time * (def::step >> 2), SEEK_SET);
}

void
mp3_file::right(u64 pos)
{
    mpg123_seek(handle, pos + period_time * (def::step >> 2), SEEK_SET);
}
