#include <stddef.h>
#include <stdarg.h>

extern int stdout, stdin, stderr;

void vprintf(int fd, const char *fmt, va_list ap);
void fprintf(int fd, const char *fmt, ...);
void printf(const char *fmt, ...);
char *gets(char *buf, int max);
void puts(const char *s);
void putchar(char c);
void hexdump(const char *buf, size_t size);