#include <stddef.h>
#include <stdint.h>

// Defined in usyscalls.S
int read(int, void *, size_t);
int write(int, const void *, size_t);
int open(const char *, int);
int close(int);
char *brk(int);
int exec(const char *, char **);
int exit(int) __attribute__((noreturn));
int wait(uint64_t);
int lseek(int, int64_t, int);

// Yield the program and give the time slice to another program
static inline void yield(void) { __asm__ volatile("int 0x80"); }