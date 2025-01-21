/**
 * Mostly from xv6-riscv:
 * https://github.com/mit-pdos/xv6-riscv/blob/de247db5e6384b138f270e0a7c745989b5a9c23b/kernel/printf.c#L26C1-L51C1
 */

#include "printf.h"
#include "ulib.h"
#include "usyscalls.h"
#include <stdarg.h>
#include <stdint.h>

static const char *digits = "0123456789abcdef";

static void print_char(int fd, char c) { write(fd, &c, sizeof(c)); }

static void print_int(int fd, long long xx, int base, int sign) {
  char buf[20];
  int i;
  unsigned long long x;

  if (sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    print_char(fd, buf[i]);
}

static void print_ptr(int fd, uint64_t x) {
  print_char(fd, '0');
  print_char(fd, 'x');
  for (size_t i = 0; i < (sizeof(uint64_t) * 2); i++, x <<= 4)
    print_char(fd, digits[x >> (sizeof(uint64_t) * 8 - 4)]);
}

// Print to a file descriptor
static void vprintf(int fd, const char *fmt, va_list ap) {
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
    if (c0 == 'd') {
      print_int(fd, va_arg(ap, int), 10, 1);
    } else if (c0 == 'l' && c1 == 'd') {
      print_int(fd, va_arg(ap, uint64_t), 10, 1);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      print_int(fd, va_arg(ap, uint64_t), 10, 1);
      i += 2;
    } else if (c0 == 'u') {
      print_int(fd, va_arg(ap, int), 10, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      print_int(fd, va_arg(ap, uint64_t), 10, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      print_int(fd, va_arg(ap, uint64_t), 10, 0);
      i += 2;
    } else if (c0 == 'x') {
      print_int(fd, va_arg(ap, int), 16, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      print_int(fd, va_arg(ap, uint64_t), 16, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      print_int(fd, va_arg(ap, uint64_t), 16, 0);
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
  vprintf(fd, fmt, ap);
}

void printf(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vprintf(stdout, fmt, ap);
}

void hexdump(const char *buf, size_t size) {
  for (size_t i = 0; i < size; i++) {
    uint8_t data = buf[i];
    putchar(digits[(data >> 4) & 0xF]);
    putchar(digits[data & 0xF]);
  }
  putchar('\n');
}
