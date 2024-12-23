/**
 * Mostly from xv6-riscv:
 * https://github.com/mit-pdos/xv6-riscv/blob/de247db5e6384b138f270e0a7c745989b5a9c23b/kernel/printf.c#L26C1-L51C1
 */

#include "printf.h"
#include "cpu/asm.h"
#include "device/serial_port.h"
#include "spinlock.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

static struct spinlock print_lock;

static const char *digits = "0123456789abcdef";

static void printint(long long xx, int base, int sign) {
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
    serial_putc(buf[i]);
}
static void printptr(uint64_t x) {
  serial_putc('0');
  serial_putc('x');
  for (size_t i = 0; i < (sizeof(uint64_t) * 2); i++, x <<= 4)
    serial_putc(digits[x >> (sizeof(uint64_t) * 8 - 4)]);
}

// Print to the console.
int kprintf(const char *fmt, ...) {
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  spinlock_lock(&print_lock);

  va_start(ap, fmt);
  for (i = 0; (cx = fmt[i] & 0xff) != 0; i++) {
    if (cx != '%') {
      serial_putc(cx);
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
      printint(va_arg(ap, int), 10, 1);
    } else if (c0 == 'l' && c1 == 'd') {
      printint(va_arg(ap, uint64_t), 10, 1);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'd') {
      printint(va_arg(ap, uint64_t), 10, 1);
      i += 2;
    } else if (c0 == 'u') {
      printint(va_arg(ap, int), 10, 0);
    } else if (c0 == 'l' && c1 == 'u') {
      printint(va_arg(ap, uint64_t), 10, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'u') {
      printint(va_arg(ap, uint64_t), 10, 0);
      i += 2;
    } else if (c0 == 'x') {
      printint(va_arg(ap, int), 16, 0);
    } else if (c0 == 'l' && c1 == 'x') {
      printint(va_arg(ap, uint64_t), 16, 0);
      i += 1;
    } else if (c0 == 'l' && c1 == 'l' && c2 == 'x') {
      printint(va_arg(ap, uint64_t), 16, 0);
      i += 2;
    } else if (c0 == 'p') {
      printptr(va_arg(ap, uint64_t));
    } else if (c0 == 's') {
      if ((s = va_arg(ap, char *)) == 0)
        s = "(null)";
      for (; *s; s++)
        serial_putc(*s);
    } else if (c0 == '%') {
      serial_putc('%');
    } else if (c0 == 0) {
      break;
    } else {
      // Print unknown % sequence to draw attention.
      serial_putc('%');
      serial_putc(c0);
    }
  }
  va_end(ap);

  spinlock_unlock(&print_lock);
  return 0;
}

void khexdump(const char *buf, size_t size) {
  for (size_t i = 0; i < size; i++) {
    uint8_t data = buf[i];
    serial_putc(digits[(data >> 4) & 0xF]);
    serial_putc(digits[data & 0xF]);
  }
  serial_putc('\n');
}

void panic(const char *s) {
  cli();
  kprintf("panic: %s\n", s);
  for (;;)
    ;
}
