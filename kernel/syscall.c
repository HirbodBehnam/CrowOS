#include "syscall.h"
#include "asm.h"
#include "gdt.h"

#define IA32_EFER 0xC0000080
#define IA32_STAR 0xC0000081
#define IA32_LSTAR 0xC0000082
#define IA32_FMASK 0xC0000084

// defined in trampoline.S
extern void syscall_handler_asm(void);

/**
 * The IA32_STAR MSR has this layout
 */
struct msr_ia32_star {
  uint64_t reserved : 32;
  uint64_t kernel_segment : 16;
  uint64_t user_segment : 16;
};

/**
 * Initialize the CPU in a way that it accepts syscalls from userspace
 */
void syscall_init(void) {
  // Enable syscall
  wrmsr(IA32_EFER, rdmsr(IA32_EFER) | 1);
  // Set CS/SS of kernel/user space
  struct msr_ia32_star segments = {
      .reserved = 0,
      .kernel_segment = GDT_KERNEL_CODE_SEGMENT,
      // For 64bit apps, CS is loaded from STAR[63:48] + 16
      .user_segment = GDT_USER_DATA_SEGMENT - 8,
  };
  wrmsr(IA32_STAR, *(uint64_t *)&segments);
  // Set the address to jump to
  wrmsr(IA32_LSTAR, (uint64_t)syscall_handler_asm);
  // We don't mask anything (we can mask interrupts tho... shall we?)
  wrmsr(IA32_FMASK, 0);
}

/**
 * The entry point of the syscall for each process.
 */
uint64_t syscall_c(uint64_t syscall_number) {
  switch (syscall_number) {
  case SYSCALL_READ:
    break;
  case SYSCALL_WRITE:
    break;
  case SYSCALL_OPEN:
    break;
  case SYSCALL_CLOSE:
    break;
  case SYSCALL_BRK:
    break;
  case SYSCALL_EXEC:
    break;
  case SYSCALL_EXIT:
    break;
  case SYSCALL_WAIT:
    break;

  default:
    break;
  }

  return 0;
}