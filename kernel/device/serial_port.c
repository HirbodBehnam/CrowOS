// Mostly from https://wiki.osdev.org/Serial_Ports
#include "serial_port.h"
#include "common/condvar.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "cpu/traps.h"
#include "device/pic.h"

#define PORT 0x3f8               // COM1
#define SERIAL_BUFFER_LENGTH 128 // Length of the serial input ring buffer

// A ring buffer for the unread input chars
static char serial_input_buffer[SERIAL_BUFFER_LENGTH];
// The head and the tail of the serial input buffer
static uint8_t serial_input_buffer_read_index = 0,
               serial_input_buffer_write_index = 0;
static struct condvar serial_input_buffer_condvar;

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
 * Must be called when the read interrupt of the serial port is received.
 *
 * Will try to put the read char inside the buffer however, there is possibility
 * that the buffer is full. In this case, the value read will be discarded.
 * It also echos back the char read to the user.
 */
void serial_received_char(void) {
  char c = serial_getc();
  // For some reason, the serial port sends \r for new lines
  if (c == '\r')
    c = '\n';
  // Write to buffer
  condvar_lock(&serial_input_buffer_condvar);
  if ((serial_input_buffer_write_index + 1) % SERIAL_BUFFER_LENGTH !=
      serial_input_buffer_read_index) {
    // We have space in the buffer!
    serial_input_buffer[serial_input_buffer_write_index] = c;
    serial_input_buffer_write_index =
        (serial_input_buffer_write_index + 1) % SERIAL_BUFFER_LENGTH;
    serial_putc(c);
    // We shall use notify all. Because two programs might be
    // waiting for input while the read buffer of the program
    // is smaller than the bytes we just read.
    condvar_notify_all(&serial_input_buffer_condvar);
  }
  condvar_unlock(&serial_input_buffer_condvar);
  lapic_send_eoi();
}

/**
 * Writes a string into the serial port
 */
int serial_write(const char *buffer, size_t len) {
  for (size_t i = 0; i < len; i++)
    serial_putc(buffer[i]);
  return (int)len;
}

/**
 * Blocks and waits until some bytes can be read from the serial port
 * and returns the result.
 */
int serial_read(char *buffer, size_t len) {
  // But why would anyone do this?
  if (len == 0)
    return 0;
  // Lock the input buffer
  condvar_lock(&serial_input_buffer_condvar);
  while (serial_input_buffer_read_index == serial_input_buffer_write_index)
    condvar_wait(&serial_input_buffer_condvar); // empty buffer so wait
  size_t available_bytes;
  if (serial_input_buffer_write_index <
      serial_input_buffer_read_index) { // ring overflow
    available_bytes = serial_input_buffer_write_index +
                      (SERIAL_BUFFER_LENGTH - serial_input_buffer_read_index);
  } else {
    available_bytes =
        serial_input_buffer_write_index - serial_input_buffer_read_index;
  }
  size_t to_read_bytes = len > available_bytes ? available_bytes : len;
  // Now read bytes one by one
  for (size_t i = 0; i < to_read_bytes; i++) {
    buffer[i] = serial_input_buffer[serial_input_buffer_read_index];
    serial_input_buffer_read_index =
        (serial_input_buffer_read_index + 1) % SERIAL_BUFFER_LENGTH;
  }
  // We are done!
  condvar_unlock(&serial_input_buffer_condvar);
  return to_read_bytes;
}