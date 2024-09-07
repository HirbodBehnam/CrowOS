#include <stdint.h>

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
static const union gdt_entry gdt_entries[] = {
    // First segment is NULL
    {.normal = {0}},
    // 64-Bit Code Segement
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b10011011,
                .granularity = 0b00100000,
                .base_hi = 0x00}},
    // 64-Bit Data Segement
    {.normal = {.limit = 0x0000,
                .base_low = 0x0000,
                .base_mid = 0x00,
                .access = 0b10010011,
                .granularity = 0b00000000,
                .base_hi = 0x00}}};

extern void reload_segments(void *gdt); // defined in snippet.S

void gdt_init(void) {
  struct gdtr gdt = {
      .limit = sizeof(gdt_entries) - 1,
      .ptr = (uint64_t)&gdt_entries[0],
  };
  reload_segments(&gdt);
}