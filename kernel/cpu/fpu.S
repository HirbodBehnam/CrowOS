# Mostly from https://wiki.osdev.org/SSE
# Because we are running this OS on x86_64, there is always at least
# SSE2 support here.

.section .text
.intel_syntax noprefix

.global fpu_enable
.type fpu_enable, @function
fpu_enable:
    mov rax, cr0
    and ax, 0xFFFB	# clear coprocessor emulation CR0.EM
    or ax, 0x2		# set coprocessor monitoring  CR0.MP
    mov cr0, rax
    mov rax, cr4
    or ax, 3 << 9	# set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    mov cr4, rax
    ret

.global fpu_save
.type fpu_save, @function
fpu_save:
    fxsave64 [rdi]
    ret

.global fpu_load
.type fpu_load, @function
fpu_load:
    fxrstor64 [rdi]
    ret

.section .note.GNU-stack