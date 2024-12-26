# CrowOS

Writing an operating system for x86_64 for fun on top of [Limine](https://github.com/limine-bootloader/limine) boot protocol. Most of the ideas in this OS is from [xv6](https://github.com/mit-pdos/xv6-riscv/).

## TODO/Feature List

- [x] Serial port I/O
- [x] Userspace
    - [x] syscalls
    - [x] Interrupt handler
    - [x] Cooperative scheduling
    - [x] Round robin scheduling
    - [ ] Preemptive scheduling
    - [ ] Shell
- [ ] File System
    - [x] NVMe driver
    - [x] CrowFS file system
    - [ ] File open/closing
    - [ ] File read
    - [ ] File append
    - [ ] File creation
    - [ ] Folder creation
- [ ] CMOS RTC
- [ ] Multiprocessor/SMP
- [ ] VGA/Framebuffer
- [ ] PS2 Keyboard Driver

## Running

At first, you need to clone this project and submodules. This means that you should execute such a command to clone this project:

```bash
git clone --recurse-submodules -j2 https://github.com/HirbodBehnam/CrowOS
```

Next, you only need to run `make qemu` to run the OS under emulation. You can use `make qemu-kvm` in order to run the OS with virtualization.

For debugging, use `make qemu-gdb` and run `gdb` in another window. `gdb` will automatically connect to `qemu` and stop.

### Hardware Limitations

There is a big limitation on the CPU which the OS should run under and that is the support of [`rdtscp`](https://www.felixcloutier.com/x86/rdtscp) instruction and more specifically, IA32_TSC_AUX MSR which is not available on old CPUs. From my testing, this means that it does not support CPUs older than Sandy Bridge (such as Nehalem or Penryn). For this, I run the emulation under the SandyBridge virtual CPU in QEMU.

Other than that, Limine needs at least 128MB of RAM in order to show the boot menu. The OS itself tho should run on lower amounts of RAM.
