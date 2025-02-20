#!/bin/bash
echo '#include "include/syscall.h"'
echo ".section .text"
echo ".intel_syntax noprefix" # fuck AT&T
for syscall in "read" "write" "open" "close" "sbrk" "exec" "exit" "wait" "lseek" "time" "sleep" "ioctl" "rename" "unlink" "mkdir" "chdir" "readdir"; do
	echo ".globl $syscall"
	echo ".type $syscall, @function"
	echo "$syscall:"
	echo "  mov rax, SYSCALL_${syscall^^}" # syscall number from syscall.h
    # The syscall arguments must be in the rdi, rsi and rdx which is the
    # registers used to pass arguments to this function as well!
    # This means that we can just call syscall because the rdi, rsi and rdx
    # are filled with correct values.
    echo "  syscall"
    # The result of the syscall is in the rax which is the register which
    # has the return value in the sys-v ABI!
    echo "  ret"
done

echo ".section .note.GNU-stack"
