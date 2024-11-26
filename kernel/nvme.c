#include "nvme.h"
#include "pcie.h"
#include "printf.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Base register for NVMe
 */
static volatile void *nvme_base;

// Returns the addres of a 4 byte long register
#define NVME_REG4(offset) ((uint32_t *)(nvme_base + offset))
// Returns the addres of a 8 byte long register
#define NVME_REG8(offset) ((uint64_t *)(nvme_base + offset))

/**
 * Initialize NVMe driver
 * 
 * Under the hood, it looks for NVMe devices attached to PCIe,
 * initializes the IO queues and setups the interrupts.
 */
void nvme_init(void) {
  // Get the base of NVMe registers
  uint64_t nvme_base_physical = pcie_get_nvme_base();
  // Map for IO based region
  nvme_base = vmm_io_memmap(nvme_base_physical, 0x1000);
  if (nvme_base == NULL)
    panic("could not get NVMe base");
  // Read CAP register
  uint64_t cap = *NVME_REG8(0);
  kprintf("CAP is %llu\n", cap);
}