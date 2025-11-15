#include "fs.h"
#include "CrowFS/crowfs.h"
#include "common/lib.h"
#include "common/printf.h"
#include "common/spinlock.h"
#include "device/nvme.h"
#include "device/rtc.h"
#include "include/file.h"
#include "mem/mem.h"
#include "mem/pagecache.h"

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
  pagecache_write(PARTITION_OFFSET +
                      (uint64_t)block_index *
                          (CROWFS_BLOCK_SIZE / nvme_block_size()),
                  (const char *)block);
  return 0;
}

/**
 * Works mostly like write_block function but reads a block. Always succeeds.
 */
static int read_block(uint32_t block_index, union CrowFSBlock *block) {
  pagecache_read(PARTITION_OFFSET + (uint64_t)block_index *
                                        (CROWFS_BLOCK_SIZE / nvme_block_size()),
                 (char *)block);
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
 */
static int64_t current_date(void) { return rtc_now(); }

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
 *
 * If relative_to is NULL, then open works relative to the root.
 */
struct fs_inode *fs_open(const char *path, const struct fs_inode *relative_to,
                         uint32_t flags) {
  // Get the dnode from the file system
  uint32_t dnode, parent;
  uint32_t relative_to_dnode =
      relative_to == NULL ? main_filesystem.root_dnode : relative_to->dnode;
  int result = crowfs_open_relative(&main_filesystem, path, relative_to_dnode,
                                    &dnode, &parent, flags);
  if (result != CROWFS_OK)
    return NULL;
  // Look for an inode
  struct fs_inode *inode = NULL, *free_inode = NULL;
  spinlock_lock(&fs_inode_list.lock);
  for (int i = 0; i < MAX_INODES; i++) {
    // Note: We have to lock here as well because of races that
    // can happen if another core is trying to delete an inode and another
    // is opening one.
    spinlock_lock(&fs_inode_list.inodes[i].lock);
    if (fs_inode_list.inodes[i].type == INODE_EMPTY) {
      // We save a free inode in case that we end up not having this inode in
      // the list of open inodes
      free_inode = &fs_inode_list.inodes[i];
      spinlock_unlock(&fs_inode_list.inodes[i].lock);
    } else if (fs_inode_list.inodes[i].dnode == dnode) {
      // We found the inode!
      inode = &fs_inode_list.inodes[i];
      // Note for myself: I'm not sure about this. The whole goddamn
      // file system is racy and buggy as fuck. If we set the parent
      // each time we move this inode, I think we won't have an issue
      // in regard of inode->parent_dnode. So I don't think we need
      // to set the inode->parent_dnode as parent.
      // Even setting it MIGHT cause some race issues.
      __atomic_add_fetch(&inode->reference_count, 1, __ATOMIC_RELAXED);
      spinlock_unlock(&fs_inode_list.inodes[i].lock);
      break;
    } else { // Some other in use file, just unlock the spinlock
      spinlock_unlock(&fs_inode_list.inodes[i].lock);
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
      inode->size = stat.size;
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
 * Duplicates an inode. Basically, increases its reference counter
 */
void fs_dup(struct fs_inode *inode) {
  // Note: No locks needed here
  __atomic_add_fetch(&inode->reference_count, 1, __ATOMIC_RELAXED);
}

/**
 * Closes an inode. Decrements it's reference counter and
 * frees it if needed.
 */
void fs_close(struct fs_inode *inode) {
  spinlock_lock(&inode->lock);
  if (__atomic_sub_fetch(&inode->reference_count, 1, __ATOMIC_ACQ_REL) == 0) {
    inode->type = INODE_EMPTY;
    inode->dnode = 0;
    inode->parent_dnode = 0;
    inode->size = 0;
  }
  spinlock_unlock(&inode->lock);
}

/**
 * Writes a chunk of data in the disk.
 *
 * Returns the number of bytes written or -1 on error.
 */
int fs_write(struct fs_inode *inode, const char *buffer, size_t len,
             size_t offset) {
  spinlock_lock(&inode->lock);
  int result =
      crowfs_write(&main_filesystem, inode->dnode, buffer, len, offset);
  if (result != CROWFS_OK) { // Error
    spinlock_unlock(&inode->lock);
    return -1;
  }
  // Increase the file size if needed
  if (offset + len > inode->size)
    inode->size = offset + len;
  spinlock_unlock(&inode->lock);
  return (int)len;
}

/**
 * Reads a chunk of data from the disk.
 *
 * Returns the number of bytes written or -1 on error.
 */
int fs_read(struct fs_inode *inode, char *buffer, size_t len, size_t offset) {
  spinlock_lock(&inode->lock);
  int result = crowfs_read(&main_filesystem, inode->dnode, buffer, len, offset);
  spinlock_unlock(&inode->lock);
  if (result < 0)
    return -1;
  return result;
}

/**
 * Renames a file or directory and updates all effected inodes.
 */
int fs_rename(const char *old_path, const char *new_path,
              const struct fs_inode *relative_to) {
  (void)new_path;
  (void)old_path;
  (void)relative_to;
  panic("NOT IMPLEMENTED");
}

/**
 * Deletes a file or empty directory. Will fail if the file/directory is open
 * in a program.
 */
int fs_delete(const char *path, const struct fs_inode *relative_to) {
  uint32_t dnode, parent_dnode;
  int result = crowfs_open_relative(&main_filesystem, path, relative_to->dnode,
                                    &dnode, &parent_dnode, 0);
  if (result != CROWFS_OK)
    return -1; // does not exist
  result = crowfs_delete(&main_filesystem, dnode, parent_dnode);
  if (result != CROWFS_OK)
    return -1;
  return 0;
}

/**
 * Creates an empty directory.
 */
int fs_mkdir(const char *directory, const struct fs_inode *relative_to) {
  uint32_t dnode, parent_dnode;
  int result = crowfs_open_relative(&main_filesystem, directory,
                                    relative_to->dnode, &dnode, &parent_dnode,
                                    CROWFS_O_CREATE | CROWFS_O_DIR);
  if (result != CROWFS_OK)
    return -1;
  return 0;
}

/**
 * Reads a directory to a dirent struct array.
 * Returns the number of entries read or zero if we have reached end of
 * the directory. If an error occurs, a negative number will be returned.
 *
 * Buffer must be the type of struct dirent and len is the size of the buffer.
 */
int fs_readdir(const struct fs_inode *inode, void *buffer, size_t len,
               int offset) {
  if (inode->type != INODE_DIRECTORY)
    return -1; // not directory
  // Read each entry
  struct CrowFSStat stat;
  int read_directories = 0;
  while (1) {
    int result = crowfs_read_dir(&main_filesystem, inode->dnode, &stat, offset);
    if (result == CROWFS_ERR_LIMIT) // end of dir
      break;
    if (result != CROWFS_OK) // fuckup
      return -2;
    // What is the directory name size
    size_t directory_name_length = strlen(stat.name);
    // If the buffer cannot even hold the dirent, bail
    if (sizeof(struct dirent) + directory_name_length >= len)
      break;
    // Copy the variables
    struct dirent *d = buffer;
    switch (stat.type) {
    case CROWFS_ENTITY_FILE:
      d->type = DT_FILE;
      break;
    case CROWFS_ENTITY_FOLDER:
      d->type = DT_DIR;
      break;
    default:
      panic("fs_readdir: invalid type");
    }
    d->creation_date = stat.creation_date;
    d->size = stat.size;
    strcpy(d->name, stat.name);
    // Update variables for next iteration
    offset++;
    read_directories++;
    len -= sizeof(struct dirent) + directory_name_length;
    buffer += sizeof(struct dirent) + directory_name_length;
  }
  return read_directories;
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