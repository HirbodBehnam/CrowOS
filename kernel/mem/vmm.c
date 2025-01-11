#include "vmm.h"
#include "common/lib.h"
#include "common/printf.h"
#include "cpu/asm.h"

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
 * Number of PTEs in a pagetable
 */
#define PAGETABLE_PTE_COUNT 512

/**
 * Gets the lower boundry of the page which we are trying to access.
 */
#define PAGE_ROUND_DOWN(a) ((a) & ~(PAGE_SIZE - 1))

/**
 * Follow a PTE to the pagetable/frame it is pointing to.
 * Note that this returns the physical address and later should be converted
 * to virtual address.
 */
static inline pagetable_t pte_follow(struct pte_t pte) {
  return (pagetable_t)((uint64_t)pte.address << 12);
}

/**
 * The kernel pagetable which Limine sets up for us. This is in virtual
 * address space.
 */
static pagetable_t kernel_pagetable;

/**
 * kernel address which we got from limine
 */
static struct limine_kernel_address_response kernel_address;

/**
 * The address which we start assigning new io memory map requests to.
 * Increments with each IO memmap request.
 *
 * This value looks like the next sector after the code of Limine
 * thus I think it's safe to use it. Limine uses 0xffffffff80000000
 * region.
 */
static uint64_t io_memmap_current_address = 0xfffffffff0000000;

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
static struct pte_t *walk(pagetable_t pagetable, uint64_t va, bool alloc,
                          bool io) {
  if ((!io && va >= VA_MAX) || va < VA_MIN)
    panic("walk");

  for (int level = 3; level > 0; level--) {
    struct pte_t *pte = &pagetable[PTE_INDEX_FROM_VA(va, level)];
    if (pte->present) { // if PTE is here, just point to it
      pagetable = (pagetable_t)P2V(pte_follow(*pte));
    } else { // PTE does not exists...
      if (!alloc ||
          (pagetable = (pagetable_t)kalloc()) == 0) // should we make one?
        return 0;                                   // either OOM or N/A page
      memset(pagetable, 0, PAGE_SIZE);              // zero the new pagetable
      pte->present = 1;                             // now we have this page
      // Just like xv6, we do some generous access bits on every page allocated.
      // The last PTE will take care of the actual access bits.
      pte->xd = 0;
      pte->rw = 1;
      pte->us = 1;
      pte->address = PTE_GET_PHY_ADDRESS(V2P(pagetable)); // set the address
    }
  }

  return &pagetable[PTE_INDEX_FROM_VA(va, 0)];
}

/**
 * Deep copy the pagetable (and not the frames) to another pagetable.
 *
 * TODO: I know that if we goto OOM state we won't free the frames used by
 * pagetables.
 */
static int copy_pagetable(pagetable_t dst, const pagetable_t src, int level) {
  // The first step is to copy the pagetable from src to dest (the PTEs)
  memcpy(dst, src, PAGE_SIZE);
  // Next check if this is the leaf (points to 4kb page)
  if (level == 0)
    return 0;
  // Now check each entry in pagetable
  for (size_t i = 0; i < PAGETABLE_PTE_COUNT; i++) {
    const struct pte_t pte_src = src[i];
    if (pte_src.present && !pte_src.huge_page) {
      // If this PTE is not a huge page, we should allocate a frame
      // and copy the inner pagetable. Then we should recursively copy the
      // content of the pagetable which this PTE is pointing at to the
      // allocated frame.
      pagetable_t dst_inner_pagetable = (pagetable_t)kalloc();
      if (dst_inner_pagetable == NULL) // OOM!
        return 1;
      const pagetable_t src_inner_pagetable =
          (pagetable_t)P2V(pte_follow(pte_src));
      if (copy_pagetable(dst_inner_pagetable, src_inner_pagetable, level - 1) !=
          0) // OOM!
        return 1;
    }
  }
  return 0;
}

/**
 * We just save the limine_kernel_address_response to be later accessed.
 */
void vmm_init_kernel(
    const struct limine_kernel_address_response _kernel_address) {
  kernel_address = _kernel_address;
  kernel_pagetable = (pagetable_t)P2V(get_installed_pagetable());
}

/**
 * Gets the physical addres of a virtual address from a page table.
 * If user is true, the page must be in user mode. Otherwise it should be
 * in the kernel mode. Returns zero if the page does not exists or the
 * permissions do not match.
 */
uint64_t vmm_walkaddr(pagetable_t pagetable, uint64_t va, bool user) {
  if (va >= VA_MAX)
    return 0;

  struct pte_t *pte = walk(pagetable, va, false, false);
  if (pte == 0)
    return 0;
  if (!pte->present)
    return 0;
  if (pte->us != user)
    return 0;
  return pte->address << 12;
}

/**
 * Create PTEs for virtual addresses starting at va that refer to physical
 * addresses starting at pa. va and size MUST be page-aligned. Returns 0 on
 * success, -1 if walk() couldn't allocate a needed page-table page.
 */
int vmm_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size,
                  uint64_t pa, pte_permissions permissions) {
  // Sanity checks
  if (pa % PAGE_SIZE != 0)
    panic("vmm_map_pages: pa not aligned");
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
    struct pte_t *pte = walk(pagetable, current_va, true, false);
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

/**
 * Allocates pages in a page table. va must be page aligned and the size
 * must be devisable by page size. Returns 0 on success, -1 if walk() couldn't
 * allocate a needed pagetable page.
 */
int vmm_allocate(pagetable_t pagetable, uint64_t va, uint64_t size,
                 pte_permissions permissions) {
  // Sanity checks
  if (va % PAGE_SIZE != 0)
    panic("vmm_allocate: va not aligned");
  if (size % PAGE_SIZE != 0)
    panic("vmm_allocate: size not aligned");
  if (size == 0)
    panic("vmm_allocate: size");
  // Allocate pages
  const uint64_t pages_to_alloc = size / PAGE_SIZE;
  for (uint64_t i = 0; i < pages_to_alloc; i++) {
    const uint64_t current_va = va + i * PAGE_SIZE;
    void *frame = kalloc();
    if (frame == NULL)
      return -1;
    struct pte_t *pte = walk(pagetable, current_va, true, false);
    if (pte == NULL)
      return -1;      // OOM
    if (pte->present) // this page already exists?
      panic("vmm_allocate: remap");
    pte->present = 1; // make this page available
    pte->rw = permissions.writable;
    pte->xd = !permissions.executable;
    pte->us = permissions.userspace;
    pte->address = PTE_GET_PHY_ADDRESS(V2P(frame));
  }
  return 0;
}

/**
 * Maps a physical address which is used for IO.
 * Returns the virtual address which the region is mapped to.
 */
void *vmm_io_memmap(uint64_t pa, uint64_t size) {
  // Sanity checks
  if (pa % PAGE_SIZE != 0)
    panic("vmm_io_memmap: pa not aligned");
  if (size % PAGE_SIZE != 0)
    panic("vmm_io_memmap: size not aligned");
  if (size == 0)
    panic("vmm_io_memmap: size");
  // Calculate the virtual address
  uint64_t va =
      __atomic_fetch_add(&io_memmap_current_address, size, __ATOMIC_RELAXED);
  // Map each page individually
  const uint64_t pages_to_map = size / PAGE_SIZE;
  for (uint64_t i = 0; i < pages_to_map; i++) {
    const uint64_t current_va = va + i * PAGE_SIZE;
    const uint64_t current_pa = pa + i * PAGE_SIZE;
    struct pte_t *pte = walk(kernel_pagetable, current_va, true, true);
    if (pte == NULL)
      return NULL;    // OOM
    if (pte->present) // this page already exists?
      panic("vmm_io_memmap: remap");
    pte->present = 1; // make this page available
    pte->rw = 1;      // IO pages are writable
    pte->xd = 1;      // we should not execute from IO
    pte->us = 0;      // no userspace
    pte->pwt = 1;     // IO pages should not be cached
    pte->pct = 1;     // Same thing
    pte->address = PTE_GET_PHY_ADDRESS(current_pa);
  }
  return (void *)va;
}

/**
 * Create a pagetable for a program running in userspace.
 * This is done by at first deep copying the kernel pagetable and then mapping
 * the user stuff in the lower addresses. The memory layout is almost as same as
 * https://i.sstatic.net/Ufj7o.png
 *
 * This method does not allocate pages for code, data and heap and only
 * allocates trap pages and the kernel address space. However, a single stack
 * page is allocated.
 */
pagetable_t vmm_create_user_pagetable() {
  // Allocate a pagetable to be our result
  pagetable_t pagetable = (pagetable_t)kalloc();
  if (pagetable == NULL)
    return NULL;
  memset(pagetable, 0, PAGE_SIZE);
  // Copy everything to new pagetable (original kernelspace to userspace)
  if (copy_pagetable(pagetable, kernel_pagetable, 3) != 0)
    return NULL;
  // Create dedicated pages
  void *user_stack = NULL, *int_stack = NULL, *syscall_stack = NULL;
  if ((user_stack = kalloc()) == NULL)
    goto failed;
  if ((int_stack = kalloc()) == NULL)
    goto failed;
  if ((syscall_stack = kalloc()) == NULL)
    goto failed;
  // Map pages
  // TODO: later map trampoline?
  vmm_map_pages(
      pagetable, USER_STACK_BOTTOM, PAGE_SIZE, V2P(user_stack),
      (pte_permissions){.writable = 1, .executable = 0, .userspace = 1});
  vmm_map_pages(
      pagetable, INTSTACK_VIRTUAL_ADDRESS_BOTTOM, PAGE_SIZE, V2P(int_stack),
      (pte_permissions){.writable = 1, .executable = 0, .userspace = 0});
  vmm_map_pages(
      pagetable, SYSCALLSTACK_VIRTUAL_ADDRESS_BOTTOM, PAGE_SIZE,
      V2P(syscall_stack),
      (pte_permissions){.writable = 1, .executable = 0, .userspace = 0});
  // Done
  return pagetable;

failed:
  if (user_stack != NULL)
    kfree(user_stack);
  if (int_stack != NULL)
    kfree(int_stack);
  if (syscall_stack != NULL)
    kfree(syscall_stack);
  return NULL;
}