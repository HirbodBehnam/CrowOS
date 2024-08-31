.section .text
.global handle_dummy_interrupt
.type handle_dummy_interrupt, @function
handle_dummy_interrupt:
	push %rbx
	push %r12
	push %r13
	push %r14
	push %r15
	push %rbp
	call echo_back_char
	pop %rbp
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %rbx
	iretq