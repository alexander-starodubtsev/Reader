#ifndef GOODREADER_ZIP_H
#define GOODREADER_ZIP_H

#include <stddef.h>
#include <stdint.h>

int goodreader_inflate_raw(const uint8_t *input, size_t input_size, uint8_t *output,
                           size_t output_size);

#endif
