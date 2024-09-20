#pragma once
#ifndef __ASSEMBLER__
#include "limine.h"
#include "mem.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

/**
 * Maximum number of pages which a trampoline can have.
 *
 * I've not sure if there is a good way to do this. I simply hardcoded this
 * thing. There is a check for this in the linker script.
 */
#define TRAMPOLINE_PAGES 10

/**
 * Max virtual address of a memory in a process. This is not The max actual
 * virtual address because Limine puts its stuff in there. So we go a little bit
 * lower because we need to mutally map trampoline to kernel and userspace.
 */
#define VA_MAX (1ULL << 46)
/**
 * Min virutal address of a memory in a process (2MB)
 */
#define VA_MIN (1ULL << 21)

/**
 * Where in the virtual memory we put the code of program in.
 * For now, this is the lowest possible virtual address.
 */
#define USER_CODE_START (VA_MIN)

/**
 * Where we should put the top of the stack in the virtual address space
 */
#define USER_STACK_TOP (1ULL << 31)

/**
 * Where we should put the bottom of the stack in the virtual address space
 */
#define USER_STACK_BOTTOM ((1ULL << 31) - PAGE_SIZE)

/**
 * Where should we put the trampoline in userspace and kernel space.
 */
#define TRAMPOLINE_VIRTUAL_ADDRESS (VA_MAX - TRAMPOLINE_PAGES * PAGE_SIZE)

/**
 * Interrupt stack virtual address. Used when userspace is switching to kernel space
 * to store the interrupt stack. Interrupt stack is one page only.
 */
#define INTSTACK_VIRTUAL_ADDRESS (TRAMPOLINE_VIRTUAL_ADDRESS - PAGE_SIZE)

/**
 * The stack which we can use for syscall of user programs. The first value
 * (top 8 bytes) is the RSP of the user program.
 */
#define SYSCALLSTACK_VIRTUAL_ADDRESS (INTSTACK_VIRTUAL_ADDRESS - PAGE_SIZE)

#ifndef __ASSEMBLER__
/**
 * Some set of PTE permissions
 */
typedef struct {
  // If 1, we can write to this page
  uint8_t writable : 1;
  // If 1, we can execute this page
  uint8_t executable : 1;
  // If 1, this is a userspace page
  uint8_t userspace : 1;
} pte_permissions;

/**
 * Each page table in Intel CPU is like this. Based on
 * https://wiki.osdev.org/Paging and Intel Manual Vol. 3 Table 4-15 and Figure
 * 4-11
 */
struct pte_t {
  // 1 if this PTE is available
  uint64_t present : 1;
  // If 0, writes may not be allowed to the covered memory
  uint64_t rw : 1;
  // User/supervisor; if 0, user-mode accesses are not allowed to the covered
  // memory
  uint64_t us : 1;
  // Page-level write-through. Because we use pages for userspace only, we can
  // se this to zero.
  uint64_t pwt : 1;
  // Page-level cache disable. For the same reason as pwt, we can leave it to
  // zero.
  uint64_t pct : 1;
  // Accessed; indicates whether this entry has been used for linear-address
  // translation.
  uint64_t accessed : 1;
  // Indicates whether software has written to the 4-KByte page referenced by
  // this entry
  uint64_t dirty : 1;
  // If this is a huge page, this will be set to 1.
  uint64_t huge_page : 1;
  // Global page. We don't use them so we set this to zero.
  uint64_t global : 1;
  // Ignored by CPU, can be used by us
  uint64_t ignored2 : 3;
  // The physical address of page or frame. 34-bits is enough for 16 GB of
  // memory. Who the fuck wants to run this shit on a PC with more than 16 GB of
  // memory?
  uint64_t address : 34;
  // Must be zero
  uint64_t reserved2 : 6;
  // Ignored by CPU, can be used by us
  uint64_t ignored3 : 11;
  // Execute-disable
  uint64_t xd : 1;
};

_Static_assert(sizeof(struct pte_t) == 8, "Each PTE must be 8 bytes");

/**
 * Each pagetable contains multiple (or 512 to be exact) PTEs
 */
typedef struct pte_t *pagetable_t;

void vmm_init_kernel(const struct limine_kernel_address_response);
uint64_t vmm_ring3init_frame(void);
int vmm_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size,
                  uint64_t pa, pte_permissions permissions);
int vmm_memcpy_to(pagetable_t pagetable, uint64_t dst, const void *src,
                  size_t n, bool userspace);
pagetable_t vmm_create_user_pagetable();
#endif