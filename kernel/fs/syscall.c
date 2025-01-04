#include "syscall.h"
#include "device.h"
#include "file.h"
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
  if (p->open_files[fd].type == FD_EMPTY)
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
  if (p->open_files[fd].type == FD_EMPTY)
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
