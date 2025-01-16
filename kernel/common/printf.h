#include <stddef.h>

int kprintf(const char *fmt, ...);
void khexdump(const char *buf, size_t size);
void panic(const char *s) __attribute__ ((noreturn));