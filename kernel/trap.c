#include "proc.h"
#include "printf.h"
#include "serial_port.h"
#include "traps.h"
#include <stddef.h>
#include <stdint.h>

/**
 * interrupt_handler_asm will call this function
 */
void handle_trap(uint64_t irq /*, uint64_t error_code*/) {
  switch (irq) {
  case T_IRQ0 + IRQ_COM1:
    serial_echo_back_char();
    break;
  case T_YEILD:
    my_process()->state = RUNNABLE;
    scheduler_switch_back();
    break;
  default:
    panic("irq");
  }
}