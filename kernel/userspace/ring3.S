#include "cpu/gdt.h"
#include "mem/vmm.h"

.section .text
.intel_syntax noprefix
.global jump_to_ring3
.type jump_to_ring3, @function
# Jumps to ring3 for the first time of running a program.
# The pagetable of the process must be installed before calling
# this function. The prototype is void jump_to_ring3(uint64_t rsp, uint64_t _start, int argc)
# which rsp is the initial stack pointer and the _start is the code address
# which we should jump to.
# However there is a very strange thing about this function: The calling convention.
# We do not use SYS-V ABI to call this function. Because the entry point of this
# function is from a context_switch, we do not control rdi and rsi. But we do control
# callee saved registers such as r14 and r15. And thus, we exactly use r14 and r15
# for the rsp value and the _start address.
# argc is passed to the user program. It shall be put in r13.
# You might wonder, what about argv? Lemme tell you. argv is the rsp!
jump_to_ring3:
    # Setup the data segments
    mov rax, GDT_USER_DATA_SEGMENT | 3
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    # Create the interrupt values. See figure 6-9 from Intel Manual Volume 3
    push rax # SS, set to GDT_USER_DATA_SEGMENT | 3
    push r14 # Userspace stack
    mov  rax, 0x200 # Only IF is set
    push rax # rflags
    mov  rax, GDT_USER_CODE_SEGMENT | 3
    push rax # CS
    push r15 # Function to run

    # Set the registers for userspace
    mov rdi, r13 # argc
    mov rsi, r14 # argv / userstack

    # Clear all registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    # Jump to userspace
    iretq

.section .note.GNU-stack