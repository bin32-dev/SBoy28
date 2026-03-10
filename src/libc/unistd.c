#include <unistd.h>

#include "drivers/filesystem.h"

int open(const char *path, int flags) {
    (void)flags;
    return fs_open(path);
}

int read(int fd, void *buf, size_t count) {
    return fs_read(fd, buf, (int)count);
}

int write(int fd, const void *buf, size_t count) {
    return fs_write(fd, buf, (int)count);
}

int close(int fd) {
    return fs_close(fd);
}
