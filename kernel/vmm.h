#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Max virtual address of a memory in a process
 */
#define VA_MAX (1ULL << 47)
/**
 * Min virutal address of a memory in a process (2MB)
 */
#define VA_MIN (1ULL << 21)

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
  // Ignored by CPU, can be used by US
  uint64_t ignored1 : 1;
  // Must be zero
  uint64_t reserved1 : 1;
  // Ignored by CPU, can be used by US
  uint64_t ignored2 : 4;
  // The physical address of page or frame. 34-bits is enough for 16 GB of
  // memory. Who the fuck wants to run this shit on a PC with more than 16 GB of
  // memory?
  uint64_t address : 34;
  // Must be zero
  uint64_t reserved2 : 6;
  // Ignored by CPU, can be used by US
  uint64_t ignored3 : 11;
  // Execute-disable
  uint64_t xd : 1;
};

_Static_assert(sizeof(struct pte_t) == 8, "Each PTE must be 8 bytes");

/**
 * Each pagetable contains multiple (or 512 to be exact) PTEs
 */
typedef struct pte_t *pagetable_t;

int vmm_map_pages(pagetable_t pagetable, uint64_t va, uint64_t size,
                  uint64_t pa, pte_permissions permissions);
pagetable_t vmm_create_pagetable(void);