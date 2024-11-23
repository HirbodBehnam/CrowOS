void pcie_list(void);

/**
 * Gets the NVMe base register from PCIe tree. This is the virtual address
 *
 * Returns 0 if not found.
 */
void *pcie_get_nvme_base(void);