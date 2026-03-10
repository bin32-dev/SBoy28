#include <stdio.h>
#include <unistd.h>

#include "common/utils.h"

struct sboy28_file {
    int fd;
    unsigned int flags;
    int eof;
    int error;
    int ungot_char;
};

#define FILE_FLAG_STATIC_STREAM (1u << 0)
#define FILE_FLAG_IN_USE        (1u << 1)

static struct sboy28_file g_stdin  = {-1, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};
static struct sboy28_file g_stdout = {-1, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};
static struct sboy28_file g_stderr = {-1, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};

#define MAX_DYNAMIC_FILES 16
static struct sboy28_file g_dynamic_streams[MAX_DYNAMIC_FILES];

static struct sboy28_file *stdio_alloc_stream(void) {
    int i;
    for (i = 0; i < MAX_DYNAMIC_FILES; ++i) {
        if ((g_dynamic_streams[i].flags & FILE_FLAG_IN_USE) == 0) {
            g_dynamic_streams[i].fd = -1;
            g_dynamic_streams[i].flags = FILE_FLAG_IN_USE;
            g_dynamic_streams[i].eof = 0;
            g_dynamic_streams[i].error = 0;
            g_dynamic_streams[i].ungot_char = EOF;
            return &g_dynamic_streams[i];
        }
    }

    return (struct sboy28_file *)0;
}

static void stdio_free_stream(struct sboy28_file *f) {
    if (!f) return;
    f->fd = 0;
    f->flags = 0;
    f->eof = 0;
    f->error = 0;
    f->ungot_char = EOF;
}

static int stdio_mode_to_flags(const char *mode) {
    if (!mode || !mode[0]) {
        return -1;
    }

    if (mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a') {
        return 0;
    }

    return -1;
}

FILE *stdin = &g_stdin;
FILE *stdout = &g_stdout;
FILE *stderr = &g_stderr;

FILE *fopen(const char *filename, const char *mode) {
    int fd;
    struct sboy28_file *file;

    if (!filename || stdio_mode_to_flags(mode) < 0) {
        return (FILE *)0;
    }

    fd = open(filename, stdio_mode_to_flags(mode));
    if (fd < 0) {
        return (FILE *)0;
    }

    file = stdio_alloc_stream();
    if (!file) {
        (void)close(fd);
        return (FILE *)0;
    }

    file->fd = fd;
    return (FILE *)file;
}

FILE *freopen(const char *filename, const char *mode, FILE *stream) {
    if (stream) {
        (void)fclose(stream);
    }
    return fopen(filename, mode);
}

int fclose(FILE *stream) {
    if (stream == (FILE *)0) {
        return -1;
    }

    if (((struct sboy28_file *)stream)->flags & FILE_FLAG_STATIC_STREAM) {
        return 0;
    }

    if (close(((struct sboy28_file *)stream)->fd) < 0) {
        ((struct sboy28_file *)stream)->error = 1;
        return -1;
    }

    stdio_free_stream((struct sboy28_file *)stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    if (stream == (FILE *)0) {
        return 0;
    }

    if (size == 0 || nmemb == 0) {
        return 0;
    }

    {
        size_t total = size * nmemb;
        int bytes = read(((struct sboy28_file *)stream)->fd, ptr, total);
        if (bytes < 0) {
            ((struct sboy28_file *)stream)->error = 1;
            return 0;
        }
        if ((size_t)bytes < total) {
            ((struct sboy28_file *)stream)->eof = 1;
        }
        return (size_t)bytes / size;
    }
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    if (stream == (FILE *)0) {
        return 0;
    }

    if (stream == stdout || stream == stderr) {
        const char *chars = (const char *)ptr;
        size_t total = size * nmemb;
        char temp[2] = {0, 0};
        size_t i;
        for (i = 0; i < total; ++i) {
            temp[0] = chars[i];
            print_string(temp);
        }
        return nmemb;
    }

    if (size == 0 || nmemb == 0) {
        return 0;
    }

    {
        size_t total = size * nmemb;
        int bytes = write(((struct sboy28_file *)stream)->fd, ptr, total);
        if (bytes < 0) {
            ((struct sboy28_file *)stream)->error = 1;
            return 0;
        }
        return (size_t)bytes / size;
    }
}

int fseek(FILE *stream, long offset, int whence) {
    (void)offset;
    (void)whence;
    if (stream == (FILE *)0) {
        return -1;
    }

    /* TODO: Hook fseek -> VFS seek syscall. */
    return -1;
}

long ftell(FILE *stream) {
    if (stream == (FILE *)0) {
        return -1;
    }

    /* TODO: Hook ftell -> VFS tell syscall. */
    return -1;
}

int fflush(FILE *stream) {
    (void)stream;
    /* TODO: Implement buffering layer (optional later). */
    return 0;
}

int getc(FILE *stream) {
    unsigned char byte = 0;
    if (stream == (FILE *)0) {
        return EOF;
    }

    if (((struct sboy28_file *)stream)->ungot_char != EOF) {
        int c = ((struct sboy28_file *)stream)->ungot_char;
        ((struct sboy28_file *)stream)->ungot_char = EOF;
        return c;
    }

    if (fread(&byte, 1, 1, stream) != 1) {
        return EOF;
    }

    return (int)byte;
}

int ungetc(int c, FILE *stream) {
    if (stream == (FILE *)0 || c == EOF) {
        return EOF;
    }

    ((struct sboy28_file *)stream)->ungot_char = (unsigned char)c;
    return c;
}

char *fgets(char *s, int size, FILE *stream) {
    int i;
    if (s == (char *)0 || stream == (FILE *)0 || size <= 0) {
        return (char *)0;
    }

    for (i = 0; i < size - 1; ++i) {
        int c = getc(stream);
        if (c == EOF) {
            break;
        }
        s[i] = (char)c;
        if (c == '\n') {
            ++i;
            break;
        }
    }

    if (i == 0) {
        return (char *)0;
    }

    s[i] = '\0';
    return s;
}

int fputs(const char *s, FILE *stream) {
    size_t len = 0;
    if (s == (const char *)0 || stream == (FILE *)0) {
        return EOF;
    }

    while (s[len] != '\0') {
        ++len;
    }

    return (fwrite(s, 1, len, stream) == len) ? 0 : EOF;
}

int feof(FILE *stream) {
    if (stream == (FILE *)0) {
        return 0;
    }
    return ((struct sboy28_file *)stream)->eof;
}

int ferror(FILE *stream) {
    if (stream == (FILE *)0) {
        return 1;
    }
    return ((struct sboy28_file *)stream)->error;
}

void clearerr(FILE *stream) {
    if (stream == (FILE *)0) {
        return;
    }

    ((struct sboy28_file *)stream)->eof = 0;
    ((struct sboy28_file *)stream)->error = 0;
}

int setvbuf(FILE *stream, char *buffer, int mode, size_t size) {
    (void)stream;
    (void)buffer;
    (void)mode;
    (void)size;
    /* TODO: Implement buffering layer (optional later). */
    return 0;
}

FILE *tmpfile(void) {
    /* TODO: Support device streams (console, pipes). */
    return (FILE *)0;
}

int vfprintf(FILE *stream, const char *format, __builtin_va_list args) {
    (void)stream;
    (void)format;
    (void)args;
    return -1;
}

int fprintf(FILE *stream, const char *format, ...) {
    __builtin_va_list args;
    int result;

    __builtin_va_start(args, format);
    result = vfprintf(stream, format, args);
    __builtin_va_end(args);

    return result;
}

int remove(const char *pathname) {
    (void)pathname;
    return -1;
}

int rename(const char *oldpath, const char *newpath) {
    (void)oldpath;
    (void)newpath;
    return -1;
}
