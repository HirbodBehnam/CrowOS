#include "pic.h"
#include "asm.h"
#include "mem.h"
#include "smp.h"
#include "traps.h"

#define IOAPIC 0xFEC00000 // Default physical address of IO APIC

#define REG_ID 0x00    // Register index: ID
#define REG_VER 0x01   // Register index: version
#define REG_TABLE 0x10 // Redirection table base

// The redirection table starts at REG_TABLE and uses
// two registers to configure each interrupt.
// The first (low) register in a pair contains configuration bits.
// The second (high) register contains a bitmask telling which
// CPUs can serve that interrupt.
#define INT_DISABLED 0x00010000  // Interrupt disabled
#define INT_LEVEL 0x00008000     // Level-triggered (vs edge-)
#define INT_ACTIVELOW 0x00002000 // Active low (vs high)
#define INT_LOGICAL 0x00000800   // Destination is CPU id (vs APIC ID)

/**
 * The IO APIC struct which points to the global address in RAM
 */
volatile struct ioapic *ioapic;

// IO APIC MMIO structure: write reg, then read or write data.
struct ioapic {
  uint32_t reg;
  uint32_t pad[3];
  uint32_t data;
};

/**
 * Read data from IO APIC
 */
static uint32_t ioapic_read(int reg) {
  ioapic->reg = reg;
  return ioapic->data;
}

/**
 * Write data to IO APIC
 */
static void ioapic_write(int reg, uint32_t data) {
  ioapic->reg = reg;
  ioapic->data = data;
}

/**
 * Initialize the IO APIC by getting it and masking every interrupt.
 */
void ioapic_init(void) {
  ioapic = (volatile struct ioapic *)P2V(IOAPIC);
  int maxintr = (ioapic_read(REG_VER) >> 16) & 0xFF;

  // Mark all interrupts edge-triggered, active high, disabled,
  // and not routed to any CPUs.
  for (int i = 0; i <= maxintr; i++) {
    ioapic_write(REG_TABLE + 2 * i, INT_DISABLED | (T_IRQ0 + i));
    ioapic_write(REG_TABLE + 2 * i + 1, 0);
  }
}

/**
 * Mark interrupt edge-triggered, active high, enabled, and routed
 * to the given cpunum which happens to be that cpu's APIC ID.
 */
void ioapic_enable(int irq, int cpunum) {
  ioapic_write(REG_TABLE + 2 * irq, T_IRQ0 + irq);
  ioapic_write(REG_TABLE + 2 * irq + 1, cpunum << 24);
}

// The local APIC address will be requested from the MSR
// registers.
#define IA32_APIC_BASE_MSR 0x1B

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID (0x0020 / 4)    // ID
#define VER (0x0030 / 4)   // Version
#define TPR (0x0080 / 4)   // Task Priority
#define EOI (0x00B0 / 4)   // EOI
#define SVR (0x00F0 / 4)   // Spurious Interrupt Vector
#define ENABLE 0x00000100  // Unit Enable
#define ESR (0x0280 / 4)   // Error Status
#define ICRLO (0x0300 / 4) // Interrupt Command
#define INIT 0x00000500    // INIT/RESET
#define STARTUP 0x00000600 // Startup IPI
#define DELIVS 0x00001000  // Delivery status
#define ASSERT 0x00004000  // Assert interrupt (vs deassert)
#define DEASSERT 0x00000000
#define LEVEL 0x00008000 // Level triggered
#define BCAST 0x00080000 // Send to all APICs, including self.
#define BUSY 0x00001000
#define FIXED 0x00000000
#define ICRHI (0x0310 / 4)  // Interrupt Command [63:32]
#define TIMER (0x0320 / 4)  // Local Vector Table 0 (TIMER)
#define X1 0x0000000B       // divide counts by 1
#define PERIODIC 0x00020000 // Periodic
#define PCINT (0x0340 / 4)  // Performance Counter LVT
#define LINT0 (0x0350 / 4)  // Local Vector Table 1 (LINT0)
#define LINT1 (0x0360 / 4)  // Local Vector Table 2 (LINT1)
#define ERROR (0x0370 / 4)  // Local Vector Table 3 (ERROR)
#define MASKED 0x00010000   // Interrupt masked
#define TICR (0x0380 / 4)   // Timer Initial Count
#define TCCR (0x0390 / 4)   // Timer Current Count
#define TDCR (0x03E0 / 4)   // Timer Divide Configuration

/**
 * The address of the local APIC for each CPU core.
 *
 * Also, it's very important to note that this must be uint32_t pointer
 * and writes must be 32 bits wide or else, this won't work.
 */
static volatile uint32_t *lapic[MAX_CORES];

/**
 * Write a value to local APIC register
 */
static void lapic_write(int index, int value) {
  uint32_t core_id = get_processor_id();
  lapic[core_id][index] = value;
  lapic[core_id][ID]; // wait for write to finish, by reading
}

/**
 * Get the address of memory location which the local APIC resides.
 */
static uintptr_t cpu_get_apic_base(void) {
  uint64_t msr = rdmsr(IA32_APIC_BASE_MSR);
  return msr & 0xfffff000;
}

/**
 * Initialize the local APIC by just getting the address of it
 * and storing it in a global variable.
 */
void lapic_init(void) {
  lapic[get_processor_id()] = (volatile uint32_t *)P2V(cpu_get_apic_base());
}

/**
 * Send an end of interrupt signal to local APIC
 */
void lapic_send_eoi(void) { lapic_write(EOI, 0); }