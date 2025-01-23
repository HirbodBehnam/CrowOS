/**
 * Some of these functions are from
 * https://android.googlesource.com/platform/bionic/+/ics-mr0/libc/string and
 * the others are from https://github.com/mit-pdos/xv6-riscv/tree/riscv/user
 */

#include "string.h"
#include "ctype.h"
#include "stdlib.h"
#include <stdint.h>

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
  while ((*s++ = *t++) != 0) {
  }
  return os;
}

char *strncpy(char *dst, const char *src, size_t n) {
  if (n != 0) {
    char *d = dst;
    const char *s = src;
    do {
      if ((*d++ = *s++) == 0) {
        /* NUL pad the remaining n-1 bytes */
        while (--n != 0)
          *d++ = 0;
        break;
      }
    } while (--n != 0);
  }
  return (dst);
}

int strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uint8_t)*p - (uint8_t)*q;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  if (n == 0)
    return (0);
  do {
    if (*s1 != *s2++)
      return (*(unsigned char *)s1 - *(unsigned char *)--s2);
    if (*s1++ == 0)
      break;
  } while (--n != 0);
  return (0);
}

int strcasecmp(const char *p, const char *q) {
  while (*p && toupper(*p) == toupper(*q))
    p++, q++;
  return (uint8_t)*p - (uint8_t)*q;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
  if (n == 0)
    return (0);
  do {
    if (toupper(*s1) != toupper(*s2++))
      return (*(unsigned char *)s1 - *(unsigned char *)--s2);
    if (*s1++ == 0)
      break;
  } while (--n != 0);
  return (0);
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

char *strrchr(const char *p, char ch) {
  char *save;
  for (save = NULL;; ++p) {
    if (*p == ch)
      save = (char *)p;
    if (!*p)
      return (save);
  }
  /* NOTREACHED */
}

char *strstr(const char *s, const char *find) {
  char c, sc;
  size_t len;
  if ((c = *find++) != 0) {
    len = strlen(find);
    do {
      do {
        if ((sc = *s++) == 0)
          return (NULL);
      } while (sc != c);
    } while (strncmp(s, find, len) != 0);
    s--;
  }
  return ((char *)s);
}

char *strdup(const char *s) {
  size_t string_length = strlen(s);
  char *new_string = malloc(string_length + 1);
  if (new_string == NULL)
    return NULL;
  memcpy(new_string, s, string_length + 1);
  return new_string;
}
