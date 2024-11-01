#include "asm.h"
#include "gdt.h"
#include "idt.h"
#include "lib.h"
#include "limine.h"
#include "mem.h"
#include "pic.h"
#include "printf.h"
#include "proc.h"
#include "serial_port.h"
#include "smp.h"
#include "syscall.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

__attribute__((used,
               section(".requests"))) static volatile LIMINE_BASE_REVISION(2);

// Get the mapping of the memory to allocate free space for programs
__attribute__((
    used, section(".requests"))) static volatile struct limine_memmap_request
    memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0,
};

// Have a 1:1 map of physical memory on a high address
__attribute__((used,
               section(".requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {
        .id = LIMINE_HHDM_REQUEST,
        .revision = 0,
};

// Get the physical address of kernel for trampoline page
__attribute__((
    used,
    section(".requests"))) static volatile struct limine_kernel_address_request
    kernel_address_request = {
        .id = LIMINE_KERNEL_ADDRESS_REQUEST,
        .revision = 0,
};

// Enable SMP
__attribute__((used,
               section(".requests"))) static volatile struct limine_smp_request
    smp_request = {
        .id = LIMINE_SMP_REQUEST,
        .revision = 0,
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.
__attribute__((used,
               section(".requests_start_"
                       "marker"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".requests_end_marker"))) static volatile LIMINE_REQUESTS_END_MARKER;

// Other CPU cores will start from here
void slave_cpu_start(struct limine_smp_info *core) {
  // Save the process ID
  wrmsr(IA32_TSC_AUX, core->processor_id);
  // Print a hello message
  kprintf("Hello from slave %u\n", get_processor_id());
  halt();
}

// The following will be our kernel's entry point.
void kmain(void) {
  // Ensure the bootloader actually understands our base revision (see spec).
  if (LIMINE_BASE_REVISION_SUPPORTED == false)
    halt();

  // Do not start cores if they are larger than supported number of cores
  wrmsr(IA32_TSC_AUX, 0); // this is core 0
  if (get_processor_id() >= MAX_CORES)
    halt();

  // Setup new GDT on every core at first
  gdt_init();

  // Initialize serial port
  if (serial_init() != 0)
    halt(); // well shit...
  kprintf("Serial port initialized\n");

  // Initialize memory
  init_mem(hhdm_request.response->offset, memmap_request.response);
  vmm_init_kernel(*kernel_address_request.response);
  kprintf("Kernel memory layout changed\n");

  // Setup IOAPIC to get interrupts. PIC is disabled via Limine spec
  ioapic_init();
  serial_init_interrupt();
  kprintf("IO APIC initialized\n");

  // Setup the interrupt vector
  idt_init();
  kprintf("IDT initialized\n");

  // Create the first program
  scheduler_init();

  // On each core initialize the lapic
  lapic_init();

  // Start other cores
  kprintf("Detected %d cores\n", smp_request.response->cpu_count);
  for (uint64_t i = 0; i < smp_request.response->cpu_count; i++) {
    if (smp_request.response->cpus[i]->processor_id == get_processor_id())
      continue;
    if (smp_request.response->cpus[i]->processor_id >= MAX_CORES)
      continue;
    smp_request.response->cpus[i]->goto_address = slave_cpu_start;
  }

  // Load the IDT and enable interrupts on each core
  idt_load();
  sti();
  
  // Initialize syscall on each core
  syscall_init();

  // Run the scheduler to schedule processes
  kprintf("Master Core Initiated\n");
  scheduler();
}
