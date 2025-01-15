#include <stddef.h>

int serial_init(void);
void serial_init_interrupt(void);
void serial_putc(char a);
void serial_received_char(void);
int serial_write(const char *buffer, size_t len);
int serial_read(char *buffer, size_t len);