#pragma once
#include <stddef.h>
#include <stdint.h>
// Each inode which represents a dnode and a reference counted
// value which represents the number of files which are using this
// inode
struct fs_inode {
  // What is this inode? File or directory?
  enum { INODE_EMPTY, INODE_FILE, INODE_DIRECTORY } type;
  // The dnode on disk
  uint32_t dnode;
  // The parent of this file/directory. This is always a directory
  uint32_t parent_dnode;
  // Size of the file.
  // Not used in directories.
  uint32_t size;
  // How many of file are using this inode
  uint32_t reference_count;
};

struct fs_inode *fs_open(const char *path, uint32_t flags);
void fs_close(struct fs_inode *inode);
int fs_write(struct fs_inode *inode, const char *buffer, size_t len, size_t offset);
int fs_read(struct fs_inode *inode, char *buffer, size_t len, size_t offset);
void fs_init(void);