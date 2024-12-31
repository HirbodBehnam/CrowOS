#include "fs.h"
#include "CrowFS/crowfs.h"
#include "common/lib.h"
#include "common/printf.h"
#include "common/spinlock.h"
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

#define MAX_INODES 64
// A static list of inodes
static struct {
  // List of all inodes on the memory
  struct fs_inode inodes[MAX_INODES];
  // A lock to disable mutual access
  struct spinlock lock;
} fs_inode_list;

/**
 * Opens the inode for the given file. Returns NULL
 * if there is no free inodes or the file does not exists.
 *
 * Flags must correspond to the CrowFS flags.
 */
struct fs_inode *fs_open(const char *path, uint32_t flags) {
  // Get the dnode from the file system
  uint32_t dnode, parent;
  int result = crowfs_open(&main_filesystem, path, &dnode, &parent, flags);
  if (result != CROWFS_OK)
    return NULL;
  // Look for an inode
  struct fs_inode *inode = NULL, *free_inode = NULL;
  spinlock_lock(&fs_inode_list.lock);
  for (int i = 0; i < MAX_INODES; i++) {
    if (fs_inode_list.inodes[i].type == INODE_EMPTY) {
      // We save a free inode in case that we end up not having this inode in
      // the list of open inodes
      free_inode = &fs_inode_list.inodes[i];
    } else if (fs_inode_list.inodes[i].dnode == dnode) {
      // We found the inode!
      inode = &fs_inode_list.inodes[i];
      // Note for myself: I'm not sure about this. The whole goddamn
      // file system is racy and buggy as fuck. If we se the parent
      // each time we move this inode, I think we won't have an issue
      // in regard of inode->parent_dnode. So I don't think we need
      // to set the inode->parent_dnode as parent.
      // Even setting it MIGHT cause some race issues.
      inode->reference_count++;
      break;
    }
  }
  // Did we found an inode? If not, did we found a free inode?
  if (inode == NULL && free_inode != NULL) {
    inode = free_inode;
    inode->dnode = dnode;
    inode->parent_dnode = parent;
    inode->reference_count = 1;
    // TODO: Move this to the file system.
    struct CrowFSStat stat;
    result = crowfs_stat(&main_filesystem, dnode, &stat);
    if (result != CROWFS_OK)
      panic("fs_open stat failed");
    switch (stat.type) {
    case CROWFS_ENTITY_FILE:
      inode->type = INODE_FILE;
      inode->size = stat.size;
      break;
    case CROWFS_ENTITY_FOLDER:
      inode->type = INODE_DIRECTORY;
      break;
    default:
      panic("open: invalid dnode type");
      break;
    }
  }
  spinlock_unlock(&fs_inode_list.lock);
  return inode;
}

/**
 * Closes an inode. Decrements it's reference counter and
 * frees it if needed.
 */
void fs_close(struct fs_inode *inode) {
  spinlock_lock(&fs_inode_list.lock);
  inode->reference_count--;
  if (inode->reference_count == 0)
    memset(inode, 0, sizeof(*inode));
  spinlock_unlock(&fs_inode_list.lock);
}

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