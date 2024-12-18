#include "idt.h"
#include "gdt.h"
#include "traps.h"

typedef struct {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t	    ist;          // The IST in the TSS that the CPU will load into RSP; set to zero for now
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_mid;      // The higher 16 bits of the lower 32 bits of the ISR's address
	uint32_t    isr_high;     // The higher 32 bits of the ISR's address
	uint32_t    reserved;     // Set to zero
} __attribute__((packed)) idt_entry_t;

// Number of descriptors in the IDT
#define IDT_MAX_DESCRIPTORS 256
// The entries for each interrupt vector
__attribute__((aligned(0x10))) 
static idt_entry_t idt[IDT_MAX_DESCRIPTORS];

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idtr_t;

static idtr_t idtr;

// Main interrupt handler function
extern void (*const irq_vec[IDT_MAX_DESCRIPTORS]) (void);

/**
 * Sets the IDT entry in the IDT table
 */
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags, uint8_t ist) {
    idt_entry_t* descriptor = &idt[vector];

    descriptor->isr_low        = (uint64_t)isr & 0xFFFF;
    descriptor->kernel_cs      = GDT_KERNEL_CODE_SEGMENT;
    descriptor->ist            = ist;
    descriptor->attributes     = flags;
    descriptor->isr_mid        = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isr_high       = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved       = 0;
}

/**
 * Setup IDT create the IDT table with the addresses of interrupt functions
 */
void idt_init(void) {
    idtr.base = (uintptr_t)&idt[0];
    idtr.limit = (uint16_t)sizeof(idt_entry_t) * IDT_MAX_DESCRIPTORS - 1;

    for (size_t irq = 0; irq < IDT_MAX_DESCRIPTORS; irq++)
        idt_set_descriptor(irq,
        (void *)irq_vec[irq],
        irq == T_YEILD ? 0xEE : 0x8E,
        irq == T_DBLFLT ? IST_DOUBLE_FAULT_STACK_INDEX : 0);
}

/**
 * Loads the IDT register from the idtr value
 */
void idt_load(void) {
    __asm__ volatile ("lidt %0" : : "m"(idtr)); // load the new IDT
}