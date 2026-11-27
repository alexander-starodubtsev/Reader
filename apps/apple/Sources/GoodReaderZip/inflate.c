#include "GoodReaderZip.h"

#include <limits.h>
#include <zlib.h>

int goodreader_inflate_raw(const uint8_t *input, size_t input_size, uint8_t *output,
                           size_t output_size) {
    if (input == NULL || output == NULL || input_size > UINT_MAX || output_size > UINT_MAX) {
        return Z_STREAM_ERROR;
    }

    z_stream stream = {0};
    stream.next_in = (Bytef *)input;
    stream.avail_in = (uInt)input_size;
    stream.next_out = output;
    stream.avail_out = (uInt)output_size;

    int status = inflateInit2(&stream, -MAX_WBITS);
    if (status != Z_OK) {
        return status;
    }

    status = inflate(&stream, Z_FINISH);
    const int output_matches = stream.total_out == output_size;
    inflateEnd(&stream);
    return status == Z_STREAM_END && output_matches ? Z_OK : Z_DATA_ERROR;
}
