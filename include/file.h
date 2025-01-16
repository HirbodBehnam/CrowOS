// These values are just like Linux.
// The definition and the values
#define O_RDONLY  00
#define O_WRONLY  01
#define O_RDWR    02
#define O_CREAT   0100
#define O_TRUNC   01000
#define O_APPEND  02000
#define O_DEVICE  04000 // open a device file instead of a file

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Default file descriptors for stdin, stdout and stderr.
// Basically POSIX numbering.
#define DEFAULT_STDIN  0
#define DEFAULT_STDOUT 1
#define DEFAULT_STDERR 2