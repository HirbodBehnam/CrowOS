#include "smp.h"
#include "asm.h"
#include "common/printf.h"

// List of local datas of all cores
static struct cpu_local_data cpu_locals[MAX_CORES];
// An atomic counter for CPU core ID.
// I mean, we can simply any ID to each core, thus just
// use a counter.
static uint8_t next_cpuid = 0;

struct cpu_local_data *cpu_local(void) {
  return (struct cpu_local_data *)rdmsr(MSR_GS_BASE);
}

uint8_t get_processor_id(void) { return cpu_local()->cpuid; }

void cpu_local_setup(void) {
  uint8_t cpuid = __atomic_fetch_add(&next_cpuid, 1, __ATOMIC_RELAXED);
  if (cpuid > MAX_CORES)
    panic("too much cores");
  cpu_locals[cpuid].cpuid = cpuid;
  wrmsr(MSR_GS_BASE, (uint64_t)&cpu_locals[cpuid]);
  wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&cpu_locals[cpuid]);
}