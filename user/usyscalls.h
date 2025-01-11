// Defined in usyscalls.S
int read(int, void *, int);
int write(int, const void *, int);
int open(const char *, int);
int close(int);
char *brk(int);
int exec(const char *, char **);
int exit(int) __attribute__((noreturn));
int wait(int *);

// Yield the program and give the time slice to another program
static inline void yield(void) { __asm__ volatile("int 0x80"); }