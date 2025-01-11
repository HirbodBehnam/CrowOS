#define SYSCALL_READ  0
#define SYSCALL_WRITE 1
#define SYSCALL_OPEN  2
#define SYSCALL_CLOSE 3
#define SYSCALL_BRK   4
#define SYSCALL_EXEC  5
#define SYSCALL_EXIT  6
#define SYSCALL_WAIT  7

#ifndef __ASSEMBLER__
void syscall_init(void);
#endif