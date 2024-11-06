#include "gdt.h"
#include "mem.h"
#include "vmm.h"
#include <stdint.h>

/**
 * Most of this file is from these sources:
 * https://github.com/limine-bootloader/limine/blob/68558ec8259307174ccd462d8c92064ae679ab01/common/sys/gdt.s2.c
 * https://wiki.osdev.org/GDT_Tutorial#How_to_Set_Up_The_GDT
 * https://github.com/torvalds/linux/blob/b31c4492884252a8360f312a0ac2049349ddf603/arch/x86/include/asm/processor.h#L292C1-L311C27
 */

/**
 * The structure of GDT register
 */
struct gdtr {
  uint16_t limit;
  uint64_t ptr;
} __attribute__((packed));

/**
 * Each GDT entry is like this
 */
struct gdt_desc {
  uint16_t limit;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_hi;
} __attribute__((packed));

/**
 * Or each GDT entry can continue if it is a System Segment Descriptor.
 * Read more:
 * https://wiki.osdev.org/Global_Descriptor_Table#Long_Mode_System_Segment_Descriptor
 */
struct gdt_sys_desc_upper {
  uint32_t base_very_high;
  uint32_t reserved;
} __attribute__((packed));

/**
 * Each GDT entry can either be a normal entry or the continuation of upper
 * part of a system segment descriptor.
 */
union gdt_entry {
  struct gdt_desc normal;
  struct gdt_sys_desc_upper sys_desc_upper;
};

/**
 * The GDT entries needed for our OS
 */
static union gdt_entry gdt_entries[] = {
    // First segment is NULL
    {.normal = {0}},
    // 64-Bit Code Segement (Kernel)
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b10011011,
                .granularity = 0b00100000,
                .base_hi = 0x00}},
    // 64-Bit Data Segement (Kernel)
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b10010011,
                .granularity = 0b00000000,
                .base_hi = 0x00}},
    // 64-Bit Data Segement (User)
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b11110011,
                .granularity = 0b00000000,
                .base_hi = 0x00}},
    // 64-Bit Code Segement (User)
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b11111011,
                .granularity = 0b00100000,
                .base_hi = 0x00}},
    // TSS. Limit and base will be filled before this gets loaded
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b10001001,
                .granularity = 0b00000000,
                .base_hi = 0x00}},
    // TSS continued...
    {.sys_desc_upper = {
         .base_very_high = 0,
         .reserved = 0,
     }}};

/**
 * The TSS entry
 */
struct tss_entry {
  uint32_t reserved1;
  uint64_t sp0;
  /**
   * While I was reading the docs for Linux kernel, I realized that Linux
   * uses sp2 as a scratch register in syscalls. We can also do something
   * very similar with sp1 and sp2 because we do not use ring 1 nor 2.
   */
  uint64_t sp1;
  uint64_t sp2;
  uint64_t reserved2;
  uint64_t ist[7];
  uint32_t reserved3;
  uint32_t reserved4;
  uint16_t reserved5;
  uint16_t io_bitmap_base;
} __attribute__((packed));

/**
 * The TSS which will be loaded. At first, we initialize everything
 * with zero and then fill them in tss_init.
 */
static struct tss_entry tss = {0};

extern void reload_segments(void *gdt); // defined in snippet.S

/**
 * Setup the Task State Segment for this (and only) core and
 * put the address of it in GDT.
 *
 * This function must be called before gdt_init.
 */
void tss_init_and_load(void) {
  // Setup TSS itself
  tss.sp0 = INTSTACK_VIRTUAL_ADDRESS_TOP;
  tss.io_bitmap_base = 0xFFFF;
  // Create some IST entries
  tss.ist[IST_DOUBLE_FAULT_STACK_INDEX - 1] = (uint64_t) kalloc();
  // Load it
  __asm__ volatile("ltr ax" : : "a"(GDT_TSS_SEGMENT)); // load the task register
}

void gdt_init(void) {
  gdt_entries[GDT_TSS_SEGMENT / 8].normal.limit = sizeof(tss);
  const uint64_t tss_address = (uint64_t)&tss;
  gdt_entries[GDT_TSS_SEGMENT / 8].normal.base_low = tss_address & 0xFFFF;
  gdt_entries[GDT_TSS_SEGMENT / 8].normal.base_mid = (tss_address >> 16) & 0xFF;
  gdt_entries[GDT_TSS_SEGMENT / 8].normal.base_hi = (tss_address >> 24) & 0xFF;
  gdt_entries[GDT_TSS_SEGMENT / 8 + 1].sys_desc_upper.base_very_high =
      (tss_address >> 32) & 0xFFFFFFFF;
  struct gdtr gdt = {
      .limit = sizeof(gdt_entries) - 1,
      .ptr = (uint64_t)&gdt_entries[0],
  };
  reload_segments(&gdt);
}