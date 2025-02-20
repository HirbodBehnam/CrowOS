#pragma once
#include "common/spinlock.h"
#include <stddef.h>
#include <stdint.h>

// Each inode which represents a dnode and a reference counted
// value which represents the number of files which are using this
// inode
struct fs_inode {
  // A lock to disable mutual access to this node
  struct spinlock lock;
  // What is this inode? File or directory?
  enum { INODE_EMPTY, INODE_FILE, INODE_DIRECTORY } type;
  // The dnode on disk
  uint32_t dnode;
  // The parent of this file/directory. This is always a directory
  uint32_t parent_dnode;
  // Size of the file or number of entries in a directory
  uint32_t size;
  // How many of file are using this inode
  uint32_t reference_count;
};

struct fs_inode *fs_open(const char *path, const struct fs_inode *relative_to,
                         uint32_t flags);
void fs_close(struct fs_inode *inode);
void fs_dup(struct fs_inode *inode);
int fs_write(struct fs_inode *inode, const char *buffer, size_t len,
             size_t offset);
int fs_read(struct fs_inode *inode, char *buffer, size_t len, size_t offset);
int fs_rename(const char *old_path, const char *new_path,
              const struct fs_inode *relative_to);
int fs_delete(const char *path, const struct fs_inode *relative_to);
int fs_mkdir(const char *directory, const struct fs_inode *relative_to);
int fs_readdir(const struct fs_inode *inode, void *buffer, size_t len,
               int offset);
void fs_init(void);