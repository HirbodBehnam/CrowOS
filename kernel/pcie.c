#include "pcie.h"
#include "asm.h"
#include "printf.h"
#include "spinlock.h"

/**
 * Reads a PCIe register from a bus and a slot.
 * From https://wiki.osdev.org/PCI
 */
static uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func,
                              uint8_t offset) {
  uint32_t lbus = (uint32_t)bus;
  uint32_t lslot = (uint32_t)slot;
  uint32_t lfunc = (uint32_t)func;

  // Create configuration address as per Figure 1
  uint32_t address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) |
                                (offset & 0xFC) | ((uint32_t)0x80000000));

  // Save and disable interrupts
  bool interrupts_enabled = is_interrupts_enabled();
  cli();
  // Write out the address
  outl(0xCF8, address);
  // Read in the data
  // (offset & 2) * 8) = 0 will choose the first word of the 32-bit register
  uint16_t result = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
  // Restore interrupts
  if (interrupts_enabled)
    sti();
  return result;
}

/**
 * Gets the NVMe base register from PCIe tree. This is the physical address
 *
 * Returns 0 if not found.
 */
uint64_t pcie_get_nvme_base(void) {
  for (uint32_t device = 0; device < 32; device++) {
    uint16_t vendor = pci_config_read_word(0, device, 0, 0);
    // Check if device exists
    if (vendor == 0xFFFF)
      continue;
    uint16_t class_subclass = pci_config_read_word(0, device, 0, 0xA);
    uint16_t prog_if = pci_config_read_word(0, device, 0, 0x8) >> 8;
    // Check NVMe
    if (class_subclass != 0x108 || prog_if != 2)
      continue;
    // Read the BAR values
    uint32_t bar0 = (uint32_t)pci_config_read_word(0, device, 0, 0x10) | ((uint32_t)pci_config_read_word(0, device, 0, 0x12) << 16);
    uint32_t bar1 = (uint32_t)pci_config_read_word(0, device, 0, 0x14) | ((uint32_t)pci_config_read_word(0, device, 0, 0x16) << 16);
    // Create the physical address
    uint64_t bar = (((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0));
    kprintf("NVMe found at %p\n", bar);
    return bar;
  }
  return 0;
}

/**
 * Lists all PCIe devices
 */
void pcie_list(void) {
  kprintf("Attached PCIe devices:\n");
  for (uint8_t bus = 0; bus < 255; bus++) {
    for (uint32_t device = 0; device < 32; device++) {
      uint16_t vendor = pci_config_read_word(bus, device, 0, 0);
      if (vendor == 0xFFFF)
        continue;
      uint16_t class_subclass = pci_config_read_word(bus, device, 0, 0xA);
      uint16_t prog_if = pci_config_read_word(bus, device, 0, 0x8) >> 8;
      kprintf("PCIe device %u.%u -> 0x%x -> 0x%x %u\n", (uint32_t)bus,
              (uint32_t)device, (uint32_t)vendor, (uint32_t)class_subclass,
              (uint32_t)prog_if);
    }
  }
}