#define GDT_CODE_SEGMENT 0x8
#define GDT_DATA_SEGMENT 0x10

#ifndef __ASSEMBLER__
/**
 * Setups the GDT based on the needs of our operating system
 */
void gdt_init(void);
#endif