#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "asm.h"
#include "pic.h"
#include "idt.h"
#include "serial_port.h"

// We use base revision 0 to have the lower 4 GB of memory mapped 1:1
// in the virtual page table
__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(0);

// Requst framebuffer to draw stuff on screen
__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

// Get the mapping of the memory to allocate free space for programs
__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.
__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
        halt();

    // Draw a line from Limine example
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    for (size_t i = 0; i < 100; i++) {
        volatile uint32_t *fb_ptr = framebuffer->address;
        fb_ptr[i * (framebuffer->pitch / 4) + i] = 0xffffff;
    }

    // Initialize serial port
    if (serial_init() != 0)
        halt(); // well shit...
    serial_puts("Serial port initialized\n");

    // Setup PIC to get interrupts
    pic_disable();
    lapic_init();
    ioapic_init();
    serial_init_interrupt();
    serial_puts("APIC initialized\n");

    // Setup the interrupt vector 
    setup_idt();
    serial_puts("IDT initialized\n");
    
    // We're done, just hang...
    halt();
}
