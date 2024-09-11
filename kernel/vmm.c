#include "vmm.h"
#include "lib.h"
#include "mem.h"
#include "printf.h"

/**
 * From a virtual address, get the index of PTE entry based on the level
 * of it.
 */
#define PTE_INDEX_FROM_VA(va, level)                                           \
  (((((uint64_t)(va)) >> 12) >> ((uint64_t)(level) * 9)) & ((1 << 9) - 1))

/**
 * Gets the address which we should put in a PTE from a physical address
 */
#define PTE_GET_PHY_ADDRESS(addr) ((addr) >> 12)

/**
 * Follow a PTE to the pagetable/frame it is pointing to.
 * Note that this returns the physical address and later should be converted
 * to virtual address.
 */
static inline pagetable_t pte_follow(struct pte_t pte) {
  return (pagetable_t)((uint64_t)pte.address << 12);
}

/**
 * Return the address of the PTE in page table pagetable that corresponds to
 * virtual address va. If alloc is true, create any required page-table pages.
 *
 * Intel has two page table types: 5 level and 4 level. For our purpose, we only
 * use 4 level paging. Each page in pagetable contains 512 (4096/8) PTEs.
 * Top 16 bits must be zero.
 * A 64-bit virtual address is split into five fields:
 *   48..63 -- must be zero.
 *   39..47 -- 9 bits of level-4 index.
 *   30..38 -- 9 bits of level-3 index.
 *   21..29 -- 9 bits of level-2 index.
 *   12..20 -- 9 bits of level-1 index.
 *    0..11 -- 12 bits of byte offset within the page.
 */
struct pte_t *walk(pagetable_t pagetable, uint64_t va, bool alloc) {
  if (va >= VA_MAX || va < VA_MIN)
    panic("walk");

  for (int level = 3; level > 0; level--) {
    struct pte_t *pte = &pagetable[PTE_INDEX_FROM_VA(va, level)];
    if (pte->present) { // if PTE is here, just point to it
      pagetable = P2V(pte_follow(*pte));
    } else { // PTE does not exists...
      if (!alloc ||
          (pagetable = (pagetable_t)kalloc()) == 0) // should we make one?
        return 0;                                   // either OOM or N/A page
      memset(pagetable, 0, PAGE_SIZE);              // zero the new pagetable
      pte->present = 1;                             // now we have this page
      pte->address =
          PTE_GET_PHY_ADDRESS((uint64_t)V2P(pagetable)); // set the address
    }
  }

  return &pagetable[PTE_INDEX_FROM_VA(va, 0)];
}

/**
 * Create PTEs for virtual addresses starting at va that refer to physical
 * addresses starting at pa. va and size MUST be page-aligned. Returns 0 on
 * success, -1 if walk() couldn't allocate a needed page-table page.
 */
int vmm_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size,
                  uint64_t pa, struct pte_permissions permissions) {
  // Sanity checks
  if (va % PAGE_SIZE != 0)
    panic("vmm_map_pages: va not aligned");
  if (size % PAGE_SIZE != 0)
    panic("vmm_map_pages: size not aligned");
  if (size == 0)
    panic("vmm_map_pages: size");
  // Map each page individually
  const uint64_t pages_to_map = size / PAGE_SIZE;
  for (uint64_t i = 0; i < pages_to_map; i++) {
    const uint64_t current_va = va + i * PAGE_SIZE;
    const uint64_t current_pa = pa + i * PAGE_SIZE;
    struct pte_t *pte = walk(pagetable, current_va, true);
    if (pte == NULL)
      return -1;      // OOM
    if (pte->present) // this page already exists?
      panic("vmm_map_pages: remap");
    pte->present = 1; // make this page available
    pte->rw = permissions.writable;
    pte->xd = !permissions.executable;
    pte->us = permissions.userspace;
    pte->address = PTE_GET_PHY_ADDRESS(current_pa);
  }
  return 0;
}