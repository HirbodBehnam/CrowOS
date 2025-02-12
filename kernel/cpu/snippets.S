#include "gdt.h"

.section .text
.intel_syntax noprefix
.global reload_segments
.type reload_segments, @function
# Load the GDT and reload the segments to point to kernel data and user segment
# Function prototype is void reload_segments(void *gdt)
reload_segments:
    lgdt [rdi]
    push GDT_KERNEL_CODE_SEGMENT
    lea rax, [.reload_CS]
    push rax
    retfq
.reload_CS:
    # SS must contain the data segment
    mov ax, GDT_KERNEL_DATA_SEGMENT
    mov ss, ax
    # Other segments contain zero
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ret

.global context_switch
.type context_switch, @function
# Switch context between two contexts by saving the callee saved registers
# and switching the stack pointer between two tasks or contexes.
# Function prototype is void context_switch(uint64_t to_rsp, uint64_t *from_rsp)
# It will effectively set the RSP to "to_rsp" and save the previous RSP in "from_rsp"
context_switch:
    # We should preserve rbx, rsp, rbp, r12, r13, r14, and r15 based on sys-v ABI
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    # Switch the context
    mov [rsi], rsp
    mov rsp, rdi

    # Now this code is running in the new context and will restore it!
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

.section .note.GNU-stack