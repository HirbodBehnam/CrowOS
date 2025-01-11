#include "exec.h"
#include "common/printf.h"
#include "cpu/asm.h"
#include "fs/fs.h"
#include "mem/mem.h"
#include "mem/vmm.h"
#include "userspace/proc.h"

#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little endian

// File header
struct ElfHeader {
  uint32_t magic; // must equal ELF_MAGIC
  uint8_t elf[12];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};

// Program section header
struct ProgramHeader {
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD 1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

// defined in ring3.S
extern void jump_to_ring3(void);

static pte_permissions flags2perm(int flags) {
  // Always userspace
  pte_permissions perm = {.executable = 0, .userspace = 1, .writable = 0};
  if (flags & 0x1)
    perm.executable = 1;
  if (flags & 0x2)
    perm.writable = 1;
  return perm;
}

/**
 * Loads a segment from the ELF file to the memory. The pages of the ELF file
 * must be already allocated.
 */
static int load_segment(pagetable_t pagetable, struct fs_inode *ip, uint64_t va,
                        uint32_t offset, uint32_t sz) {
  /**
   * Note: You might wonder: Why we get the physical address instead of just
   * using the virtual address if the page table is mapped? Well, in some cases
   * (like .rodata or .text) the MMU maps that section as read only. Thus, we
   * cannot write to that data. Instead, we can write to the physical address of
   * the frame and that works just fine.
   */
  for (uint32_t i = 0; i < sz; i += PAGE_SIZE) {
    uint64_t phyiscal_address = vmm_walkaddr(pagetable, va + i, true);
    if (phyiscal_address == 0)
      panic("load_segment: address should exist");
    uint64_t n;
    if (sz - i < PAGE_SIZE)
      n = sz - i;
    else
      n = PAGE_SIZE;
    if (fs_read(ip, (char *)P2V(phyiscal_address), n, offset + i) != (int)n)
      return -1;
  }
  return 0;
}

/**
 * Creates a new process as the child of the running process.
 *
 * Returns the new PID as the result if successful. Otherwise returns
 * -1 which is an error.
 */
uint64_t proc_exec(const char *path, const char *args[]) {
  (void)args;
  struct ElfHeader elf;
  struct ProgramHeader ph;
  struct process *proc = NULL;
  struct fs_inode *proc_inode = NULL;
  uint64_t current_pagetable = get_installed_pagetable();
  // Open the file
  proc_inode = fs_open(path, 0);
  if (proc_inode == NULL) // not found?
    goto bad;
  // Read the elf header
  if (fs_read(proc_inode, (char *)&elf, sizeof(elf), 0) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;
  // Allocate a new process
  proc = proc_allocate();
  if (proc == NULL)
    goto bad;
  // Read program section
  for (uint64_t i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (fs_read(proc_inode, (char *)&ph, sizeof(ph), off) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if (ph.vaddr % PAGE_SIZE != 0)
      goto bad;
    // Load the segments in the memory
    if (vmm_allocate(proc->pagetable, ph.vaddr, PAGE_ROUND_UP(ph.memsz),
                     flags2perm(ph.flags)) == -1)
      goto bad;
    // Probably no need to flush the TLB
    if (load_segment(proc->pagetable, proc_inode, ph.vaddr, ph.off, ph.filesz) <
        0)
      goto bad;
  }
  // Setup the context of the new process by switching to its address
  // space temporary
  install_pagetable(V2P(proc->pagetable));
  // Write the arguments to the user stack
  // TODO:

  // Write the initial context to the interrupt stack
  *(struct process_context *)(INTSTACK_VIRTUAL_ADDRESS_TOP -
                              sizeof(struct process_context)) =
      (struct process_context){
          .return_address = (uint64_t)jump_to_ring3,
          .r14 = USER_STACK_TOP, // TODO: Add the arguments as well
          .r15 = elf.entry,      // _start of the program
      };
  proc->resume_stack_pointer =
      INTSTACK_VIRTUAL_ADDRESS_TOP - sizeof(struct process_context);

  // We are fucking done!
  install_pagetable(current_pagetable); // switch back to page table before
  fs_close(proc_inode);
  proc->state = RUNNABLE; // now we can run this!
  return proc->pid;
bad:
  if (proc_inode != NULL) // close the file
    fs_close(proc_inode);
  if (proc != NULL) {
    proc->state = UNUSED;
    // TODO: The page table?
  }
  // TODO: unmap/deallocate the pages allocated
  install_pagetable(current_pagetable); // switch back to page table before
  return -1;
}