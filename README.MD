# CrowOS

Writing an operating system for x86_64 for fun on top of [Limine](https://github.com/limine-bootloader/limine) boot protocol. Most of the ideas in this OS is from [xv6](https://github.com/mit-pdos/xv6-riscv/).

## TODO List

- [x] Serial port I/O
- [x] Userspace
    - [x] syscalls
    - [x] Interrupt handler
    - [x] Cooperative scheduling
    - [x] Round robin scheduling
    - [ ] Preemptive scheduling
    - [ ] Shell
- [x] NVMe driver
    - [ ] CrowFS file system
- [ ] Multiprocessor/SMP
- [ ] VGA/Framebuffer
- [ ] PS2 Keyboard Driver

## Running

You only need to run `make qemu` to run the OS under emulation. You can use `make qemu-kvm` in order to run the OS with virtualization.

For debugging, use `make qemu-gdb` and run `gdb` in another window. `gdb` will automatically connect to `qemu` and stop.