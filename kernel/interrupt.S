.section .text
.global handle_dummy_interrupt
.type handle_dummy_interrupt, @function
handle_dummy_interrupt:
    iretq