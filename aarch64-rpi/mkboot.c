/*
 * aarch64-rpi/mkboot.c
 *
 * Copyright 2017 Public Domain BOOTBOOT bztsrc@github
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief Little tool create a RPi compatible MBR for ESP
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* entry point */
int main(int argc, char** argv)
{
    // variables
    unsigned char data[65536];
    unsigned int np,sp,i,es,ee;
    int f;

    // check arguments
    if(argc < 2) {
        printf( "BOOTBOOT mkboot utility - Public Domain bztsrc@github\n\nUsage:\n"
                "  ./mkboot <disk>\n\n"
                "Maps GPT EFI System Partition into MBR so that Raspberry Pi 3\n"
                "firmware can find it's files and boot from it.\n"
                "Examples:\n"
                "  ./mkboot diskimage.dd      - modify a disk image file\n"
                "  ./mkboot /dev/sda          - modify a real disk\n");
        return 1;
    }
    // open file
    f = open(argv[1], O_RDONLY);
    if(f < 0) {
        fprintf(stderr, "mkboot: file not found\n");
        return 2;
    }
    // read the boot record
    if(read(f, data, 65536)==-1) {
        close(f);
        fprintf(stderr, "mkboot: unable to read file\n");
        return 2;
    }
    close(f);
    if(memcmp((void*)&data[512], "EFI PART", 8)) {
        fprintf(stderr, "mkboot: GPT partitioning table not found\n");
        return 2;
    }
    // get number of partitions and size of partition entry
    np=*((unsigned int*)&data[512+80]); sp=*((unsigned int*)&data[512+84]);
    i=*((unsigned int*)&data[512+72])*512; es=ee=0;
    // get esp start and esp end sectors
    while(np--) {
        if(*((unsigned int*)&data[i])==0xC12A7328 && *((unsigned int*)&data[i+4])==0x11D2F81F) {
            es=*((unsigned int*)&data[i+32]); ee=*((unsigned int*)&data[i+40]); break;
        }
    }
    if(es==0 || ee==0 || ee<es+1) {
        fprintf(stderr, "mkboot: ESP not found in GPT\n");
        return 2;
    }
    // if first MBR partition is not a FAT, make space for it
    if(data[0x1C2]!=0xC/*FAT32 LBA*/ && data[0x1C2]!=0xE/*FAT16 LBA*/) {
        memcpy(&data+0x1EE, &data+0x1DE, 16);
        memcpy(&data+0x1DE, &data+0x1CE, 16);
        memcpy(&data+0x1CE, &data+0x1BE, 16);
        data[0x1C2]=0xC;
    }
    // check if it's already pointing to ESP
    ee-=sp-1;
    if(*((unsigned int*)&data[0x1C6])==es && *((unsigned int*)&data[0x1CA])==ee) {
        fprintf(stderr, "mkboot: ESP already mapped to MBR, nothing to do\n");
        return 0;
    }
    *((unsigned int*)&data[0x1C6])=es;
    *((unsigned int*)&data[0x1CA])=ee;

    // save boot record
    f = open(argv[1], O_WRONLY);
    if(f < 0 || write(f, data, 512) <= 0) {
        fprintf(stderr, "mkboot: unable to write MBR\n");
        return 3;
    }
    close(f);
    // all went well
    printf("mkboot: GPT ESP mapped to MBR successfully\n");
}
