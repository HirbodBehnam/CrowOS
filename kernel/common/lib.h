#include <stddef.h>
#include <stdint.h>

// Max/Min from https://stackoverflow.com/a/3437484/4213397
#define MAX_SAFE(a, b)                                                         \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })
#define MIN_SAFE(a, b)                                                         \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *s, const char *t);
int strcmp(const char *p, const char *q);
size_t strlen(const char *s);