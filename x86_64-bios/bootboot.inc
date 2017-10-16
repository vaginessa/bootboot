;*
;* loader/x86_64-bios/bootboot.inc
;*
;* Copyright 2017 Public Domain BOOTBOOT bztsrc@github
;*
;* This file is part of the BOOTBOOT Protocol package.
;* @brief The BOOTBOOT structure
;*
; ------ !!! WARNING: MUST MATCH ../bootboot.h !!! ------

bootboot = 8000h





; this define is in the 18th line of bootboot.h
bootboot_MAGIC     equ   'BOOT'

; minimum protocol level:
;  hardcoded kernel name, static kernel memory addresses
PROTOCOL_MINIMAL   equ   0
; static protocol level:
;  kernel name parsed from environment, static kernel memory addresses
PROTOCOL_STATIC    equ   1
; dynamic protocol level:
;  kernel name parsed from environment, kernel memory addresses parsed from ELF symbols
PROTOCOL_DYNAMIC   equ   2

; loader types, just informational
LOADER_BIOS   equ   0
LOADER_UEFI   equ   1
LOADER_RPI    equ   2

; framebuffer pixel format, only 32 bits supported
FB_ARGB       equ 0
FB_RGBA       equ 1
FB_ABGR       equ 2
FB_BGRA       equ 3



;  mmap entry, type is stored in least significant tetrad of size
virtual at 0
    mmap_ent.ptr:         dq	0
    mmap_ent.size:        dq	0
end virtual
; we don't have entry field macros for asm
; realsize = size & 0xFFFFFFFFFFF0
; type = size & 0xF


MMAP_FREE         equ 1
MMAP_RESERVED     equ 2
MMAP_ACPIFREE     equ 3
MMAP_ACPINVS      equ 4
MMAP_MMIO         equ 5

INITRD_MAXSIZE     equ 16 ; Mb

virtual at bootboot
    bootboot.magic:       dd	0
    bootboot.size:        dd	0

    bootboot.protocol_ver:db	1
    bootboot.loader_type: db	0
    bootboot.flags:       dw	0
    bootboot.timezone:    dw	0
    bootboot.fb_type:     dw	0

    bootboot.pagesize:    dd	0
    bootboot.bspid:       dd	0

    bootboot.datetime:    dq	0

    bootboot.initrd_ptr:  dq	0
    bootboot.initrd_size: dq	0

    bootboot.fb_ptr:      dq	0
    bootboot.fb_size:     dd	0
    bootboot.fb_width:    dd	0
    bootboot.fb_height:   dd	0
    bootboot.fb_scanline: dd	0

     ; architecture specific pointers


      bootboot.acpi_ptr:    dq	0
      bootboot.smbi_ptr:    dq	0
      bootboot.efi_ptr:     dq	0
      bootboot.mp_ptr:      dq	0
      bootboot.unused:      dq	0,0,0

     bootboot.mmap:
end virtual




