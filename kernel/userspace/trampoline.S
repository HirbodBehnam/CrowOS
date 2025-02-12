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
	# We shall switch the GS base register if we are not coming from the
	# kernel space.
	# Also, for now, FUCK THE NMI AND SHIT (https://wiki.osdev.org/SWAPGS#Complications,_Part_2)
	cmp BYTE PTR [rsp + 8 * 10], 8
	je interrupt_handler_asm_skip_swapgs1
	swapgs
	interrupt_handler_asm_skip_swapgs1:
	# Align the stack using the rbp
	push rbp
	mov rbp, rsp # RBP is callee saved
	and rsp, 0xFFFFFFFFFFFFFFF0
	# Now we can jump to C code
	call handle_trap
	# Restore everything
	mov rsp, rbp
	pop rbp
	# Restore GS base register if needed
	cmp BYTE PTR [rsp + 8 * 10], 8
	je interrupt_handler_asm_skip_swapgs2
	swapgs
	interrupt_handler_asm_skip_swapgs2:
	# Restore registers
	pop r11
	pop r10
	pop r9
	pop r8
	pop rcx
	pop rdx
	pop rax
	# The way we pushed them in stack in each IRQ
	pop rdi
	pop rsi
	iretq

# Syscall parameters in CrowOS are just like Linux
.global syscall_handler_asm
.type syscall_handler_asm, @function
syscall_handler_asm:
	swapgs
	# Now we can access the scratchpad in gs:0
	# Save the user stack pointer
	mov [gs:0], rsp
	# Apply the kernel stack pointer
	mov rsp, SYSCALLSTACK_VIRTUAL_ADDRESS_TOP
	# Save the user stack pointer in kernel stack
	push [gs:0]
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
	swapgs
	sysretq

.section .note.GNU-stack