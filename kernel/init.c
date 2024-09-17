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
  vmm_init_kernel(*kernel_address_request.response);
  kprintf("Kernel memory layout changed\n");

  // Setup PIC to get interrupts. PIC is disabled via Limine spec
  lapic_init();
  ioapic_init();
  serial_init_interrupt();
  kprintf("APIC initialized\n");

  // Setup the interrupt vector
  idt_init();
  kprintf("IDT initialized\n");

  // Userspace?
  kprintf("Entring ring 3\n");
  test_ring3();
  kprintf("Ring 3 exited\n");

  // We're done, just hang...
  halt();
}

extern void jump_to_ring3(uint64_t program_start,
                          uint64_t stack_virtual_address);
extern void ring3_halt();

void test_ring3(void) {
  // Create a page table and install it
  pagetable_t pagetable = vmm_create_user_pagetable((void *)ring3_halt);
  install_pagetable(V2P(pagetable));
  // Jump to the trampoline to do userspace stuff
  jump_to_ring3(USER_CODE_START, USER_STACK_TOP & 0xFFFFFFFFFFFFFFF0);
}