#include "gdt.h"

.section .text
.intel_syntax noprefix
.global jump_to_ring3
.type jump_to_ring3, @function
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
    push rsi # Userspace stack
    pushfq   # eflags
    mov  rax, GDT_USER_CODE_SEGMENT | 3
    push rax # CS
    push rdi # Function to run
    iretq

.section ring3
.global ring3_halt
ring3_halt:
    # For now, we use busyloop
    jmp ring3_halt