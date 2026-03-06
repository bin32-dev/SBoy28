#pragma once

#include <stdint.h>

#define FS_OK 0
#define FS_ERR_INVALID -1
#define FS_ERR_IO -2
#define FS_ERR_NOT_FOUND -3
#define FS_ERR_NOT_MOUNTED -4
#define FS_ERR_NO_SPACE -5
#define FS_ERR_ACCESS -6

#define FS_OPEN_READ  0x01
#define FS_OPEN_WRITE 0x02

#define FS_ATTR_READ_ONLY 0x01
#define FS_ATTR_HIDDEN    0x02
#define FS_ATTR_SYSTEM    0x04
#define FS_ATTR_VOLUME_ID 0x08
#define FS_ATTR_DIRECTORY 0x10
#define FS_ATTR_ARCHIVE   0x20

typedef struct {
    uint8_t* base;
    uint32_t size;
} FSBlockDevice;

typedef struct {
    char name[13];
    uint8_t attributes;
    uint32_t size;
} FSDirEntryInfo;

typedef struct {
    uint32_t first_cluster;
    uint32_t size;
    uint32_t position;
    uint8_t mode;
    uint32_t dir_entry_sector;
    uint32_t dir_entry_offset;
    uint8_t valid;
} FSFile;

int fs_mount_fat32(FSBlockDevice* device);
int fs_unmount(void);

int fs_open(const char* path, uint8_t mode, FSFile* out_file);
int fs_read(FSFile* file, void* out_buffer, uint32_t bytes_to_read, uint32_t* out_bytes_read);
int fs_write(FSFile* file, const void* buffer, uint32_t bytes_to_write, uint32_t* out_bytes_written);
int fs_seek(FSFile* file, uint32_t position);
int fs_close(FSFile* file);

int fs_list_dir(const char* path, FSDirEntryInfo* out_entries, uint32_t max_entries, uint32_t* out_count);

