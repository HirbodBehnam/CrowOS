#include <stdint.h>
#include "fs/fs.h"

uint64_t proc_exec(const char *path, const char *args[],
                   struct fs_inode *working_directory);