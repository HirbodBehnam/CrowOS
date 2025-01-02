#include "file.h"
#include "common/printf.h"
#include "CrowFS/crowfs.h"
#include "userspace/proc.h"

/**
 * Opens a file/directory in the running process.
 * The path must be a FILE in the filesystem. Devices and
 * pipes are not handled here.
 *
 * Returns the fd which was allocated. -1 on failure.
 */
int file_open(const char *path, uint32_t flags) {
  // Look for an empty file.
  // This is OK to be not locked because each process is
  // currently only single threaded.
  struct process *p = my_process();
  if (p == NULL)
    panic("file_open: no process");
  int fd = -1;
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (p->open_files[i].type == FD_EMPTY) {
      fd = i;
      break;
    }
  }
  if (fd == -1) // out of free files
    return -1;
  // Note: I can defer the p->open_files[i].type = FD_... because
  // of single threaded.
  // Try to open the file
  // Flags for now are very simple.
  uint32_t fs_flags = (flags & O_CREAT) ? CROWFS_O_CREATE : 0;
  struct fs_inode *inode = fs_open(path, fs_flags);
  if (inode == NULL)
    return -1;
  // Now open the file
  p->open_files[fd].type = FD_INODE;
  p->open_files[fd].structures.inode = inode;
  p->open_files[fd].offset = 0;
  p->open_files[fd].readble = (flags & O_WRONLY) != 0;
  p->open_files[fd].writable = (flags & O_WRONLY) || (flags & O_RDWR);
  return fd;
}

/**
 * Writes a data to a file. Expects the fd to be valid.
 * 
 * Returns the number of bytes written or a negative value on error.
 */
int file_write(int fd, const char *buffer, size_t len) {
  struct process *p = my_process();
  if (p == NULL)
    panic("file_open: no process");
  if (fd < 0 || fd >= MAX_OPEN_FILES || !p->open_files[fd].writable || p->open_files[fd].type != FD_INODE)
    panic("fd_open: fd");
  int result = fs_write(p->open_files[fd].structures.inode, buffer, len, p->open_files[fd].offset);
  if (result < 0)
    return result;
  p->open_files[fd].offset += result;
  return result;
}

/**
 * Reads data from a file. Expects the fd to be valid.
 * 
 * Returns the number of bytes written or a negative value on error.
 */
int file_read(int fd, char *buffer, size_t len) {
  struct process *p = my_process();
  if (p == NULL)
    panic("file_open: no process");
  if (fd < 0 || fd >= MAX_OPEN_FILES || !p->open_files[fd].readble || p->open_files[fd].type != FD_INODE)
    panic("fd_open: fd");
  int result = fs_read(p->open_files[fd].structures.inode, buffer, len, p->open_files[fd].offset);
  if (result < 0)
    return result;
  p->open_files[fd].offset += result;
  return result;
}