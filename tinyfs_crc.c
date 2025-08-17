//DISCLAIMER: AI-generated because mundane repetitions

#include "tinyfs_crc.h"
#include <string.h> // for memcpy, optional

// --------------------- Superblock ---------------------

void set_superblock_checksum(Superblock *sb) {
    sb->checksum = 0;
    sb->checksum = (uint16_t)(crc32(sb, sizeof(Superblock)) & 0xFFFF);
}

bool verify_superblock_checksum(const Superblock *sb) {
    Superblock temp = *sb;
    temp.checksum = 0;
    uint16_t expected = (uint16_t)(crc32(&temp, sizeof(Superblock)) & 0xFFFF);
    return sb->checksum == expected;
}

// ------------------------ Inode ------------------------

void set_inode_checksum(Inode *inode) {
    inode->checksum = 0;
    inode->checksum = (uint16_t)(crc32(inode, sizeof(Inode)) & 0xFFFF);
}

bool verify_inode_checksum(const Inode *inode) {
    Inode temp = *inode;
    temp.checksum = 0;
    uint16_t expected = (uint16_t)(crc32(&temp, sizeof(Inode)) & 0xFFFF);
    return inode->checksum == expected;
}

// ---------------------- Datablock ----------------------

void set_datablock_checksum(Datablock *db) {
    db->checksum = 0;
    db->checksum = (uint16_t)(crc32(db->data, sizeof(db->data)) & 0xFFFF);
}

bool verify_datablock_checksum(const Datablock *db) {
    uint16_t expected = (uint16_t)(crc32(db->data, sizeof(db->data)) & 0xFFFF);
    return db->checksum == expected;
}
