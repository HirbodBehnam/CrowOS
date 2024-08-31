// Mostly from https://wiki.osdev.org/Serial_Ports
#include "serial_port.h"
#include "asm.h"
#include "traps.h"
#include "pic.h"

#define PORT 0x3f8 // COM1

/**
 * Init serial will initialize the serial port for output and input.
 * 
 * TODO: setup the serial port in a way that we can receive text and input.
 */
int init_serial(void) {
   char *p;

  // Turn off the FIFO
  outb(PORT+2, 0);

  // 9600 baud, 8 data bits, 1 stop bit, parity off.
  outb(PORT+3, 0x80);    // Unlock divisor
  outb(PORT+0, 115200/9600);
  outb(PORT+1, 0);
  outb(PORT+3, 0x03);    // Lock divisor, 8 data bits.
  outb(PORT+4, 0);
  outb(PORT+1, 0x01);    // Enable receive interrupts.

  // If status is 0xFF, no serial port.
  if(inb(PORT+5) == 0xFF)
    return 1;

  // Acknowledge pre-existing interrupt conditions;
  // enable interrupts.
  inb(PORT+2);
  inb(PORT+0);
  ioapicenable(IRQ_COM1, 0);

  return 0;
}

static int is_transmit_empty() {
   return inb(PORT + 5) & 0x20;
}

/**
 * Writes a single char in the serial port
 */
void write_serial(char a) {
   while (is_transmit_empty() == 0);

   outb(PORT,a);
}

/**
 * Writes a string in the serial port
 */
void print_string(const char *str) {
    for (; (*str) != '\0'; str++)
        write_serial(*str);
}

static int serial_received() {
   return inb(PORT + 5) & 1;
}

char read_serial() {
   while (serial_received() == 0);

   return inb(PORT);
}

void echo_back_char(void) {
   char c = read_serial();
   if (c >= 'a' && c <= 'z')
      c += 'A' - 'a';
   write_serial(c);
   send_eoi();
}