#include "limine.h"
#include <stddef.h>
#include <stdint.h>

void init_mem(uint64_t hhdm_offset,
              const struct limine_memmap_response *memory_map);
void *p2v(void *ptr);
void *v2p(void *ptr);
void kfree(void *page);