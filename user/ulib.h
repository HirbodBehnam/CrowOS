#include <stddef.h>

extern int serial_fd;

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *s, const char *t);
int strcmp(const char *p, const char *q);
size_t strlen(const char *s);
char *strchr(const char *s, char c);
char *gets(char *buf, int max);
void puts(const char *s);
int atoi(const char *s);