#ifndef SBOY28_UNISTD_H
#define SBOY28_UNISTD_H

#include <stddef.h>

int open(const char *path, int flags);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int close(int fd);

#endif
