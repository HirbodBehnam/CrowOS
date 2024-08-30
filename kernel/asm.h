/**
 * Outputs a value to a port using the OUT instruction
 */
static inline void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("out dx, al" : : "a"(value), "d"(port));
}

/**
 * Inputs a value from a port using the IN instruciton
 */
static inline unsigned char inb(unsigned short port) {
    unsigned char al;
    __asm__ volatile ("in al, dx" : "=a"(al) : "d"(port));
    return al;
}