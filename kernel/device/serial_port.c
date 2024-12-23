// Mostly from https://wiki.osdev.org/Serial_Ports
#include "serial_port.h"
#include "cpu/asm.h"
#include "cpu/traps.h"
#include "device/pic.h"

#define PORT 0x3f8 // COM1

/**
 * Init serial will initialize the serial port for output and input.
 * From xv6-x86.
 *
 * Returns 1 if no serial port is available.
 */
int serial_init(void) {
  // Turn off the FIFO
  outb(PORT + 2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(PORT + 3, 0b10000000);    // Enable DLAB
  outb(PORT + 0, 115200 / 9600); // Budrate low
  outb(PORT + 1, 0);             // Budrate high
  outb(PORT + 3, 0b00000011);    // Disable DLAB, use 8 bits of data
  outb(PORT + 4, 0);             // We do not care about the modem
  outb(PORT + 1, 0b00000001);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if (inb(PORT + 5) == 0xFF)
    return 1;

  return 0;
}

/**
 * Serial port initialize interrupts to receive them. This must be called
 * after IO APIC init.
 */
void serial_init_interrupt(void) {
  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(PORT + 2); // Interrupt Identification Register
  inb(PORT + 0); // Get data if anything is there
  ioapic_enable(IRQ_COM1,
                0); // Tell IO APIC to get COM1 interrupts on core zero
}

/**
 * Waits until the line is empty. This is done by polling the Line Status
 * Register and looking at the 5th bit which is "Transmitter holding register
 * empty (THRE)": "Set if the transmission buffer is empty (i.e. data can be
 * sent) "
 */
static int is_transmit_empty(void) { return inb(PORT + 5) & 0b00100000; }

/**
 * Writes a single char in the serial port
 */
void serial_putc(char a) {
  while (is_transmit_empty() == 0)
    ;

  outb(PORT, a);
}

/**
 * Checks if there is anything in the serial port to receive.
 * This will check the first bit of the Line Status Register which is
 * "Data ready (DR)" that is "Set if there is data that can be read"
 */
static int serial_received(void) { return inb(PORT + 5) & 0x01; }

/**
 * Reads one character from the serial port
 */
char serial_getc(void) {
  while (serial_received() == 0)
    ;

  return inb(PORT);
}

/**
 * Gets an input from the serial port and puts it back in the serial
 * port. It will also send end of interrupt to the local APIC.
 */
void serial_echo_back_char(void) {
  char c = serial_getc();
  serial_putc(c);
  lapic_send_eoi();
}
