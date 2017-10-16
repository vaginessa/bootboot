/*
 * loader/x86_64-bios/mkboot.c
 *
 * Copyright 2017 Public Domain BOOTBOOT bztsrc@github
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief Little tool to install boot.bin in MBR or VBR
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* the BOOTBOOT 1st stage loader code */
extern unsigned char *_binary____boot_bin_start;

/* entry point */
int main(int argc, char** argv)
{
    // variables
    unsigned char bootrec[512], data[512];
    int f, lba=0, lsn, bootlsn=-1;

    // check arguments
    if(argc < 2) {
        printf( "BOOTBOOT mkboot utility - Public Domain bztsrc@github\n\nUsage:\n"
                "  ./mkboot <disk> [partition lba]\n\n"
                "Installs boot record on a disk. Disk can be a local file, a disk or partition\n"
                "device. If you want to install it on a partition, you'll have to specify the\n"
                "starting LBA of that partition as well. Requires that bootboot.bin is already\n"
                "copied on the disk in a contiguous area in order to work.\n\n"
                "Examples:\n"
                "  ./mkboot diskimage.dd      - installing on a disk image\n"
                "  ./mkboot /dev/sda          - installing as MBR\n"
                "  ./mkboot /dev/sda1 123     - installing as VBR\n");
        return 1;
    }
    if(argc > 2 || argv[2]!=NULL) {
        lba = atoi(argv[2]);
    }
    // open file
    f = open(argv[1], O_RDONLY);
    if(f < 0) {
        fprintf(stderr, "mkboot: file not found\n");
        return 2;
    }
    // read the boot record
    if(read(f, data, 512)==-1) {
        close(f);
        fprintf(stderr, "mkboot: unable to read file\n");
        return 2;
    }
    // create the boot record. First copy the code then the data area from disk
    memcpy((void*)&bootrec, (void*)&_binary____boot_bin_start, 512);
    memcpy((void*)&bootrec+0x1B8, (void*)&data+0x1B8, 510-0x1B8);
    // now locate the second stage by magic bytes
    for(lsn = 1; lsn < 1024*1024; lsn++) {
        printf("Checking sector %d\r", lsn);
        if(read(f, data, 512) != -1 &&
            data[0] == 0x55 && data[1] == 0xAA && data[3] == 0xE9 && data[8] == 'B' && data[12] == 'B') {
                bootlsn=lsn;
                break;
        }
    }
    close(f);
    // add bootboot.bin's address to boot record
    if(bootlsn == -1) {
        fprintf(stderr, "mkboot: unable to locate 2nd stage in the first 512 Mbyte\n");
        return 2;
    }
    bootlsn += lba;
    memcpy((void*)&bootrec+0x1B0, (void*)&bootlsn, 4);
    // save boot record
    f = open(argv[1], O_WRONLY);
    if(f < 0 || write(f, bootrec, 512) <= 0) {
        fprintf(stderr, "mkboot: unable to write boot record\n");
        return 3;
    }
    close(f);
    // all went well
    printf("mkboot: BOOTBOOT installed, 2nd stage starts at LBA %d\n", bootlsn);
}
