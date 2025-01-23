#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
char *strcpy(char *s, const char *t);
char *strncpy(char *s, const char *t, size_t n);
int strcmp(const char *p, const char *q);
int strncmp(const char *p, const char *q, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
char *strchr(const char *s, char c);
char *strrchr(const char *s, char c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);