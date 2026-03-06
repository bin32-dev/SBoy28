#include "drivers/filesystem.h"
#include "common/utils.h"
#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t bk_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} FAT32BPB;

typedef struct __attribute__((packed)) {
    uint8_t name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} FATDirEntry;

typedef struct {
    FSBlockDevice* dev;
    FAT32BPB bpb;
    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t sectors_per_fat;
    uint32_t bytes_per_cluster;
    uint32_t cluster_count;
    uint8_t mounted;
} FSMount;

static FSMount g_fs;

static int memcmp_local(const void* a, const void* b, uint32_t len) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (uint32_t i = 0; i < len; ++i) {
        if (x[i] != y[i]) {
            return (int)x[i] - (int)y[i];
        }
    }
    return 0;
}

static uint8_t to_upper(uint8_t c) {
    if (c >= 'a' && c <= 'z') {
        return (uint8_t)(c - ('a' - 'A'));
    }
    return c;
}

static int read_sector(uint32_t sector, void* out) {
    uint32_t off = sector * g_fs.bpb.bytes_per_sector;
    if (off + g_fs.bpb.bytes_per_sector > g_fs.dev->size) {
        return FS_ERR_IO;
    }
    memcpy(out, g_fs.dev->base + off, g_fs.bpb.bytes_per_sector);
    return FS_OK;
}

static int write_sector(uint32_t sector, const void* in) {
    uint32_t off = sector * g_fs.bpb.bytes_per_sector;
    if (off + g_fs.bpb.bytes_per_sector > g_fs.dev->size) {
        return FS_ERR_IO;
    }
    memcpy(g_fs.dev->base + off, in, g_fs.bpb.bytes_per_sector);
    return FS_OK;
}

static uint32_t cluster_to_sector(uint32_t cluster) {
    return g_fs.data_start_sector + ((cluster - 2U) * g_fs.bpb.sectors_per_cluster);
}

static int get_fat_entry(uint32_t cluster, uint32_t* out_value) {
    uint32_t fat_offset = cluster * 4U;
    uint32_t sector = g_fs.fat_start_sector + (fat_offset / g_fs.bpb.bytes_per_sector);
    uint32_t offset = fat_offset % g_fs.bpb.bytes_per_sector;
    uint8_t sec[512];

    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) {
        return FS_ERR_IO;
    }

    int rc = read_sector(sector, sec);
    if (rc != FS_OK) return rc;

    uint32_t value = *(uint32_t*)(sec + offset);
    *out_value = value & 0x0FFFFFFFU;
    return FS_OK;
}

static int set_fat_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4U;
    uint32_t offset = fat_offset % g_fs.bpb.bytes_per_sector;
    uint8_t sec[512];

    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) {
        return FS_ERR_IO;
    }

    for (uint32_t fat_idx = 0; fat_idx < g_fs.bpb.num_fats; ++fat_idx) {
        uint32_t sector = g_fs.fat_start_sector + (fat_idx * g_fs.sectors_per_fat) + (fat_offset / g_fs.bpb.bytes_per_sector);
        int rc = read_sector(sector, sec);
        if (rc != FS_OK) return rc;

        uint32_t cur = *(uint32_t*)(sec + offset);
        cur &= 0xF0000000U;
        cur |= (value & 0x0FFFFFFFU);
        *(uint32_t*)(sec + offset) = cur;

        rc = write_sector(sector, sec);
        if (rc != FS_OK) return rc;
    }

    return FS_OK;
}

static int alloc_cluster(uint32_t* out_cluster) {
    for (uint32_t cl = 2; cl < g_fs.cluster_count + 2U; ++cl) {
        uint32_t v = 0;
        int rc = get_fat_entry(cl, &v);
        if (rc != FS_OK) return rc;
        if (v == 0) {
            rc = set_fat_entry(cl, 0x0FFFFFFFU);
            if (rc != FS_OK) return rc;
            *out_cluster = cl;
            return FS_OK;
        }
    }
    return FS_ERR_NO_SPACE;
}

static void fat_name_from_path_component(const char* comp, uint8_t out_name[11]) {
    for (int i = 0; i < 11; ++i) out_name[i] = ' ';

    int idx = 0;
    while (*comp && *comp != '/' && *comp != '.' && idx < 8) {
        out_name[idx++] = to_upper((uint8_t)*comp++);
    }

    if (*comp == '.') {
        ++comp;
        idx = 8;
        while (*comp && *comp != '/' && idx < 11) {
            out_name[idx++] = to_upper((uint8_t)*comp++);
        }
    }
}

static int next_component(const char** path, char* out_comp, uint32_t out_len) {
    const char* p = *path;
    while (*p == '/') p++;
    if (*p == '\0') {
        *path = p;
        return 0;
    }

    uint32_t n = 0;
    while (*p && *p != '/' && n + 1 < out_len) {
        out_comp[n++] = *p++;
    }
    out_comp[n] = '\0';
    while (*p == '/') p++;
    *path = p;
    return 1;
}

static int find_entry_in_dir(uint32_t dir_cluster, const uint8_t fat_name[11], FATDirEntry* out_entry,
                             uint32_t* out_sector, uint32_t* out_offset) {
    uint32_t cluster = dir_cluster;
    uint8_t sec[512];

    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) {
        return FS_ERR_IO;
    }

    while (cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint32_t base_sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < g_fs.bpb.sectors_per_cluster; ++s) {
            uint32_t sector = base_sector + s;
            int rc = read_sector(sector, sec);
            if (rc != FS_OK) return rc;

            for (uint32_t off = 0; off < g_fs.bpb.bytes_per_sector; off += sizeof(FATDirEntry)) {
                FATDirEntry* e = (FATDirEntry*)(sec + off);
                if (e->name[0] == 0x00) return FS_ERR_NOT_FOUND;
                if (e->name[0] == 0xE5 || e->attr == 0x0F) continue;
                if (memcmp_local(e->name, fat_name, 11) == 0) {
                    memcpy(out_entry, e, sizeof(FATDirEntry));
                    if (out_sector) *out_sector = sector;
                    if (out_offset) *out_offset = off;
                    return FS_OK;
                }
            }
        }

        uint32_t next = 0;
        int rc = get_fat_entry(cluster, &next);
        if (rc != FS_OK) return rc;
        cluster = next;
    }

    return FS_ERR_NOT_FOUND;
}

static uint32_t entry_first_cluster(const FATDirEntry* e) {
    return ((uint32_t)e->fst_clus_hi << 16) | e->fst_clus_lo;
}

static int resolve_path(const char* path, FATDirEntry* out_entry, uint32_t* out_sector, uint32_t* out_offset) {
    if (!path || path[0] != '/') return FS_ERR_INVALID;

    uint32_t current_cluster = g_fs.bpb.root_cluster;
    FATDirEntry current_entry;
    memset(&current_entry, 0, sizeof(current_entry));
    current_entry.attr = FS_ATTR_DIRECTORY;

    const char* cursor = path;
    char comp[32];

    while (next_component(&cursor, comp, sizeof(comp))) {
        uint8_t fat_name[11];
        fat_name_from_path_component(comp, fat_name);

        FATDirEntry found;
        uint32_t sec = 0;
        uint32_t off = 0;
        int rc = find_entry_in_dir(current_cluster, fat_name, &found, &sec, &off);
        if (rc != FS_OK) return rc;

        current_entry = found;
        current_cluster = entry_first_cluster(&found);
        if (*cursor != '\0' && !(found.attr & FS_ATTR_DIRECTORY)) {
            return FS_ERR_NOT_FOUND;
        }

        if (out_sector) *out_sector = sec;
        if (out_offset) *out_offset = off;
    }

    if (path[1] == '\0') {
        memset(out_entry, 0, sizeof(*out_entry));
        out_entry->attr = FS_ATTR_DIRECTORY;
        out_entry->fst_clus_lo = (uint16_t)(g_fs.bpb.root_cluster & 0xFFFF);
        out_entry->fst_clus_hi = (uint16_t)((g_fs.bpb.root_cluster >> 16) & 0xFFFF);
        out_entry->file_size = 0;
        if (out_sector) *out_sector = 0;
        if (out_offset) *out_offset = 0;
        return FS_OK;
    }

    memcpy(out_entry, &current_entry, sizeof(*out_entry));
    return FS_OK;
}

int fs_mount_fat32(FSBlockDevice* device) {
    if (!device || !device->base || device->size < 512) {
        return FS_ERR_INVALID;
    }

    memset(&g_fs, 0, sizeof(g_fs));
    g_fs.dev = device;
    memcpy(&g_fs.bpb, device->base, sizeof(FAT32BPB));

    if (g_fs.bpb.bytes_per_sector == 0 || g_fs.bpb.sectors_per_cluster == 0) {
        return FS_ERR_INVALID;
    }

    uint32_t total_sectors = g_fs.bpb.total_sectors_16 ? g_fs.bpb.total_sectors_16 : g_fs.bpb.total_sectors_32;
    g_fs.sectors_per_fat = g_fs.bpb.fat_size_16 ? g_fs.bpb.fat_size_16 : g_fs.bpb.fat_size_32;
    uint32_t root_dir_sectors = ((g_fs.bpb.root_entry_count * 32U) + (g_fs.bpb.bytes_per_sector - 1U)) / g_fs.bpb.bytes_per_sector;

    g_fs.fat_start_sector = g_fs.bpb.reserved_sector_count;
    g_fs.data_start_sector = g_fs.bpb.reserved_sector_count + (g_fs.bpb.num_fats * g_fs.sectors_per_fat) + root_dir_sectors;
    g_fs.bytes_per_cluster = g_fs.bpb.bytes_per_sector * g_fs.bpb.sectors_per_cluster;

    uint32_t data_sectors = total_sectors - g_fs.data_start_sector;
    g_fs.cluster_count = data_sectors / g_fs.bpb.sectors_per_cluster;

    if (g_fs.cluster_count < 65525 || g_fs.bpb.root_cluster < 2) {
        return FS_ERR_INVALID;
    }

    g_fs.mounted = 1;
    return FS_OK;
}

int fs_unmount(void) {
    memset(&g_fs, 0, sizeof(g_fs));
    return FS_OK;
}

int fs_open(const char* path, uint8_t mode, FSFile* out_file) {
    if (!g_fs.mounted) return FS_ERR_NOT_MOUNTED;
    if (!path || !out_file || mode == 0) return FS_ERR_INVALID;

    FATDirEntry e;
    uint32_t sec = 0;
    uint32_t off = 0;
    int rc = resolve_path(path, &e, &sec, &off);
    if (rc != FS_OK) return rc;

    if (e.attr & FS_ATTR_DIRECTORY) return FS_ERR_ACCESS;
    if ((mode & FS_OPEN_WRITE) && (e.attr & FS_ATTR_READ_ONLY)) return FS_ERR_ACCESS;

    memset(out_file, 0, sizeof(*out_file));
    out_file->first_cluster = entry_first_cluster(&e);
    out_file->size = e.file_size;
    out_file->mode = mode;
    out_file->dir_entry_sector = sec;
    out_file->dir_entry_offset = off;
    out_file->valid = 1;
    return FS_OK;
}

int fs_seek(FSFile* file, uint32_t position) {
    if (!file || !file->valid) return FS_ERR_INVALID;
    file->position = position;
    return FS_OK;
}

static int file_cluster_for_position(FSFile* file, uint32_t pos, uint8_t allow_extend, uint32_t* out_cluster) {
    uint32_t idx = pos / g_fs.bytes_per_cluster;
    uint32_t cluster = file->first_cluster;

    if (cluster < 2) {
        if (!allow_extend) return FS_ERR_IO;
        int rc = alloc_cluster(&cluster);
        if (rc != FS_OK) return rc;
        file->first_cluster = cluster;
    }

    for (uint32_t i = 0; i < idx; ++i) {
        uint32_t next = 0;
        int rc = get_fat_entry(cluster, &next);
        if (rc != FS_OK) return rc;

        if (next >= 0x0FFFFFF8U) {
            if (!allow_extend) return FS_ERR_IO;
            uint32_t new_cluster = 0;
            rc = alloc_cluster(&new_cluster);
            if (rc != FS_OK) return rc;
            rc = set_fat_entry(cluster, new_cluster);
            if (rc != FS_OK) return rc;
            cluster = new_cluster;
        } else {
            cluster = next;
        }
    }

    *out_cluster = cluster;
    return FS_OK;
}

int fs_read(FSFile* file, void* out_buffer, uint32_t bytes_to_read, uint32_t* out_bytes_read) {
    if (!g_fs.mounted) return FS_ERR_NOT_MOUNTED;
    if (!file || !file->valid || !out_buffer) return FS_ERR_INVALID;
    if (!(file->mode & FS_OPEN_READ)) return FS_ERR_ACCESS;

    if (file->position >= file->size) {
        if (out_bytes_read) *out_bytes_read = 0;
        return FS_OK;
    }

    uint32_t remaining = file->size - file->position;
    if (bytes_to_read > remaining) bytes_to_read = remaining;

    uint8_t* out = (uint8_t*)out_buffer;
    uint32_t done = 0;
    uint8_t sec[512];
    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) return FS_ERR_IO;

    while (done < bytes_to_read) {
        uint32_t cluster = 0;
        int rc = file_cluster_for_position(file, file->position, 0, &cluster);
        if (rc != FS_OK) return rc;

        uint32_t cluster_off = file->position % g_fs.bytes_per_cluster;
        uint32_t sector_in_cluster = cluster_off / g_fs.bpb.bytes_per_sector;
        uint32_t off_in_sector = cluster_off % g_fs.bpb.bytes_per_sector;
        uint32_t sector = cluster_to_sector(cluster) + sector_in_cluster;

        rc = read_sector(sector, sec);
        if (rc != FS_OK) return rc;

        uint32_t can = g_fs.bpb.bytes_per_sector - off_in_sector;
        if (can > bytes_to_read - done) can = bytes_to_read - done;

        memcpy(out + done, sec + off_in_sector, can);
        done += can;
        file->position += can;
    }

    if (out_bytes_read) *out_bytes_read = done;
    return FS_OK;
}

int fs_write(FSFile* file, const void* buffer, uint32_t bytes_to_write, uint32_t* out_bytes_written) {
    if (!g_fs.mounted) return FS_ERR_NOT_MOUNTED;
    if (!file || !file->valid || !buffer) return FS_ERR_INVALID;
    if (!(file->mode & FS_OPEN_WRITE)) return FS_ERR_ACCESS;

    const uint8_t* in = (const uint8_t*)buffer;
    uint32_t done = 0;
    uint8_t sec[512];
    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) return FS_ERR_IO;

    while (done < bytes_to_write) {
        uint32_t cluster = 0;
        int rc = file_cluster_for_position(file, file->position, 1, &cluster);
        if (rc != FS_OK) return rc;

        uint32_t cluster_off = file->position % g_fs.bytes_per_cluster;
        uint32_t sector_in_cluster = cluster_off / g_fs.bpb.bytes_per_sector;
        uint32_t off_in_sector = cluster_off % g_fs.bpb.bytes_per_sector;
        uint32_t sector = cluster_to_sector(cluster) + sector_in_cluster;

        rc = read_sector(sector, sec);
        if (rc != FS_OK) return rc;

        uint32_t can = g_fs.bpb.bytes_per_sector - off_in_sector;
        if (can > bytes_to_write - done) can = bytes_to_write - done;

        memcpy(sec + off_in_sector, in + done, can);
        rc = write_sector(sector, sec);
        if (rc != FS_OK) return rc;

        done += can;
        file->position += can;
        if (file->position > file->size) file->size = file->position;
    }

    if (out_bytes_written) *out_bytes_written = done;
    return FS_OK;
}

int fs_close(FSFile* file) {
    if (!g_fs.mounted) return FS_ERR_NOT_MOUNTED;
    if (!file || !file->valid) return FS_ERR_INVALID;

    if (file->mode & FS_OPEN_WRITE) {
        uint8_t sec[512];
        if (g_fs.bpb.bytes_per_sector > sizeof(sec)) return FS_ERR_IO;
        int rc = read_sector(file->dir_entry_sector, sec);
        if (rc != FS_OK) return rc;

        FATDirEntry* e = (FATDirEntry*)(sec + file->dir_entry_offset);
        e->file_size = file->size;
        e->fst_clus_hi = (uint16_t)((file->first_cluster >> 16) & 0xFFFF);
        e->fst_clus_lo = (uint16_t)(file->first_cluster & 0xFFFF);

        rc = write_sector(file->dir_entry_sector, sec);
        if (rc != FS_OK) return rc;
    }

    file->valid = 0;
    return FS_OK;
}

static void decode_short_name(const uint8_t name[11], char out[13]) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < 8 && name[i] != ' '; ++i) {
        out[n++] = (char)name[i];
    }

    if (name[8] != ' ') {
        out[n++] = '.';
        for (uint32_t i = 8; i < 11 && name[i] != ' '; ++i) {
            out[n++] = (char)name[i];
        }
    }
    out[n] = '\0';
}

int fs_list_dir(const char* path, FSDirEntryInfo* out_entries, uint32_t max_entries, uint32_t* out_count) {
    if (!g_fs.mounted) return FS_ERR_NOT_MOUNTED;
    if (!path || !out_entries || max_entries == 0 || !out_count) return FS_ERR_INVALID;

    FATDirEntry dir;
    int rc = resolve_path(path, &dir, NULL, NULL);
    if (rc != FS_OK) return rc;
    if (!(dir.attr & FS_ATTR_DIRECTORY)) return FS_ERR_INVALID;

    uint32_t cluster = entry_first_cluster(&dir);
    if (path[1] == '\0') cluster = g_fs.bpb.root_cluster;

    uint32_t count = 0;
    uint8_t sec[512];
    if (g_fs.bpb.bytes_per_sector > sizeof(sec)) return FS_ERR_IO;

    while (cluster >= 2 && cluster < 0x0FFFFFF8U) {
        uint32_t base_sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < g_fs.bpb.sectors_per_cluster; ++s) {
            rc = read_sector(base_sector + s, sec);
            if (rc != FS_OK) return rc;

            for (uint32_t off = 0; off < g_fs.bpb.bytes_per_sector; off += sizeof(FATDirEntry)) {
                FATDirEntry* e = (FATDirEntry*)(sec + off);
                if (e->name[0] == 0x00) {
                    *out_count = count;
                    return FS_OK;
                }
                if (e->name[0] == 0xE5 || e->attr == 0x0F) continue;
                if (count == max_entries) {
                    *out_count = count;
                    return FS_OK;
                }

                decode_short_name(e->name, out_entries[count].name);
                out_entries[count].attributes = e->attr;
                out_entries[count].size = e->file_size;
                ++count;
            }
        }

        uint32_t next = 0;
        rc = get_fat_entry(cluster, &next);
        if (rc != FS_OK) return rc;
        cluster = next;
    }

    *out_count = count;
    return FS_OK;
}

