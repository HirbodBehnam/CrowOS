#include "limine.h"
#include <stddef.h>
#include <stdint.h>

/**
 * The HHDM offset with current Limine works with. Defined in mem.c
 */
extern volatile uint64_t hhdm_offset;

/**
 * Default page size of Intel CPUs
 */
#define PAGE_SIZE 4096

/**
 * Convert virtual memory to physical address of kernel space based on HHDM
 * offset
 */
#define V2P(ptr) ((ptr) - (hhdm_offset))

/**
 * Convert physical memory to virtual address of kernel space based on HHDM
 * offset
 */
#define P2V(ptr) ((ptr) + (hhdm_offset))

void init_mem(uint64_t hhdm_offset,
              const struct limine_memmap_response *memory_map);
void kfree(void *page);
void *kalloc(void);