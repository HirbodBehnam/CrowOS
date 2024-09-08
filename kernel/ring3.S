#include "gdt.h"

.section .bss
.align 32
ring3_stack:
        .zero   65536

.section .text
.intel_syntax noprefix
.global jump_to_ring3
.type jump_to_ring3, @function
jump_to_ring3:
    # Save current stack pointer
    mov rax, rsp
    push rax

    # Setup the data segments
    mov ax, GDT_USER_DATA_SEGMENT | 3
    mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

    # Create the interrupt values. See figure 6-9 from Intel Manual Volume 3
    mov  rax, GDT_USER_DATA_SEGMENT | 3
    push rax # SS
    mov  rax, OFFSET FLAT:ring3_stack+65535 # Note the end of stack
    push rax # Userspace stack
    pushfq   # eflags
    mov  rax, GDT_USER_CODE_SEGMENT | 3
    push rax # CS
    push [.ring3_halt] # Function to run
    iretq

.ring3_halt:
ring3_halt:
    # For now, we use busyloop
    jmp ring3_halt