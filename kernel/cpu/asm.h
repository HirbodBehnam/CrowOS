#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FLAGS_CF (1UL << 0)    // Carry flag
#define FLAGS_PF (1UL << 2)    // Parity flag
#define FLAGS_AF (1UL << 4)    // Auxiliary Carry flag
#define FLAGS_ZF (1UL << 6)    // Zero flag
#define FLAGS_SF (1UL << 7)    // Sign flag
#define FLAGS_TF (1UL << 8)    // Trap flag
#define FLAGS_IF (1UL << 9)    // Interrupt enable flag
#define FLAGS_DF (1UL << 10)   // Direction flag
#define FLAGS_OF (1UL << 11)   // Overflow flag
#define FLAGS_IOPL (3UL << 12) // I/O privilege level
#define FLAGS_NT (1UL << 14)   // Nested task flag
#define FLAGS_RF (1UL << 16)   // Resume flag
#define FLAGS_VM (1UL << 17)   // Virtual 8086 mode flag
#define FLAGS_AC (1UL << 18)   // Alignment Check
#define FLAGS_VIF (1UL << 19)  // Virtual interrupt flag
#define FLAGS_VIP (1UL << 20)  // Virtual interrupt pending
#define FLAGS_ID (1UL << 21)   // Able to use CPUID instruction

#define MSR_FS_BASE        0xC0000100
#define MSR_GS_BASE        0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

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

/**
 * Outputs a value to a port using the OUT instruction
 */
static inline void outl(uint16_t port, uint32_t value) {
  __asm__ volatile("out dx, eax" : : "a"(value), "d"(port));
}

/**
 * Inputs a value from a port using the IN instruciton
 */
static inline uint32_t inl(uint16_t port) {
  uint32_t eax;
  __asm__ volatile("in eax, dx" : "=a"(eax) : "d"(port));
  return eax;
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
  asm volatile("pushf\n\t"
               "pop %0"
               : "=g"(flags));
  return flags;
}

/**
 * Clear Interrupt Flag
 */
static inline void cli(void) { asm volatile("cli"); }

/**
 * Set Interrupt Flag
 */
static inline void sti(void) { asm volatile("sti"); }

/**
 * Gets the current TSC value of the running processor
 */
static inline uint64_t get_tsc(void) {
  uint32_t timestamp_low, timestamp_high;
  asm volatile("rdtsc" : "=a"(timestamp_low), "=d"(timestamp_high));
  return ((uint64_t)timestamp_high << 32) | ((uint64_t)timestamp_low);
}

/**
 * Returns true if interrupts are enabled
 */
static inline bool is_interrupts_enabled(void) {
  return (read_rflags() & FLAGS_IF) != 0;
}