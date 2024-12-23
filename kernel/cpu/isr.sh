#!/bin/bash
echo ".section .text"
echo ".intel_syntax noprefix" # fuck AT&T
for irq in {0..255}; do
	echo ".globl irq$irq"
	echo ".type irq$irq, @function"
	echo "irq$irq:"
	# The goal in each of these functions is to have the stack contain
	# rdi and rsi and then the stack which was built in interrupt routine of CPU
	# If the IRQ is one of these, we should get the error code
	# from the top of the stack. Based on Intel Manual Vol. 3 chapter 6.13
	# we need to pop this value before executing IRET.
	# These IRQ values are from xv6 but also available in Intel Manual Vol. 3 chapter 6.15
	if [[ $irq -eq 8 || ($irq -ge 10 && $irq -le 14) || $irq -eq 17 || $irq -eq 21 ]]; then
		# Move the error code in rsi and save rsi to top of stack
		echo "	xchg rsi, [rsp]"
	else
		echo "	push rsi"
		echo "	xor rsi, rsi" # clear rsi
	fi
	echo "	push rdi"
	echo "	mov rdi, $irq"
	# Now we have the IRQ in rsi (first argument) and exception code in rsi (second argument)
	echo "	jmp interrupt_handler_asm"
done

# Put the address of each function in the data segment
echo ".section .data.rel.ro"
echo ".globl irq_vec"
echo "irq_vec:"
for irq in {0..255}; do
	echo "  .quad irq$irq"
done
echo ".section .note.GNU-stack"
