#include <stddef.h>
#include <stdint.h>

// Interrupt enable flag
#define FLAGS_IF (1 << 9)

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
__attribute__((noreturn)) static inline void halt(void) {
  for (;;)
    __asm__ volatile("hlt");
}

/**
 * Install a new pagetable by changing the CR3 value. Address must be divisable
 * by pagesize. The address must be physical.
 */
static inline void install_pagetable(uint64_t pagetable_address) {
  __asm__ volatile("mov cr3, rax"
                   :
                   : "a"(pagetable_address & 0xFFFFFFFFFFFFF000ULL));
}

/**
 * Gets the physical address of installed current page table.
 */
static inline uint64_t get_installed_pagetable() {
  uint64_t cr3;
  __asm__ volatile("mov rax, cr3" : "=a"(cr3));
  return cr3 & 0xFFFFFFFFFFFFF000ULL;
}

/**
 * Reads the module specific register
 */
static inline uint64_t rdmsr(uint32_t msr) {
  uint32_t lo, hi;
  asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return (uint64_t)lo | ((uint64_t)hi << 32);
}

/**
 * Writes to module specific register
 */
static inline void wrmsr(uint32_t msr, uint64_t value) {
  const uint32_t lo = value & 0xFFFFFFFF, hi = value >> 32;
  asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

/**
 * Reads the flag register
 */
static inline uint64_t read_rflags(void) {
  uint64_t flags;
  asm volatile("pushf\n"
               "pop %0"
               : "=g"(flags));
  return flags;
}

/**
 * Clear Interrupt Flag
 */
static inline void cli(void) {
  asm volatile("cli");
}

/**
 * Set Interrupt Flag
 */
static inline void sti(void) {
  asm volatile("sti");
}

/**
 * Gets the processor ID of the running processor
 */
static inline uint32_t get_processor_id(void) {
  uint32_t timestamp_low, timestamp_high, processor_id;
  asm volatile("rdtscp" : "=a"(timestamp_low), "=d"(timestamp_high), "=c"(processor_id));
  return processor_id;
}
