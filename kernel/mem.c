#include "mem.h"
#include "lib.h"
#include "printf.h"

/**
 * Default page size of Intel CPUs
 */
#define PAGE_SIZE 4096

/**
 * The HHDM offset with current Limine works with
 */
static uint64_t hhdm_offset;

/**
 * Represents a free page which is linked to next page
 */
struct freepage_t {
  // Next page which is free or null if this is last page
  struct freepage_t *next;
};

/**
 * List of free pages
 */
static struct freepage_t *freepages = NULL;

/**
 * Initialize memory stuff. This means to first initialize each page and then
 * save the HHDM offset.
 */
void init_mem(uint64_t hhdm_offset_local,
              const struct limine_memmap_response *memory_map) {
  hhdm_offset = hhdm_offset_local;
  // Setup the free pages
  uint64_t total_free_pages = 0;
  for (uint64_t i = 0; i < memory_map->entry_count; i++) {
    const struct limine_memmap_entry *entry = memory_map->entries[i];
    if (entry->type == LIMINE_MEMMAP_USABLE) {
      // Sanity check
      if (entry->base % PAGE_SIZE != 0 || entry->length % PAGE_SIZE != 0)
        panic("init_mem align");
      // Free each page
      const uint64_t free_page_count = entry->length / PAGE_SIZE;
      uint64_t current_page = entry->base;
      for (uint64_t page_number = 0; page_number < free_page_count;
           page_number++, current_page += PAGE_SIZE) {
        kfree(p2v((void *)current_page));
        total_free_pages++;
      }
    }
  }
  // Log
  kprintf("Memory initialized with %lu free pages\n", total_free_pages);
}

/**
 * Convert physical memory to virtual address of kernel space based on HHDM
 * offset
 */
void *p2v(void *ptr) { return ptr + hhdm_offset; }
/**
 * Convert virtual memory to physical address of kernel space based on HHDM
 * offset
 */
void *v2p(void *ptr) { return ptr - hhdm_offset; }

/**
 * Free a memory got by kalloc
 */
void kfree(void *page) {
  // Some sanity checks
  const uint64_t physical_address = (uint64_t)v2p(page);
  if (physical_address % PAGE_SIZE != 0)
    panic("kfree");
  // Fill with junk to catch dangling refs.
  memset(page, 1, PAGE_SIZE);
  // In this page, we only store the reference to next page
  struct freepage_t *current_page = (struct freepage_t *)page;
  current_page->next = freepages;
  freepages = current_page;
}