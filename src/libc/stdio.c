#include <stdio.h>

struct sboy28_file {
    void *backend_handle;
    unsigned int flags;
    int eof;
    int error;
    int ungot_char;
};

#define FILE_FLAG_STATIC_STREAM (1u << 0)

static struct sboy28_file g_stdin  = {0, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};
static struct sboy28_file g_stdout = {0, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};
static struct sboy28_file g_stderr = {0, FILE_FLAG_STATIC_STREAM, 0, 0, EOF};

FILE *stdin = &g_stdin;
FILE *stdout = &g_stdout;
FILE *stderr = &g_stderr;

FILE *fopen(const char *filename, const char *mode) {
    (void)filename;
    (void)mode;
    /* TODO: Hook fopen -> VFS open syscall. */
    return (FILE *)0;
}

FILE *freopen(const char *filename, const char *mode, FILE *stream) {
    (void)stream;
    /* TODO: Hook fopen -> VFS open syscall. */
    return fopen(filename, mode);
}

int fclose(FILE *stream) {
    if (stream == (FILE *)0) {
        return -1;
    }

    if (((struct sboy28_file *)stream)->flags & FILE_FLAG_STATIC_STREAM) {
        return 0;
    }

    /* TODO: Hook fclose -> VFS close. */
    return -1;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    if (stream == (FILE *)0) {
        return 0;
    }

    ((struct sboy28_file *)stream)->eof = 1;
    /* TODO: Hook fread -> kernel file I/O. */
    return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    if (stream == (FILE *)0) {
        return 0;
    }

    /* TODO: Hook fwrite -> kernel file I/O. */
    return 0;
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
