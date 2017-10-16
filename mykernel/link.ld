/*
 * loader/mykernel/link.ld
 *
 * Copyright 2017 Public Domain BOOTBOOT bztsrc@github
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief An example linker script for sample kernel
 *
 */

CORE_ADDRESS = 0xffffffffffe00000;

ENTRY(_start)
/* make the loadable segment contain the file headers */
PHDRS
{
  text PT_LOAD FILEHDR PHDRS FLAGS(5);
}
SECTIONS
{
    /* bootboot.fb_ptr holds the physical address, mapped here */
    . = 0xfffffffffc000000;
        fb = .;

    /* map bootboot and environment right before text segment */
    . = CORE_ADDRESS;
        bootboot = .;
        . += 4096;
        environment = .;
        . += 4096;

    /* rest of your code, from the C source, in one loadable segment */
    .text CORE_ADDRESS + 8192 + SIZEOF_HEADERS : AT(ADDR(.text) - CORE_ADDRESS - 8192 + SIZEOF_HEADERS)
    {
        _code = .;
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
        _data = .;
        *(.data)
        . = ALIGN(4096);
    } :text
    .bss (NOLOAD):
    {
        __bss_start = .;
        *(.bss .bbs.*)
        *(COMMON)
        __bss_end = .;
    }
    _end = .;

   /DISCARD/ :
   {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
   }
}
