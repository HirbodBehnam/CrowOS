#include <stddef.h>
#include <stdarg.h>

extern int stdout, stdin, stderr;

void vfprintf(int fd, const char *fmt, va_list ap);
void fprintf(int fd, const char *fmt, ...);
void printf(const char *fmt, ...);
void vsnprintf(char *str, size_t size, const char *format, va_list ap);
void snprintf(char *str, size_t size, const char *format, ...);

char *gets(char *buf, int max);
void puts(const char *s);
void putchar(char c);

void hexdump(const char *buf, size_t size);