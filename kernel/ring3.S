#include "gdt.h"
#include "vmm.h"

.section .text
.intel_syntax noprefix
.global jump_to_ring3
.type jump_to_ring3, @function
# Jumps to ring3 for the first time. The pagetable of the process must
# be installed before calling this function. The prototype is just
# void jump_to_ring3(void) and everything else is done from now on.
jump_to_ring3:
    # Setup the data segments
    mov ax, GDT_USER_DATA_SEGMENT | 3
    mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

    # Create the interrupt values. See figure 6-9 from Intel Manual Volume 3
    mov  rax, GDT_USER_DATA_SEGMENT | 3
    push rax # SS
    mov  rax, USER_STACK_TOP
    push rax # Userspace stack
    pushfq   # eflags
    mov  rax, GDT_USER_CODE_SEGMENT | 3
    push rax # CS
    mov  rax, USER_CODE_START
    push rax # Function to run

    # Clear all registers
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rdi, rdi
    xor rsi, rsi
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

.section ring3
.global ring3_init
# This is the code which initializes the userspace
ring3_init:
    # For now, we use busyloop
    nop
    syscall
    nop
    int 0x80
    nop
    jmp ring3_init