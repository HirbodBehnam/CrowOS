int serial_init(void);
void serial_init_interrupt(void);
void serial_putc(char a);
void serial_puts(const char *str);
char serial_getc();
void serial_echo_back_char(void);