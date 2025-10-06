#include "pagecache.h"
#include "common/spinlock.h"
#include "mem.h"

/**
 * The page cache. It sits between the file system and the disk.
 *
 * Like a traditional OS, it uses the free pages in the memory in order to
 * handle the cache entries.
 *
 * It provides a transparent interface which can be used instead of accessing
 * the disk itself.
 *
 * Internally, it uses Clock algorithm to evict pages. It keeps a circular
 * buffer of pagecache_entries. The memory manager might call pagecache_steal at
 * any time in order to repurpose one of page cache pages into a normal memory
 * page (if the system is under the pressure). We should have a list of
 * pagecache_entries somewhere; i use each physical frame in order to handle a
 * list of entries. Each frame contains a lot of pagecache_entries and also a
 * pointer to the next frame.
 *
 * As the page cache grows, the pagecache_entries must also grow. To this
 * extend, we must get a page from the memory manager (kalloc) which might steal
 * a page back from the page cache itself! The problem here, however, is that we
 * will deadlock because the kalloc calls back to us. To this extend, a new
 * method which is called kalloc_no_cache is added which simply returns NULL if
 * the free page memory manager does not have a free page. This is also used in
 * page cache allocation itself. After that, we can safely release a page which
 * is occupied by cache itself and repurpose that. Note that there might be an
 * edge case that involves system running out of the memory. In that case, we
 * simply do not cache and pass through the data in the disk.
 *
 * Dirty pages are not handled perfectly here. In modern operating systems, you
 * "age" the dirty pages and do not allow the dirty pages to fill up more than a
 * portion of your page cache. In this OS, however, we don't give a shit.
 * Bookkeeping dirty pages is WAY above my pay grade and lets just write them
 * back when we want to evict a page.
 *
 * Note to myself: The NVMe driver sucks ass and it's fucking blocking :)))))
 * Thus, no need to do sleep lock I guess?
 */
struct pagecache_entry {
  // A pointer to the cache page data
  void *cache;
  // Which block of disk is this cache for
  uint32_t disk_block;
  // Is this cache dirty?
  bool dirty;
  // True if this entry is valid, otherwise false
  bool valid;
  // Clock algorithm shenanigans
  bool second_chance;
};

// Number of pages which can go into pagecache_entries
#define PAGECACHE_ENTRY_COUNT 255

// A frame which contains the list of page cache entries
struct pagecache_entries {
  // List of page cache's metadata
  struct pagecache_entry entries[PAGECACHE_ENTRY_COUNT];
  // A point to the frame which contains the next page cache
  // entries or NULL if this is the last entry.
  struct pagecache_entries *next_entries;
};

// Sanity check
_Static_assert(
    sizeof(struct pagecache_entries) <= PAGE_SIZE,
    "pagecache_entries size must be less than or equal to page size");

// We always have at least one pagecache_entries. So why use the
// kalloc to allocate it when we can just use a global variable?
static struct pagecache_entries first_pagecache_entries;

// We gotta lock the pagecache entries huh?
static struct spinlock pagecache_entries_lock;

// Which entry should be evicted next?
static struct {
  struct pagecache_entries *entry_frame;
  int entry_index;
} next_eviction_victim;

void pagecache_read(uint32_t block_index, char *data) {}

void pagecache_write(uint32_t block_index, const char *data) {}

void *pagecache_steal(void) {}