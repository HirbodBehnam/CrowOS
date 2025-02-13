# CrowOS

Writing an operating system for x86_64 for fun on top of [Limine](https://github.com/limine-bootloader/limine) boot protocol. Most of the ideas in this OS is from [xv6](https://github.com/mit-pdos/xv6-riscv/).

## TODO/Feature List

- [x] Serial port I/O
- [x] Userspace
    - [x] syscalls
    - [x] Interrupt handler
    - [x] Cooperative scheduling
    - [x] Round robin scheduling
    - [x] Wait
    - [x] Shell
    - [ ] Floating Point
        - [ ] 80387
        - [ ] SSE
        - [ ] AVX
    - [ ] Preemptive scheduling
- [x] File System
    - [x] NVMe driver
    - [x] CrowFS file system
    - [x] Device Files
    - [x] File open/closing
    - [x] File read
    - [x] File append
    - [x] File creation
    - [x] Folder creation
    - [x] File removal
    - [ ] Rename
    - [x] Relative paths
- [x] CMOS RTC
- [x] VGA/Framebuffer
    - [x] BMP viewer
    - [ ] Framebuffer console
- [ ] PS2 Keyboard Driver
- [ ] Multiprocessor/SMP
- [x] DOOM

## Running

At first, you need to clone this project and submodules. This means that you should execute such a command to clone this project:

```bash
git clone --recurse-submodules -j2 https://github.com/HirbodBehnam/CrowOS
```

For the build dependencies, you can use the following command in Ubuntu to get everything:

```bash
apt install qemu-system-x86 build-essential gdisk git mtools
```

Next, you only need to run `make qemu` to run the OS under emulation. You can use `make qemu-kvm` in order to run the OS with virtualization.

For debugging, use `make qemu-gdb` and run `gdb` in another window. `gdb` will automatically connect to `qemu` and stop.

### Hardware Limitations

Limine needs at least 128MB of RAM in order to show the boot menu. The OS itself tho should run on lower amounts of RAM. For the CPU, it should work on almost all 64bit Intel Core CPUs. I tested it with Penryn CPUs under QEMU and it looks like that it is working fine.
