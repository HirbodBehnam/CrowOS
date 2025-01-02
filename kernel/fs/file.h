#pragma once
#include "fs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// These values are just like Linux.
// The definition and the 
#define O_RDONLY  00
#define O_WRONLY  01
#define O_RDWR    02
#define O_CREAT   0100
#define O_TRUNC   01000
#define O_APPEND  02000

// A file which is open a program
struct fs_file {
  // What is this file?
  enum { FD_EMPTY, FD_INODE, FD_DEVICE } type;
  // Data structures which are exclusive to each file type
  union {
    // If the file is FD_INODE, this is the inode of it
    struct fs_inode *inode;
    // If the file is FD_DEVICE, this is the device number of it
    int device;
  } structures;
  // The offset which the file is read
  uint32_t offset;
  // Can we read from this file?
  bool readble;
  // Can we write in this file?
  bool writable;
};

int file_open(const char *path, uint32_t flags);
int file_write(int fd, const char *buffer, size_t len);
int file_read(int fd, char *buffer, size_t len);