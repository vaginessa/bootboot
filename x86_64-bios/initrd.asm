;*
;* loader/x86_64-bios/initrd.asm
;*
;* Copyright 2017 Public Domain BOOTBOOT bztsrc@github
;*
;* This file is part of the BOOTBOOT Protocol package.
;* @brief Option ROM for initrd. mkfs utility also can do this.
;*
;* This may seem odd, but works with BIOS (up to 128k gzipped
;* initrd), and makes much more sense on EFI, where Option ROMs
;* can be 16M in size.
;*

;------------header------------
            USE16
            ORG         0h
rom:        db          55h,0AAh                ;ROM magic
            db          (rom_end-rom)/512       ;size in 512 blocks
.executor:  xor         ax, ax                  ;entry point
            retf
.checksum:  dw          0                       ;checksum
.name:      db          "INITRD"
            db          0,0
            dd          initrd_end-initrd, 0
            db          0,0
.pnpptr:    dw          0
.flags:     dd          0
;------------data------------
initrd:
            file        "../../bin/ESP/BOOTBOOT/INITRD"
initrd_end:
;-----------padding to be multiple of 512----------
            db          (511-($-rom+511) mod 512) dup 0
rom_end:

;-----------BIOS checksum------------
chksum = 0
repeat $-rom
    load b byte from (rom+%-1)
    chksum = (chksum + b) mod 100h
end repeat
store byte (100h-chksum) at (rom.checksum)
