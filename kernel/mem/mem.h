#pragma once
#ifndef __ASSEMBLER__
#include "limine.h"
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * Default page size of Intel CPUs
 */
#define PAGE_SIZE 4096

/**
 * Rounds a size to the nearest page size
 */
#define PAGE_ROUND_UP(sz) (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))

#ifndef __ASSEMBLER__
/**
 * The HHDM offset with current Limine works with. Defined in mem.c
 */
extern volatile uint64_t hhdm_offset;

/**
 * Convert virtual memory to physical address of kernel space based on HHDM
 * offset
 */
#define V2P(ptr) ((uint64_t)(ptr) - (hhdm_offset))

/**
 * Convert physical memory to virtual address of kernel space based on HHDM
 * offset
 */
#define P2V(ptr) ((uint64_t)(ptr) + (hhdm_offset))

void init_mem(uint64_t hhdm_offset,
              const struct limine_memmap_response *memory_map);
void kfree(void *page);
void *kalloc(void);
void *kalloc_for_page_cache(void);
void *kcalloc(void);
#endif