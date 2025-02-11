#include "smp.h"

struct cpu_local_data *cpu_local(void) {
  uint64_t data_ptr;
  __asm__ volatile("lea rax, [gs:0]" : "=a"(data_ptr) :);
  return (struct cpu_local_data *)data_ptr;
}

uint8_t get_processor_id(void) { return cpu_local()->cpuid; }