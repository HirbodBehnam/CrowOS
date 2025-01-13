#include "common/lib.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/smp.h"
#include "device/nvme.h"
#include "device/pcie.h"
#include "device/pic.h"
#include "device/serial_port.h"
#include "fs/fs.h"
#include "limine.h"
#include "mem/mem.h"
#include "mem/vmm.h"
#include "userspace/proc.h"
#include "userspace/syscall.h"
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

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.
__attribute__((used,
               section(".requests_start_"
                       "marker"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".requests_end_marker"))) static volatile LIMINE_REQUESTS_END_MARKER;

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

  // Initialize the TSS
  tss_init_and_load();

  // Setup IOAPIC to get interrupts. PIC is disabled via Limine spec
  ioapic_init();
  serial_init_interrupt();
  kprintf("IO APIC initialized\n");

  // Setup NVMe
  nvme_init();
  kprintf("Initialized NVMe\n");

  // Setup the file system
  fs_init();
  kprintf("Initialized file system\n");

  // Setup the interrupt vector
  idt_init();
  kprintf("IDT initialized\n");

  // Create the first program
  scheduler_init();

  // On each core initialize the lapic
  lapic_init();

  // Load the IDT and enable interrupts on each core
  idt_load();
  // DO NOT ENABLE INTERRUPTS! The kernel is not preemptible

  // Initialize syscall on each core
  syscall_init();

  // Run the scheduler to schedule processes
  kprintf("Master Core Initiated\n");
  scheduler();
}
