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
__attribute__ ((noreturn)) static inline void halt(void) {
  for (;;)
    __asm__ volatile("hlt");
}

/**
 * Install a new pagetable by changing the CR3 value. Address must be divisable
 * by pagesize. The address must be physical.
 */
static inline void install_pagetable(uint64_t pagetable_address) {
  __asm__ volatile("mov cr3, rax" : : "a"(pagetable_address & 0xFFFFFFFFFFFFF000ULL));
}

/**
 * Gets the physical address of installed current page table.
 */
static inline uint64_t get_installed_pagetable() {
  uint64_t cr3;
  __asm__ volatile("mov rax, cr3" : "=a"(cr3));
  return cr3 & 0xFFFFFFFFFFFFF000ULL;
}