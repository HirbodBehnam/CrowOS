#include "exec.h"
#include "cpu/asm.h"
#include "fs/fs.h"
#include "mem/mem.h"
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
    // TODO: load the segments in the memory
  }
  // Setup the context of the new process by switching to its address
  // space temporary
  install_pagetable(V2P(proc->pagetable));

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
  install_pagetable(current_pagetable); // switch back to page table before
  return -1;
}