#include <stdint.h>
#include <stddef.h>

/**
 * Outputs a value to a port using the OUT instruction
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("out dx, al" : : "a"(value), "d"(port));
}

/**
 * Inputs a value from a port using the IN instruciton
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t al;
    __asm__ volatile ("in al, dx" : "=a"(al) : "d"(port));
    return al;
}

static inline void msr_get(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
   asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}