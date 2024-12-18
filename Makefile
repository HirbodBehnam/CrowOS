# We have to types of stuff: Kernel and user programs
K=kernel
U=user

# Define the compilers
CC = gcc
LD = ld
AS = as

# Compiler flags for kernel
CFLAGS = -Wall \
	-Wextra \
	-Werror \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-lto \
	-fno-PIC \
	-m64 \
	-march=x86-64 \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-mno-red-zone \
	-mcmodel=kernel \
	-masm=intel
CFLAGS += -ggdb -gdwarf-2 -O0

KLDFLAGS = -m elf_x86_64 \
	-nostdlib \
	-static \
	-z max-page-size=0x1000

# Kernel compiling
OBJS=$K/init.o \
	$K/lib.o \
	$K/snippets.o \
	$K/gdt.o \
	$K/idt.o \
	$K/serial_port.o \
	$K/printf.o \
	$K/mem.o \
	$K/pic.o \
	$K/ring3.o \
	$K/vmm.o \
	$K/proc.o \
	$K/trampoline.o \
	$K/trap.o \
	$K/syscall.o \
	$K/isr.o \
	$K/spinlock.o \
	$K/sleeplock.o \
	$K/pcie.o \
	$K/nvme.o \

$K/kernel: $(OBJS) $K/linker.ld
	$(LD) $(KLDFLAGS) -T $K/linker.ld -o $K/kernel $(OBJS) 

$K/isr.S: $K/isr.sh
	./$K/isr.sh > $K/isr.S

# Creating the bootable image
boot/disk.img: $K/kernel boot/limine.conf boot/BOOTX64.EFI
	rm boot/disk.img || true
	truncate -s 100M boot/disk.img
	sgdisk boot/disk.img -n 1:2048 -t 1:ef00
	mformat -i boot/disk.img@@1M
	mmd -i boot/disk.img@@1M ::/EFI ::/EFI/BOOT
	mcopy -i boot/disk.img@@1M $K/kernel ::/
	mcopy -i boot/disk.img@@1M boot/limine.conf ::/
	mcopy -i boot/disk.img@@1M boot/BOOTX64.EFI ::/EFI/BOOT

# Emulation
QEMU=qemu-system-x86_64
# Do not add KVM here or you are unable to debug the OS
QEMUOPT = -M q35 -smp 1 -m 128M -bios /usr/share/ovmf/OVMF.fd
QEMUOPT += -serial mon:stdio
QEMUOPT += -monitor telnet:127.0.0.1:38592,server,nowait
QEMUOPT += -drive file=boot/disk.img,if=none,id=nvm,format=raw -device nvme,serial=deadbeef,drive=nvm
#QEMUOPT += -d int,cpu_reset

.PHONY: qemu
qemu: boot/disk.img
	$(QEMU) -cpu Haswell $(QEMUOPT)

.PHONY: qemu-kvm
qemu-kvm: boot/disk.img
	$(QEMU) -enable-kvm -cpu host $(QEMUOPT)

.PHONY: qemu-gdb
qemu-gdb: boot/disk.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) -cpu Haswell $(QEMUOPT) -s -S

.PHONY: qemu-kvm-gdb
qemu-kvm-gdb: boot/disk.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) -enable-kvm -cpu host $(QEMUOPT) -s -S

.PHONY: clean
clean:
	rm -f $K/kernel $K/*.o $K/isr.S boot/disk.img
