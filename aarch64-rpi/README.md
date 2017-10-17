BOOTBOOT Raspberry Pi 3 Implementation
======================================

See [BOOTBOOT Protocol](https://github.com/bztsrc/bootboot) for common details.

On [Raspberry Pi 3](https://www.raspberrypi.org/documentation/hardware/raspberrypi/bootmodes/sdcard.md) board the bootboot.img
is loaded from the boot partition on SD card as kernel8.img by start.elf.

Machine state
-------------

In addition to standard mappings, the MMIO is also mapped in kernel space:

```
   -96M         MMIO      (0xFFFFFFFFFA000000)
```

Code is running in supervisor mode, at EL1. Dummy exception handlers are installed, but your kernel should use it's own
handlers as soon as possible.

File system drivers
-------------------

For boot partition, RPi3 version expect *defragmented* FAT16 or FAT32 file systems (if the
initrd is a file and does not occupy the whole boot partition).

Gzip compression is not recommended as reading from SD card is considerably faster than uncompressing.

Installation
------------

1. Copy __bootboot.img__ to **_FS0:\KERNEL8.IMG_**.

2. You'll need other [firmware files](https://github.com/raspberrypi/firmware/tree/master/boot) as well.

3. If you have used a GPT disk with ESP as boot partition, then you need to map it in MBR so that Raspberry Pi
    firmware could find those files. The [mkboot](https://github.com/bztsrc/bootboot/blob/master/aarch64-rpi/mkboot.c)
    utility will do that for you.

Limitations
-----------

 - Initrd in ROM is not possible
 - Maps only the first 1G of RAM.
 - Cards other than SDHC Class 10 not supported.
