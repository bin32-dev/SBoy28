#ifndef SBOY28_STDIO_H
#define SBOY28_STDIO_H

/* Minimal stdio interface for Lua/lauxlib and OS-native libc compatibility. */

typedef __SIZE_TYPE__ size_t;

typedef struct sboy28_file FILE;

#define EOF      (-1)
#define BUFSIZ   512

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define _IOFBF   0
#define _IOLBF   1
#define _IONBF   2

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fflush(FILE *stream);

int getc(FILE *stream);
int ungetc(int c, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);

int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

int setvbuf(FILE *stream, char *buffer, int mode, size_t size);
FILE *tmpfile(void);

int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, __builtin_va_list args);

int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

#endif
