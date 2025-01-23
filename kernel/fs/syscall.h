#include <stddef.h>
#include <stdint.h>

int sys_open(const char *path, uint32_t flags);
int sys_read(int fd, char *buffer, size_t len);
int sys_write(int fd, const char *buffer, size_t len);
int sys_close(int fd);
int sys_lseek(int fd, int64_t offset, int whence);
int sys_ioctl(int fd, int command, void *data);