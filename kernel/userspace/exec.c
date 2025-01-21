#include "exec.h"
#include "common/lib.h"
#include "common/printf.h"
#include "device/serial_port.h"
#include "fs/device.h"
#include "fs/fs.h"
#include "include/exec.h"
#include "include/file.h"
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
                     flags2perm(ph.flags), false) == -1)
      goto bad;
    if (load_segment(proc->pagetable, proc_inode, ph.vaddr, ph.off, ph.filesz) <
        0)
      goto bad;
    proc->initial_data_segment = MAX_SAFE(proc->initial_data_segment, ph.vaddr + PAGE_ROUND_UP(ph.memsz));
  }
  // Write the arguments to the user stack
  uint64_t rsp = USER_STACK_TOP;
  uint64_t argument_pointers[MAX_ARGV] = {0};
  int argc = 0;
  if (args != NULL) {
    for (; argc < MAX_ARGV && args[argc] != NULL; argc++) {
      size_t argument_length = strlen(args[argc]);
      rsp -= argument_length + 1; // len of the string push the null terminator
      vmm_memcpy(proc->pagetable, rsp, args[argc], argument_length + 1, true);
      argument_pointers[argc] = rsp;
    }
  }
  rsp -= 8;
  // Null terminator of the argv
  const uint64_t zero = 0;
  vmm_memcpy(proc->pagetable, rsp, &zero, sizeof(uint64_t), true);
  for (int i = argc - 1; i >= 0; i--) {
    rsp -= 8;
    vmm_memcpy(proc->pagetable, rsp, &argument_pointers[i], sizeof(uint64_t),
               true);
  }
  uint64_t argv = rsp;
  rsp -= rsp % 16 + 8; // Stack alignment

  // Write the initial context to the interrupt stack
  const struct process_context initial_context = {
      .return_address = (uint64_t)jump_to_ring3,
      .r12 = (uint64_t)argc,
      .r13 = argv,
      .r14 = rsp,       // Initial stack pointer / argv
      .r15 = elf.entry, // _start of the program
  };
  vmm_memcpy(proc->pagetable,
             INTSTACK_VIRTUAL_ADDRESS_TOP - sizeof(struct process_context),
             &initial_context, sizeof(initial_context), false);
  proc->resume_stack_pointer =
      INTSTACK_VIRTUAL_ADDRESS_TOP - sizeof(struct process_context);

  // Open stdin, stdout, stderr
  // Because all of them are just serial port, we can assign
  // the serial port device index to all of them.
  int serial_device_index = device_index(SERIAL_DEVICE_NAME);
  if (serial_device_index == -1)
    panic("exec: no serial");
  proc->open_files[DEFAULT_STDIN].type = FD_DEVICE;
  proc->open_files[DEFAULT_STDIN].structures.device = serial_device_index;
  proc->open_files[DEFAULT_STDIN].offset = 0;
  proc->open_files[DEFAULT_STDIN].readble = true;
  proc->open_files[DEFAULT_STDIN].writable = false;
  proc->open_files[DEFAULT_STDOUT].type = FD_DEVICE;
  proc->open_files[DEFAULT_STDOUT].structures.device = serial_device_index;
  proc->open_files[DEFAULT_STDOUT].offset = 0;
  proc->open_files[DEFAULT_STDOUT].readble = false;
  proc->open_files[DEFAULT_STDOUT].writable = true;
  proc->open_files[DEFAULT_STDERR].type = FD_DEVICE;
  proc->open_files[DEFAULT_STDERR].structures.device = serial_device_index;
  proc->open_files[DEFAULT_STDERR].offset = 0;
  proc->open_files[DEFAULT_STDERR].readble = false;
  proc->open_files[DEFAULT_STDERR].writable = true;

  // Setup the values for the sbrk syscall
  proc->initial_data_segment = PAGE_ROUND_UP(proc->initial_data_segment);
  proc->current_sbrk = proc->initial_data_segment;

  // We are fucking done!
  fs_close(proc_inode);
  proc->state = RUNNABLE; // now we can run this!
  return proc->pid;
bad:
  if (proc_inode != NULL) // close the file
    fs_close(proc_inode);
  if (proc != NULL) {
    vmm_user_pagetable_free(proc->pagetable);
    proc->state = UNUSED;
    proc->pid = 0;
  }
  return -1;
}