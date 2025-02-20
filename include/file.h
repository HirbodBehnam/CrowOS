#include <stdint.h>

// These values are just like Linux.
// The definition and the values
#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02
#define O_CREAT 0100
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_DEVICE 04000 // open a device file instead of a file
#define O_DIR 010000   // open a directory instead of a file

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Default file descriptors for stdin, stdout and stderr.
// Basically POSIX numbering.
#define DEFAULT_STDIN 0
#define DEFAULT_STDOUT 1
#define DEFAULT_STDERR 2

#define DT_DIR  1
#define DT_FILE 2

/**
 * Each entry of folder is represented like this
 */
struct dirent {
  int64_t creation_date;
  // Size of the file, or number of entries in the folder
  uint32_t size;
  // One of the DT_ variables
  uint8_t type;
  // Name, null terminated
  char name[];
};