#include "common/printf.h"
#include "device/serial_port.h"
#include "traps.h"
#include "userspace/proc.h"
#include <stddef.h>
#include <stdint.h>

/**
 * interrupt_handler_asm will call this function
 */
void handle_trap(uint64_t irq, uint64_t error_code) {
  (void)error_code;
  switch (irq) {
  case T_IRQ0 + IRQ_COM1:
    serial_echo_back_char();
    break;
  case T_YEILD: {
    struct process *proc = my_process();
    spinlock_lock(&proc->lock);
    proc->state = RUNNABLE;
    scheduler_switch_back();
    spinlock_unlock(&proc->lock);
  } break;
  default:
    kprintf("irq: %llu - error: %llx\n", irq, error_code);
    panic("irq");
  }
}