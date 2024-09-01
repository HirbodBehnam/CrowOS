#include <stddef.h>
#include <stdint.h>

/**
 * Outputs a value to a port using the OUT instruction
 */
static inline void outb(uint16_t port, uint8_t value) {
  __asm__ volatile("out dx, al" : : "a"(value), "d"(port));
}

/**
 * Inputs a value from a port using the IN instruciton
 */
static inline uint8_t inb(uint16_t port) {
  uint8_t al;
  __asm__ volatile("in al, dx" : "=a"(al) : "d"(port));
  return al;
}

// Get MSR register content
static inline void msr_get(uint32_t msr, uint32_t *lo, uint32_t *hi) {
  __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

// Wait for next interrupt
static inline void wait_for_interrupt(void) { __asm__ volatile("hlt"); }

// Halt the processor forever
static inline void halt(void) {
  for (;;)
    __asm__ volatile("hlt");
}
