BOOTBOOT BIOS / Multiboot Implementation
========================================

See [BOOTBOOT Protocol](https://github.com/bztsrc/bootboot) for common details.

On [BIOS](http://www.scs.stanford.edu/05au-cs240c/lab/specsbbs101.pdf) based systems, the same image can be loaded via
[Multiboot](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html),
chainload from MBR or VBR (GPT hybrid booting) or run as a BIOS Expansion ROM
(so not only the ramdisk can be in ROM, but the loader as well).

Machine state
-------------

IRQs masked. GDT unspecified, but valid, IDT unset. Code is running in supervisor mode in ring 0.

Installation
------------

1. *BIOS disk*: copy __bootboot.bin__ to **_FS0:\BOOTBOOT\LOADER_**. You can place it inside your INITRD partition
        or outside of partition area as well (with `dd conv=notrunc oseek=x`). Finally install __boot.bin__ in the
        master boot record (or in volume boot record if you have a boot manager), saving bootboot.bin's first sector's
        LBA number in a dword at 0x1B0. The [mkboot](https://github.com/bztsrc/bootboot/blob/master/x86_64-bios/mkboot.c)
        utility will do that for you.

2. *BIOS ROM*: install __bootboot.bin__ in a **_BIOS Expansion ROM_**.

3. *GRUB*: specify __bootboot.bin__ as "kernel" in grub.cfg, or you can chainload __boot.bin__.

Limitations
-----------

 - As it boots in protected mode, it only maps the first 4G of RAM.
 - Compressed initrd in ROM is limited to ~96k.
 - The CMOS nvram does not store timezone, so always GMT+0 returned in bootboot.timezone.
