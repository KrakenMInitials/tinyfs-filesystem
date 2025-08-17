//Open-source code for checksum generation

// CRC32 code based on public domain/zlib implementation:
// https://opensource.org/license/zlib/
// Author: Mark Adler (original table generation), public domain

#include <stdint.h>
#include <stddef.h>

#define POLY 0xEDB88320

static uint32_t crc_table[256];

// Generate the table once
static void generate_crc32_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (POLY & -(crc & 1));
        crc_table[i] = crc;
    }
}

// Calculate CRC32 over a buffer
uint32_t crc32(const void *data, size_t n_bytes) {
    static int table_initialized = 0;
    if (!table_initialized) {
        generate_crc32_table();
        table_initialized = 1;
    }

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *buf = (const uint8_t *)data;

    for (size_t i = 0; i < n_bytes; i++)
        crc = (crc >> 8) ^ crc_table[(crc ^ buf[i]) & 0xFF];

    return ~crc;
}
