#include "syscall.h"
#include "CrowFS/crowfs.h"
#include "common/printf.h"
#include "device.h"
#include "file.h"
#include "include/file.h"
#include "userspace/proc.h"

/**
 * open syscall. Either opens a file on physical disk or a device.
 */
int sys_open(const char *path, uint32_t flags) {
  // Is this a device?
  if (flags & O_DEVICE)
    return device_open(path);
  // Nope. Check the file system
  return file_open(path, flags);
}

/**
 * read syscall. Either read from a file on physical disk or a device.
 */
int sys_read(int fd, char *buffer, size_t len) {
  // Is this fd valid?
  struct process *p = my_process();
  if (fd < 0 || fd > MAX_OPEN_FILES || p->open_files[fd].type == FD_EMPTY)
    return -1;
  // Is this fd readable?
  if (!p->open_files[fd].readble)
    return -1;
  // Read from the file/device
  switch (p->open_files[fd].type) {
  case FD_INODE:
    return file_read(fd, buffer, len);
  case FD_DEVICE:
    struct device *dev = device_get(p->open_files[fd].structures.device);
    if (dev == NULL)
      return -1;
    return dev->read(buffer, len);
  default: // not implemented
    return -1;
  }
}

/**
 * write syscall. Either write to a file on physical disk or a device.
 */
int sys_write(int fd, const char *buffer, size_t len) {
  // Is this fd valid?
  struct process *p = my_process();
  if (fd < 0 || fd > MAX_OPEN_FILES || p->open_files[fd].type == FD_EMPTY)
    return -1;
  // Is this fd writable?
  if (!p->open_files[fd].writable)
    return -1;
  // Write to the file/device
  switch (p->open_files[fd].type) {
  case FD_INODE:
    return file_write(fd, buffer, len);
  case FD_DEVICE:
    struct device *dev = device_get(p->open_files[fd].structures.device);
    if (dev == NULL)
      return -1;
    return dev->write(buffer, len);
  default: // not implemented
    return -1;
  }
}

/**
 * Closes a file descriptor. This is an no-op on devices.
 */
int sys_close(int fd) {
  // Is this fd valid?
  struct process *p = my_process();
  if (fd < 0 || fd > MAX_OPEN_FILES || p->open_files[fd].type == FD_EMPTY)
    return -1;
  // Read from the file/device
  switch (p->open_files[fd].type) {
  case FD_INODE:
    fs_close(p->open_files[fd].structures.inode);
    p->open_files[fd].type = FD_EMPTY;
    p->open_files[fd].readble = false;
    p->open_files[fd].writable = false;
    p->open_files[fd].structures.inode = NULL;
    p->open_files[fd].offset = 0;
    return 0;
  case FD_DEVICE: // nothing to do
    return 0;
  default: // not implemented
    return -1;
  }
}

/**
 * Changes the offset of a file descriptor. Returns the new offset of the file
 * descriptor.
 */
int sys_lseek(int fd, int64_t offset, int whence) {
  // Is this fd valid?
  struct process *p = my_process();
  if (fd < 0 || fd > MAX_OPEN_FILES || p->open_files[fd].type == FD_EMPTY)
    return -1;
  // Read from the file/device
  switch (p->open_files[fd].type) {
  case FD_INODE:
    return file_seek(fd, offset, whence);
  case FD_DEVICE:
    struct device *dev = device_get(p->open_files[fd].structures.device);
    if (dev == NULL || dev->lseek == NULL)
      return -1;
    return dev->lseek(offset, whence);
  default: // not implemented
    return -1;
  }
}

/**
 * Sends a command to a device and returns the result of the command or the
 * input in the third argument.
 */
int sys_ioctl(int fd, int command, void *data) {
  // Is this fd valid?
  struct process *p = my_process();
  if (fd < 0 || fd > MAX_OPEN_FILES || p->open_files[fd].type == FD_EMPTY)
    return -1;

  switch (p->open_files[fd].type) {
  case FD_INODE: // Files are not valid
    return -1;
  case FD_DEVICE:
    struct device *dev = device_get(p->open_files[fd].structures.device);
    if (dev == NULL || dev->control == NULL)
      return -1;
    return dev->control(command, data);
  default: // not implemented
    return -1;
  }
}

/**
 * Renames a file or a folder. Renaming can be the same as moving the
 * file/folder.
 *
 * On success, zero is returned.  On error, -1 is returned.
 */
int sys_rename(const char *old_path, const char *new_path) {
  return fs_rename(old_path, new_path, my_process()->working_directory);
}

/**
 * Remove a file or *empty* directory.
 * In case that you want to delete a non empty directory, you have to
 * recursively delete all files in it.
 *
 * On success, zero is returned.  On error, -1 is returned.
 */
int sys_unlink(const char *path) {
  // TODO: If we are deleting an entry and the parent directory is open
  // somewhere, we shall update the number of entries in the parent directory's
  // inode struct.
  return fs_delete(path, my_process()->working_directory);
}

/**
 * Creates an empty directory with the given path.
 *
 * On success, zero is returned.  On error, -1 is returned.
 */
int sys_mkdir(const char *directory) {
  return fs_mkdir(directory, my_process()->working_directory);
}

/**
 * Changes the working directory of the current process to
 * the given relative path.
 */
int sys_chdir(const char *directory) {
  struct process *p = my_process();
  struct fs_inode *new_chdir =
      fs_open(directory, p->working_directory, CROWFS_O_DIR);
  if (new_chdir == NULL) // not found
    return -1;
  // Free the old inode
  fs_close(p->working_directory);
  p->working_directory = new_chdir;
  return 0;
}

/**
 * Reads the content of a directory into the given buffer.
 * Returns the number of entries read or a negative item if
 * there was an error. If the result is zero, it means that either
 * the buffer is so small that nothing can be read or we have reached
 * the end of the directory.
 * The len must be the size of the given buffer.
 * Buffer will contain an array of dirent type defined in file.h
 */
int sys_readdir(int fd, void *buffer, size_t len) {
  struct process *p = my_process();
  if (p == NULL)
    panic("sys_readdir: no process");
  if (fd < 0 || fd >= MAX_OPEN_FILES || p->open_files[fd].type != FD_INODE)
    panic("sys_readdir: fd");
  // Read the directories
  int result = fs_readdir(p->open_files[fd].structures.inode, buffer, len,
                          p->open_files[fd].offset);
  if (result <= 0)
    return result;
  // Save how many entries we have read
  p->open_files[fd].offset += result;
  return result;
}