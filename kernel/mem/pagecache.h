#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void pagecache_read(uint32_t block_index, char *data);
void pagecache_write(uint32_t block_index, const char *data);
void *pagecache_steal(void);
