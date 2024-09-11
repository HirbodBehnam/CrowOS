#include "asm.h"
#include "gdt.h"
#include "idt.h"
#include "lib.h"
#include "limine.h"
#include "mem.h"
#include "pic.h"
#include "printf.h"
#include "serial_port.h"
#include "vmm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

__attribute__((used,
               section(".requests"))) static volatile LIMINE_BASE_REVISION(2);

// Requst framebuffer to draw stuff on screen
__attribute__((
    used,
    section(".requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST,
        .revision = 0,
};

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

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.
__attribute__((used,
               section(".requests_start_"
                       "marker"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".requests_end_marker"))) static volatile LIMINE_REQUESTS_END_MARKER;

void test_ring3(void);

// The following will be our kernel's entry point.
void kmain(void) {
  // Ensure the bootloader actually understands our base revision (see spec).
  if (LIMINE_BASE_REVISION_SUPPORTED == false)
    halt();

  // Draw a line from Limine example
  struct limine_framebuffer *framebuffer =
      framebuffer_request.response->framebuffers[0];
  for (size_t i = 0; i < 100; i++) {
    volatile uint32_t *fb_ptr = framebuffer->address;
    fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
  }

  // Setup new GDT
  gdt_init();

  // Initialize serial port
  if (serial_init() != 0)
    halt(); // well shit...
  kprintf("Serial port initialized\n");

  // Initialize memory
  init_mem(hhdm_request.response->offset, memmap_request.response);

  // Setup PIC to get interrupts. PIC is disabled via Limine spec
  lapic_init();
  ioapic_init();
  serial_init_interrupt();
  kprintf("APIC initialized\n");

  // Setup the interrupt vector
  setup_idt();
  kprintf("IDT initialized\n");

  // Userspace?
  test_ring3();
  kprintf("Ring 3 exited\n");

  // We're done, just hang...
  halt();
}

extern void jump_to_ring3(uint64_t program_start, uint64_t stack_virtual_address);
extern void ring3_halt();

void test_ring3(void) {
  // Create a page table
  const uint64_t program_start_address = 0x1000000, stack_address = 0x2000000;
  void *stack = kalloc(), *code = kalloc();
  pagetable_t pagetable = vmm_create_pagetable();
  vmm_map_pages(pagetable, program_start_address, PAGE_SIZE, V2P(code), (pte_permissions){.writable = 0, .executable = 1, .userspace = 1});
  vmm_map_pages(pagetable, stack_address, PAGE_SIZE, V2P(stack), (pte_permissions){.writable = 1, .executable = 0, .userspace = 1});
  // Fill code segment
  memcpy(code, ring3_halt, PAGE_SIZE);
  // TODO: jump tp trampoline to switch to userspace
}