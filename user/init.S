.intel_syntax noprefix
.section .text
.globl _start
_start:
    call setup_serial_port # get stdout
    call print_string
end_loop:
    int 0x80 # yield the CPU
    jmp end_loop

# Open the serial port FD in the serial_port global variable
setup_serial_port:
    mov rax, 2 # open syscall
    lea rdi, serial_string # path of the device
    mov rsi, 2048 # device flag
    syscall
    # Result of the syscall (fd) is in rax
    mov QWORD PTR [serial_port], rax
    ret

# Prints a string to terminal
print_string:
    mov rdi, rax # fd
    mov rax, 1 # write syscall
    lea rdi, serial_string # address of a string; TODO: change
    mov rdx, 6 # number of bytes to write
    syscall
    ret

.section .rodata
serial_string:
    .string "serial"
.section .bss
serial_port:
    .zero   8
.section .note.GNU-stack