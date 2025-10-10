#include "pagecache.h"
#include "common/lib.h"
#include "common/spinlock.h"
#include "device/nvme.h"
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
 * method which is called kalloc_for_page_cache is added which simply returns
 * NULL if the free page memory manager does not have a free page. This is also
 * used in page cache allocation itself. After that, we can safely release a
 * page which is occupied by cache itself and repurpose that. Note that there
 * might be an edge case that involves system running out of the memory. In that
 * case, we simply do not cache and pass through the data in the disk.
 *
 * Dirty pages are not handled perfectly here. In modern operating systems, you
 * "age" the dirty pages and do not allow the dirty pages to fill up more than a
 * portion of your page cache. In this OS, however, we don't give a shit.
 * Bookkeeping dirty pages is WAY above my pay grade and lets just write them
 * back when we want to evict a page.
 *
 * Note to myself: The NVMe driver sucks ass and it's fucking blocking :)))))
 * Thus, no need to do sleep lock I guess?
 *
 * Useful pages:
 * https://github.com/openbsd/src/blob/master/sys/kern/vfs_bio.c
 */
struct pagecache_entry {
  // A pointer to the cache page data
  void *cache;
  // A simple lock on the block
  struct spinlock lock;
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
#define PAGECACHE_ENTRY_COUNT 170

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
static struct pagecache_entries first_pagecache_entries = {0};

// We gotta lock the pagecache entries huh?
static struct spinlock pagecache_entries_lock;

// Which entry should be evicted next?
static struct {
  struct pagecache_entries *entry_frame;
  int entry_index;
} next_eviction_victim;

/**
 * Steal a page from the page cache itself. This function must be called when
 * there is an active lock got on pagecache_entries_lock.
 * 
 * Will return NULL if the memory is full.
 */
static void *pagecache_do_steal(void) {
  
}

/**
 * Gets the page cache entry which corresponds with the given block.
 * The entry might be created if it does not exists and populated if needed.
 * The entry will be locked upon returning.
 *
 * This function might return NULL if the memory is filled.
 */
static struct pagecache_entry *
get_pagecache_entry_of_index(uint32_t block_index, bool populate) {
  struct pagecache_entry *entry = NULL;
  /**
   * Should we populate this page at the very end just before returning?
   * At first we assume that the page exists so we do not need to populate from
   * the disk. However, if the page does not exists, we set this value to
   * populate from the argument to argument at last.
   */
  bool should_populate = false;
  // Lock the list to traverse it
  spinlock_lock(&pagecache_entries_lock);

  struct pagecache_entries *current_entries = &first_pagecache_entries;
  struct pagecache_entry *free_entry = NULL;
  while (current_entries != NULL) {
    for (int i = 0; i < PAGECACHE_ENTRY_COUNT; i++) {
      // Is this the block we are looking for?
      if (current_entries->entries[i].valid &&
          current_entries->entries[i].disk_block == block_index) {
        entry = &current_entries->entries[i];
        break;
      }
      // Is this block free?
      if (!current_entries->entries[i].valid)
        free_entry = &current_entries->entries[i];
    }
    // Get next list
    current_entries = current_entries->next_entries;
  }

  // Did we find anything?
  if (entry != NULL) {
    spinlock_lock(&entry->lock);
    entry->second_chance = true;
    goto done;
  }

  // Is there a free entry for it?
  if (free_entry == NULL) {
    // We have to allocate a new free entry.
    struct pagecache_entries *new_entries = kalloc_for_page_cache();
    memset(new_entries, 0, sizeof(struct pagecache_entries));
    if (new_entries == NULL) // no free memory
      goto done;
    // Put it in the list
    current_entries = &first_pagecache_entries;
    while (current_entries->next_entries != NULL)
      current_entries = current_entries->next_entries;
    current_entries->next_entries = new_entries;
    // Use a free entry
    free_entry = &new_entries->entries[0];
  }

  // Try to allocate a page of memory for our entry
  free_entry->cache = kalloc_for_page_cache();
  if (free_entry->cache == NULL) {
    // Out of memory :(
    goto done;
  }
  free_entry->valid = true;
  free_entry->disk_block = block_index;
  spinlock_lock(&free_entry->lock);
  entry = free_entry;
  should_populate = populate;

// We are done with the list
done:
  spinlock_unlock(&pagecache_entries_lock);

  // Read from the disk if needed
  if (should_populate)
    nvme_read(block_index, PAGE_SIZE / nvme_block_size(), entry->cache);

  return entry;
}

/**
 * Either read a block from the disk and store it in cache or read the
 * block from the cache which is already in the memory.
 */
void pagecache_read(uint32_t block_index, char *data) {}

void pagecache_write(uint32_t block_index, const char *data) {}

void *pagecache_steal(void) {}