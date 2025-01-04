#include <stddef.h>
#include <stdint.h>

int sys_open(const char *path, uint32_t flags);
int sys_read(int fd, char *buffer, size_t len);
int sys_write(int fd, const char *buffer, size_t len);