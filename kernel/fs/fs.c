#include "CrowFS/crowfs.h"
#include "common/printf.h"
#include "device/nvme.h"
#include "mem/mem.h"

// Hardcoded values of GPT table which we make.
// TODO: Parse the GPT table and find these values.
#define PARTITION_OFFSET 133120
#define PARTITION_SIZE (204766 - PARTITION_OFFSET)

/**
 * Allocates a block from the standard kernel allocator
 */
static union CrowFSBlock *allocate_mem_block(void) { return kcalloc(); }

/**
 * Frees a block which is allocated by allocate_mem_block
 */
static void free_mem_block(union CrowFSBlock *block) { kfree(block); }

/**
 * Writes a single block on the NVMe device. This can be done by writing
 * several logical blocks on the NVMe device. We are also sure that the number
 * of blocks which needs to be written fits in a page size by a panic in the
 * init function.
 *
 * This function always succeeds because the NVMe always does (for now!).
 */
static int write_block(uint32_t block_index, const union CrowFSBlock *block) {
  nvme_write(PARTITION_OFFSET + (uint64_t)block_index *
                                    (CROWFS_BLOCK_SIZE / nvme_block_size()),
             CROWFS_BLOCK_SIZE / nvme_block_size(), (const char *)block);
  return 0;
}

/**
 * Works mostly like write_block function but reads a block. Always succeeds.
 */
static int read_block(uint32_t block_index, union CrowFSBlock *block) {
  nvme_read(PARTITION_OFFSET +
                (uint64_t)block_index * (CROWFS_BLOCK_SIZE / nvme_block_size()),
            CROWFS_BLOCK_SIZE / nvme_block_size(), (char *)block);
  return 0;
}

/**
 * For now, total blocks is hardcoded. We don't need this function for now
 * as well because we are not going to create a new file system.
 */
static uint32_t total_blocks(void) {
  return PARTITION_SIZE / CROWFS_BLOCK_SIZE;
}

/**
 * This function will return the current date in Unix epoch format.
 * But for now, we don't use CMOS to get time and thus, we always return zero.
 */
static int64_t current_date(void) { return 0; }

// The file system which the OS works with.
// At first just fill the functions of it.
// Note: I can't name this fs because of GCC.
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53929
static struct CrowFS main_filesystem = {
    .allocate_mem_block = allocate_mem_block,
    .free_mem_block = free_mem_block,
    .write_block = write_block,
    .read_block = read_block,
    .total_blocks = total_blocks,
    .current_date = current_date,
};

/**
 * Initialize the filesystem. Check if the file system existsing is valid
 * and load metadata of it in the memory.
 */
void fs_init(void) {
  // Block size of the CrowFS must be divisible by the NVMe block size
  if (CROWFS_BLOCK_SIZE % nvme_block_size() != 0)
    panic("fs/nvme indivisible block size");
  // Initialize the file system
  int result = crowfs_init(&main_filesystem);
  if (result != CROWFS_OK)
    panic("fs: bad filesystem");
}