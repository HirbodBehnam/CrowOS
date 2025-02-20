#pragma once
#include "fs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A file which is open a program
struct process_file {
  // What is this file?
  enum { FD_EMPTY, FD_INODE, FD_DEVICE } type;
  // Data structures which are exclusive to each file type
  union {
    // If the file is FD_INODE, this is the inode of it
    struct fs_inode *inode;
    // If the file is FD_DEVICE, this is the device number of it
    int device;
  } structures;
  // The offset which the file is read.
  // In directories, this value is basically the number of entries read in
  // the current directory.
  uint32_t offset;
  // Can we read from this file?
  bool readble;
  // Can we write in this file?
  bool writable;
};

int file_open(const char *path, uint32_t flags);
int file_write(int fd, const char *buffer, size_t len);
int file_read(int fd, char *buffer, size_t len);
int file_seek(int fd, int64_t offset, int whence);