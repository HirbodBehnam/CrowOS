# Use bash because why not?
SHELL:=/bin/bash

# We have to types of stuff: Kernel and user programs
K=kernel
U=user
F=$K/CrowFS

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
	-masm=intel \
	-D__CROWOS__ \
	-ggdb -gdwarf-2 \
	-O0 \

# Kernel compiling
KOBJS=$K/init.o \
	$K/common/condvar.o \
	$K/common/lib.o \
	$K/common/printf.o \
	$K/common/spinlock.o \
	$K/cpu/gdt.o \
	$K/cpu/idt.o \
	$K/cpu/isr.o \
	$K/cpu/trap.o \
	$K/cpu/snippets.o \
	$K/device/fb.o \
	$K/device/nvme.o \
	$K/device/pcie.o \
	$K/device/pic.o \
	$K/device/rtc.o \
	$K/device/serial_port.o \
	$K/fs/device.o \
	$K/fs/file.o \
	$K/fs/fs.o \
	$K/fs/syscall.o \
	$K/mem/mem.o \
	$K/mem/vmm.o \
	$K/userspace/exec.o \
	$K/userspace/ring3.o \
	$K/userspace/proc.o \
	$K/userspace/trampoline.o \
	$K/userspace/syscall.o \
	$F/crowfs.o \

LDFLAGS = -m elf_x86_64 \
	-nostdlib \
	-z max-page-size=0x1000 \

# Target specific variables https://stackoverflow.com/a/1305879/4213397
$(KOBJS): CFLAGS+=-Ikernel -I. -mcmodel=kernel
$(KOBJS): ASFLAGS+=-Ikernel -I.

$K/kernel: $(KOBJS) $K/linker.ld
	$(LD) $(LDFLAGS) -T $K/linker.ld -o $@ $(KOBJS) 

$K/cpu/isr.S: $K/cpu/isr.sh
	$K/cpu/isr.sh > $K/cpu/isr.S

# Create the CrowFS interactor to create file system and manipulate
# files via a command line
$F/crowfs: $F/crowfs.c $F/main.c
	gcc -O2 -o $F/crowfs $F/crowfs.c $F/main.c

# Userspace
# Create the syscall assembly file
$U/libc/usyscalls.S: $U/libc/usyscalls.sh
	$U/libc/usyscalls.sh > $U/libc/usyscalls.S
# Define the userspace libraries
ULIB = $U/libc/usyscalls.o $U/libc/ctype.o $U/libc/malloc.o $U/libc/stdio.o $U/libc/stdlib.o $U/libc/string.o
$(ULIB): CFLAGS+=-Iuser -I.
$(ULIB): ASFLAGS+=-Iuser -I.
# User programs
UPROGS=$U/_init \
	$U/_echo \
	$U/_cat \
	$U/_date \
	$U/_bmp \

%.o: CFLAGS+=-Iuser -I.
%.o: ASFLAGS+=-Iuser -I.
# Compile the user programs and the libraries
_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -T $U/linker.ld -o $@ $^

# Creating the bootable image
boot/disk.img: $K/kernel boot/limine.conf boot/BOOTX64.EFI $F/crowfs $(UPROGS)
# Create the image
	rm boot/disk.img || true
	truncate -s 100M boot/disk.img
	sgdisk boot/disk.img -n 1:2048:133119 -t 1:ef00 -n 2:133120 -t 2:8300
# Copy the EFI partition
	mformat -i boot/disk.img@@1M
	mmd -i boot/disk.img@@1M ::/EFI ::/EFI/BOOT
	mcopy -i boot/disk.img@@1M $K/kernel ::/
	mcopy -i boot/disk.img@@1M boot/limine.conf ::/
	mcopy -i boot/disk.img@@1M boot/BOOTX64.EFI ::/EFI/BOOT
# Create the OS partition
	sudo losetup --partscan /dev/loop0 boot/disk.img
	sudo $F/crowfs /dev/loop0p2 new
# Copy the user programs
	for prog in $(UPROGS); do \
		sudo $F/crowfs /dev/loop0p2 copyin $$prog /$$(cut -d _ -f 2 <<< $$prog) ; \
	done
	sudo $F/crowfs /dev/loop0p2 copyin $U/sample.bmp /sample.bmp
# Unmount the OS partition
	sudo losetup -d /dev/loop0

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
	$(QEMU) -cpu SandyBridge $(QEMUOPT)

.PHONY: qemu-kvm
qemu-kvm: boot/disk.img
	$(QEMU) -enable-kvm -cpu host $(QEMUOPT)

.PHONY: qemu-gdb
qemu-gdb: boot/disk.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) -cpu SandyBridge $(QEMUOPT) -s -S

.PHONY: qemu-kvm-gdb
qemu-kvm-gdb: boot/disk.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) -enable-kvm -cpu host $(QEMUOPT) -s -S

.PHONY: clean
clean:
	rm -f $K/kernel $K/*.o $K/**/*.o $K/cpu/isr.S $F/crowfs $U/*.o $U/**/*.o $U/_* $U/usyscalls.S boot/disk.img
