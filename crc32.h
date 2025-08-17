//Open-source code for checksum generation
// CRC32 code based on public domain/zlib implementation:
// https://opensource.org/license/zlib/
// Author: Mark Adler (original table generation), public domain

#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t crc32(const void *data, size_t n_bytes);

#endif
