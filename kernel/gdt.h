#define GDT_KERNEL_CODE_SEGMENT 0x8
#define GDT_KERNEL_DATA_SEGMENT 0x10
#define GDT_USER_CODE_SEGMENT 0x18
#define GDT_USER_DATA_SEGMENT 0x20
#define GDT_TSS_SEGMENT 0x28

#ifndef __ASSEMBLER__
/**
 * Setups the GDT based on the needs of our operating system
 */
void gdt_init(void);
#endif