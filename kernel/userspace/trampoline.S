#include "cpu/gdt.h"
#include "mem/vmm.h"

.section .text
.intel_syntax noprefix
.global interrupt_handler_asm
.type interrupt_handler_asm, @function
interrupt_handler_asm:
	# Based on sys-v ABI we should preserve rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
	# We already have rsi and rdi so we don't need them
	push rax
	push rdx
	push rcx
	push r8
	push r9
	push r10
	push r11
	# Save the segment registers. To do so, we can simply save GS segment register
	# with a push gs instruction. The reason is that all of the segment registers
	# are the same in our OS and also in x86_64 push gs is a valid instruction
	# unlike push ds or such.
	push gs
	# Move to kernel space
	mov ax, GDT_KERNEL_DATA_SEGMENT
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	# Align the stack using the rbp
	push rbp
	mov rbp, rsp # RBP is callee saved
	and rsp, 0xFFFFFFFFFFFFFFF0
	# Now we can jump to C code
	call handle_trap
	# Restore everything
	mov rsp, rbp
	pop rbp
	pop gs
	mov ax, gs
	mov ds, ax
	mov es, ax
	mov fs, ax
	pop r11
	pop r10
	pop r9
	pop r8
	pop rcx
	pop rdx
	pop rax
	# The way we pushed them in stack in each IRQ
	pop rsi
	pop rdi
	iretq

# Syscall parameters in CrowOS are just like Linux
.global syscall_handler_asm
.type syscall_handler_asm, @function
syscall_handler_asm:
	# We cannot directly access the stack pointer so we have to
	# use rax register. We at first save it to top of the stack and
	# then use rax to move the rsp to the syscall stack. Then, we will
	# restore rax.
	# TODO: Later use swapgs to implement this in a cleaner way
	mov [SYSCALLSTACK_VIRTUAL_ADDRESS_TOP - 8], rax
	mov rax, rsp
	mov [SYSCALLSTACK_VIRTUAL_ADDRESS_TOP - 16], rax
	# Restore rax
	mov rax, [SYSCALLSTACK_VIRTUAL_ADDRESS_TOP - 8]
	# Apply the stack pointer
	mov rsp, SYSCALLSTACK_VIRTUAL_ADDRESS_TOP - 16
	# Save syscall registers
	push rcx
	push r11
	# Jump to C code
	mov rcx, rdx # third argument
	mov rdx, rsi # second argument
	mov rsi, rdi # first argument
	mov rdi, rax # syscall number
	call syscall_c
	# Result is in rax. We don't need to do anything
	# Then switch everything back
	pop r11
	pop rcx
	pop rsp
	sysretq

.section .note.GNU-stack