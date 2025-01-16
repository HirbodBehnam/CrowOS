#include "ulib.h"
#include "include/file.h"
#include "usyscalls.h"
#include <stdint.h>

// Default file descriptors used in Linux
int stdin = DEFAULT_STDIN, stdout = DEFAULT_STDOUT, stderr = DEFAULT_STDERR;

void _start(int argc, char *argv[]) {
  extern int main(int argc, char *argv[]);
  exit(main(argc, argv));
}

void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *pdest = (uint8_t *)dest;
  const uint8_t *psrc = (const uint8_t *)src;

  for (size_t i = 0; i < n; i++) {
    pdest[i] = psrc[i];
  }

  return dest;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;

  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)c;
  }

  return s;
}

void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *pdest = (uint8_t *)dest;
  const uint8_t *psrc = (const uint8_t *)src;

  if (src > dest) {
    for (size_t i = 0; i < n; i++) {
      pdest[i] = psrc[i];
    }
  } else if (src < dest) {
    for (size_t i = n; i > 0; i--) {
      pdest[i - 1] = psrc[i - 1];
    }
  }

  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

  return 0;
}

char *strcpy(char *s, const char *t) {
  char *os = s;
  while ((*s++ = *t++) != 0)
    ;
  return os;
}

int strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uint8_t)*p - (uint8_t)*q;
}

size_t strlen(const char *s) {
  size_t n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}

char *strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c)
      return (char *)s;
  return 0;
}

void puts(const char *s) {
  for (; *s; s++)
    write(stdout, s, 1);
  const char new_line = '\n';
  write(stdout, &new_line, 1);
}

char *gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    cc = read(stdin, &c, 1);
    if (cc < 1)
      break;
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

void putchar(char c) { write(stdout, &c, 1); }

int atoi(const char *s) {
  int n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}