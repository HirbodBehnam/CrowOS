#include "pagecache.h"
#include "common/lib.h"
#include "common/printf.h"
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
  // TODO: we can later on convert this to bit arithmetics
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
  struct pagecache_entries *entry_frames;
  int entry_index;
} next_eviction_victim;

/**
 * Just read a single block to a buffer. No fuss or anything.
 */
static void pagecache_nvme_read(uint32_t block_index, char *data) {
  nvme_read(block_index, PAGE_SIZE / nvme_block_size(), data);
}

/**
 * Just write a single block from a buffer to disk. No fuss or anything.
 */
static void pagecache_nvme_write(uint32_t block_index, const char *data) {
  nvme_write(block_index, PAGE_SIZE / nvme_block_size(), data);
}

/**
 * Steal a page from the page cache itself. This function must be called when
 * there is an active lock got on pagecache_entries_lock.
 *
 * Will return NULL if the memory is full. Returns the virtual address.
 */
static void *pagecache_do_steal(void) {
  if (next_eviction_victim.entry_frames == NULL)
    panic("next_eviction_victim.entry_frame is null");
  // How many time we looped around?
  int wrap_around_counter = 0;
  // Do the clock algorithm
  while (true) {
    // Move the pointer one forward
    if (next_eviction_victim.entry_index == PAGECACHE_ENTRY_COUNT - 1) {
      next_eviction_victim.entry_index = 0;
      next_eviction_victim.entry_frames =
          next_eviction_victim.entry_frames->next_entries;
      if (next_eviction_victim.entry_frames == NULL) {
        next_eviction_victim.entry_frames = &first_pagecache_entries;
        // Wrap other way around
        wrap_around_counter++;
        if (wrap_around_counter > 2) {
          // No free pages. We have given each page a second change at least one
          return NULL;
        }
      }
    } else {
      next_eviction_victim.entry_frames++;
    }
    // We don't need locks here. We have a lock on pagecache_entries_lock
    // which is a superlock
    struct pagecache_entry *current_frame =
        &next_eviction_victim.entry_frames
             ->entries[next_eviction_victim.entry_index];
    // Is this even valid?
    if (!current_frame->valid)
      continue;
    // Did this page had its second chance?
    if (current_frame->second_chance) { // found one!
      current_frame->valid = false;
      // TODO: write back data
      return current_frame->cache;
    }
    current_frame->second_chance = true;
  }
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
    if (new_entries == NULL) // no free memory
      goto done;
    memset(new_entries, 0, sizeof(struct pagecache_entries));
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
    // Can we repurpose of our pages?
    free_entry->cache = pagecache_do_steal();
    if (free_entry->cache) // Well, shit
      goto done;
  }
  free_entry->valid = true;
  free_entry->disk_block = block_index;
  free_entry->second_chance = false;
  spinlock_lock(&free_entry->lock);
  entry = free_entry;
  should_populate = populate;

// We are done with the list
done:
  spinlock_unlock(&pagecache_entries_lock);

  // Read from the disk if needed
  if (should_populate)
    pagecache_nvme_read(block_index, entry->cache);

  return entry;
}

/**
 * Either read a block from the disk and store it in cache or read the
 * block from the cache which is already in the memory.
 */
void pagecache_read(uint32_t block_index, char *data) {
  struct pagecache_entry *entry =
      get_pagecache_entry_of_index(block_index, true);
  if (entry == NULL) {
    // Just read the page from the disk. No passthrough
    pagecache_nvme_read(block_index, data);
    return;
  }
  entry->second_chance = false;
  // Copy back data. No zero copy and I see why Linux allows zero copy with
  // direct IO only :(
  memcpy(data, entry->cache, PAGE_SIZE);
  // We done with this entry and unlock it
  spinlock_unlock(&entry->lock);
}

/**
 * Either write a block to the disk or get a page cache entry and store it there
 * to be written back later on.
 */
void pagecache_write(uint32_t block_index, const char *data) {
  struct pagecache_entry *entry =
      get_pagecache_entry_of_index(block_index, false);
  if (entry == NULL) {
    // Just write the page to the disk. No passthrough
    pagecache_nvme_write(block_index, data);
    return;
  }
  entry->second_chance = false;
  entry->dirty = true;
  // Copy data to cache
  memcpy(entry->cache, data, PAGE_SIZE);
  // We done with this entry and unlock it
  spinlock_unlock(&entry->lock);
}

/**
 * Steal a page cache memory frame. This function does not steal from the
 * entries list. It only steals from the memory of frames.
 */
void *pagecache_steal(void) {
  spinlock_lock(&pagecache_entries_lock);
  void *result = pagecache_do_steal();
  spinlock_unlock(&pagecache_entries_lock);
  return result;
}