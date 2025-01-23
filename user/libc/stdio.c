/**
 * Mostly from xv6-riscv:
 * https://github.com/mit-pdos/xv6-riscv/blob/de247db5e6384b138f270e0a7c745989b5a9c23b/kernel/printf.c#L26C1-L51C1
 */

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "include/file.h"
#include "usyscalls.h"
#include <stdint.h>

// Default file descriptors used in Linux
int stdin = DEFAULT_STDIN, stdout = DEFAULT_STDOUT, stderr = DEFAULT_STDERR;

static const char *digits = "0123456789abcdef";

static void print_char(int fd, char c) { write(fd, &c, sizeof(c)); }

static void print_int(int fd, long long xx, int base, int sign, int padding) {
  char buf[20];
  int i;
  unsigned long long x;

  memset(buf, '0', sizeof(buf));

  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  i = i > padding ? i : padding;

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    print_char(fd, buf[i]);
}

static void sprint_int(char **str, const char *str_end, long long xx, int base,
                       int sign, int padding) {
  char buf[20];
  int i;
  unsigned long long x;

  memset(buf, '0', sizeof(buf));

  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  i = i > padding ? i : padding;

  if (sign)
    buf[i++] = '-';

  while (--i >= 0 && *str != str_end) {
    **str = buf[i];
    (*str)++;
  }
}

static void print_ptr(int fd, uint64_t x) {
  print_char(fd, '0');
  print_char(fd, 'x');
  for (size_t i = 0; i < (sizeof(uint64_t) * 2); i++, x <<= 4)
    print_char(fd, digits[x >> (sizeof(uint64_t) * 8 - 4)]);
}

static void sprint_ptr(char **str, const char *str_end, uint64_t x) {
  // Print 0
  if (*str == str_end) // the fuck?
    return;
  **str = '0';
  (*str)++;
  // Print x
  if (*str == str_end)
    return;
  **str = 'x';
  (*str)++;
  // Print the rest in hex
  for (size_t i = 0; i < (sizeof(uint64_t) * 2) && *str != str_end;
       i++, x <<= 4) {
    **str = digits[x >> (sizeof(uint64_t) * 8 - 4)];
    (*str)++;
  }
}

// Print to a file descriptor
void vfprintf(int fd, const char *fmt, va_list ap) {
  int i, cx, c0, c1, c2;
  char *s;

  for (i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      print_char(fd, cx);
      continue;
    }
    i++;
    c0 = fmt[i + 0] & 0xff;
    c1 = c2 = 0;
    if (c0)
      c1 = fmt[i + 1] & 0xff;
    if (c1)
      c2 = fmt[i + 2] & 0xff;
    if (c0 == 'd' || c0 == 'i') {
      print_int(fd, va_arg(ap, int), 10, 1, 0);
    } else if (c0 == '.' && c2 == 'd') {
      // For now, we only support single digit paddings
      int padding = c1 - '0';
      print_int(fd, va_arg(ap, int), 10, 1, padding);
      i += 2;
    } else if (c0 == 'l' && c1 == 'd') {
      print_int(fd, va_arg(ap, uint64_t), 10, 1, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      print_int(fd, va_arg(ap, uint64_t), 10, 1, 0);
      i += 2;
    } else if (c0 == 'u') {
      print_int(fd, va_arg(ap, int), 10, 0, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      print_int(fd, va_arg(ap, uint64_t), 10, 0, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      print_int(fd, va_arg(ap, uint64_t), 10, 0, 0);
      i += 2;
    } else if (c0 == 'x') {
      print_int(fd, va_arg(ap, int), 16, 0, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      print_int(fd, va_arg(ap, uint64_t), 16, 0, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      print_int(fd, va_arg(ap, uint64_t), 16, 0, 0);
      i += 2;
    } else if (c0 == 'p') {
      print_ptr(fd, va_arg(ap, uint64_t));
    } else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; s++)
        print_char(fd, *s);
    } else if (c0 == '%') {
      print_char(fd, '%');
    } else if (c0 == 0) {
      break;
    } else {
      // Print unknown % sequence to draw attention.
      print_char(fd, '%');
      print_char(fd, c0);
    }
  }
  va_end(ap);
}

void fprintf(int fd, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(fd, fmt, ap);
}

void printf(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
}

// Print to a string buffer
void vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
  int i, cx, c0, c1, c2;
  char *s;
  char *string_end = str + size - 1; // -1 for null terminator

  for (i = 0; (cx = fmt[i] & 0xff) != 0 && str <= string_end; i++) {
    if (cx != '%') {
      *str = cx;
      str++;
      continue;
    }
    i++;
    c0 = fmt[i + 0] & 0xff;
    c1 = c2 = 0;
    if (c0)
      c1 = fmt[i + 1] & 0xff;
    if (c1)
      c2 = fmt[i + 2] & 0xff;
    if (c0 == 'd' || c0 == 'i') {
      sprint_int(&str, string_end, va_arg(ap, int), 10, 1, 0);
    } else if (c0 == '.' && c2 == 'd') {
      // For now, we only support single digit paddings
      int padding = c1 - '0';
      sprint_int(&str, string_end, va_arg(ap, int), 10, 1, padding);
      i += 2;
    } else if (c0 == 'l' && c1 == 'd') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 10, 1, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 10, 1, 0);
      i += 2;
    } else if (c0 == 'u') {
      sprint_int(&str, string_end, va_arg(ap, int), 10, 0, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 10, 0, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 10, 0, 0);
      i += 2;
    } else if (c0 == 'x') {
      sprint_int(&str, string_end, va_arg(ap, int), 16, 0, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 16, 0, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      sprint_int(&str, string_end, va_arg(ap, uint64_t), 16, 0, 0);
      i += 2;
    } else if (c0 == 'p') {
      sprint_ptr(&str, string_end, va_arg(ap, uint64_t));
    } else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s && str <= string_end; s++) {
        *str = *s;
        str++;
      }
    } else if (c0 == '%') {
      *str = '%';
      str++;
    } else if (c0 == 0) {
      break;
    } else {
      // Print unknown % sequence to draw attention.
      *str = '%';
      str++;
      // Note: At last, this is going to be overwritten by the null terminator
      // at the end of this function
      *str = c0;
      str++;
    }
  }
  va_end(ap);

  *str = '\0';
}

void snprintf(char *str, size_t size, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(str, size, fmt, ap);
}

void puts(const char *s) {
  for (; *s; s++)
    write(stdout, s, 1);
  const char new_line = '\n';
  write(stdout, &new_line, 1);
}

char *gets(char *buf, int max) {
  char c;
  int i;

  for (i = 0; i + 1 < max;) {
    int cc = read(stdin, &c, 1);
    if (cc < 1) // end of stream
      break;
    // Check for backspace
    if (c == 127) { // DEL ASCII
      if (i == 0)   // Do not allow backspace at the first of buffer
        continue;
      // Remove from buffer
      i--;
      // Remove from console
      write(stdout, "\b \b", 3);
      continue;
    }
    // Write to buffer
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

void putchar(char c) { write(stdout, &c, 1); }

void hexdump(const char *buf, size_t size) {
  for (size_t i = 0; i < size; i++) {
    uint8_t data = buf[i];
    putchar(digits[(data >> 4) & 0xF]);
    putchar(digits[data & 0xF]);
  }
  putchar('\n');
}
