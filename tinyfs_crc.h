#ifndef TINYFS_CRC_H
#define TINYFS_CRC_H

#include "libTinyFS.h"
#include "crc32.h" // assumes crc32(const void *data, size_t n_bytes) is available

// Superblock checksum
void set_superblock_checksum(Superblock *sb);
bool verify_superblock_checksum(const Superblock *sb);

// Inode checksum
void set_inode_checksum(Inode *inode);
bool verify_inode_checksum(const Inode *inode);

// Datablock checksum
void set_datablock_checksum(Datablock *db);
bool verify_datablock_checksum(const Datablock *db);

#endif
