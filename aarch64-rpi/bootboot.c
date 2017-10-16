/*
 * loader/aarch64-rpi/bootboot.c
 *
 * Copyright 2017 Public Domain BOOTBOOT bztsrc@github
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief Boot loader for the Raspberry Pi 3+ ARMv8
 *
 */
#define DEBUG 0
//#define SD_DEBUG DEBUG
//#define INITRD_DEBUG DEBUG
//#define MEM_DEBUG DEBUG

#define NULL ((void*)0)
#define PAGESIZE 4096

/* we don't have stdint.h */
typedef signed char         int8_t;
typedef short int           int16_t;
typedef int                 int32_t;
typedef long int            int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short int  uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long int   uint64_t;

#include "tinf.h"

/* get BOOTBOOT structure */
#include "../bootboot.h"

/* import function from boot.S */
extern void jumptokernel(uint64_t pc);

/* aligned buffers */
volatile uint32_t  __attribute__((aligned(16))) mbox[32];
/* we place these manually in linker script, gcc would otherwise waste lots of memory */
volatile uint8_t __attribute__((aligned(PAGESIZE))) __bootboot[PAGESIZE];
volatile uint8_t __attribute__((aligned(PAGESIZE))) __environment[PAGESIZE];
volatile uint8_t __attribute__((aligned(PAGESIZE))) __paging[9*PAGESIZE];
#define __diskbuf __paging
extern volatile uint8_t _end;

/* forward definitions */
void puts(char *s);

/*** ELF64 defines and structs ***/
#define ELFMAG      "\177ELF"
#define SELFMAG     4
#define EI_CLASS    4       /* File class byte index */
#define ELFCLASS64  2       /* 64-bit objects */
#define EI_DATA     5       /* Data encoding byte index */
#define ELFDATA2LSB 1       /* 2's complement, little endian */
#define PT_LOAD     1       /* Loadable program segment */
#define EM_AARCH64  183     /* ARM aarch64 architecture */

typedef struct
{
  unsigned char e_ident[16];/* Magic number and other info */
  uint16_t    e_type;         /* Object file type */
  uint16_t    e_machine;      /* Architecture */
  uint32_t    e_version;      /* Object file version */
  uint64_t    e_entry;        /* Entry point virtual address */
  uint64_t    e_phoff;        /* Program header table file offset */
  uint64_t    e_shoff;        /* Section header table file offset */
  uint32_t    e_flags;        /* Processor-specific flags */
  uint16_t    e_ehsize;       /* ELF header size in bytes */
  uint16_t    e_phentsize;    /* Program header table entry size */
  uint16_t    e_phnum;        /* Program header table entry count */
  uint16_t    e_shentsize;    /* Section header table entry size */
  uint16_t    e_shnum;        /* Section header table entry count */
  uint16_t    e_shstrndx;     /* Section header string table index */
} Elf64_Ehdr;

typedef struct
{
  uint32_t    p_type;         /* Segment type */
  uint32_t    p_flags;        /* Segment flags */
  uint64_t    p_offset;       /* Segment file offset */
  uint64_t    p_vaddr;        /* Segment virtual address */
  uint64_t    p_paddr;        /* Segment physical address */
  uint64_t    p_filesz;       /* Segment size in file */
  uint64_t    p_memsz;        /* Segment size in memory */
  uint64_t    p_align;        /* Segment alignment */
} Elf64_Phdr;

/*** PE32+ defines and structs ***/
#define MZ_MAGIC                    0x5a4d      /* "MZ" */
#define PE_MAGIC                    0x00004550  /* "PE\0\0" */
#define IMAGE_FILE_MACHINE_ARM64    0xaa64      /* ARM aarch64 architecture */
#define PE_OPT_MAGIC_PE32PLUS       0x020b      /* PE32+ format */
typedef struct
{
  uint16_t magic;         /* MZ magic */
  uint16_t reserved[29];  /* reserved */
  uint32_t peaddr;        /* address of pe header */
} mz_hdr;

typedef struct {
  uint32_t magic;         /* PE magic */
  uint16_t machine;       /* machine type */
  uint16_t sections;      /* number of sections */
  uint32_t timestamp;     /* time_t */
  uint32_t sym_table;     /* symbol table offset */
  uint32_t symbols;       /* number of symbols */
  uint16_t opt_hdr_size;  /* size of optional header */
  uint16_t flags;         /* flags */
  uint16_t file_type;     /* file type, PE32PLUS magic */
  uint8_t  ld_major;      /* linker major version */
  uint8_t  ld_minor;      /* linker minor version */
  uint32_t text_size;     /* size of text section(s) */
  uint32_t data_size;     /* size of data section(s) */
  uint32_t bss_size;      /* size of bss section(s) */
  uint32_t entry_point;   /* file offset of entry point */
  uint32_t code_base;     /* relative code addr in ram */
} pe_hdr;

/*** Raspberry Pi specific defines ***/
#define CLOCKHZ         1000000

#define MMIO_BASE       0x3F000000
#define STMR_L          ((volatile uint32_t*)(MMIO_BASE+0x00003004))
#define STMR_H          ((volatile uint32_t*)(MMIO_BASE+0x00003008))
#define STMR_C3         ((volatile uint32_t*)(MMIO_BASE+0x00003018))

#define ARM_TIMER_CTL   ((volatile uint32_t*)(MMIO_BASE+0x0000B408))
#define ARM_TIMER_CNT   ((volatile uint32_t*)(MMIO_BASE+0x0000B420))

#define PM_RTSC         ((volatile uint32_t*)(MMIO_BASE+0x0010001c))
#define PM_WATCHDOG     ((volatile uint32_t*)(MMIO_BASE+0x00100024))
#define PM_WDOG_MAGIC   0x5a000000
#define PM_RTSC_FULLRST 0x00000020

#define GPFSEL0         ((volatile uint32_t*)(MMIO_BASE+0x00200000))
#define GPFSEL1         ((volatile uint32_t*)(MMIO_BASE+0x00200004))
#define GPFSEL2         ((volatile uint32_t*)(MMIO_BASE+0x00200008))
#define GPFSEL3         ((volatile uint32_t*)(MMIO_BASE+0x0020000C))
#define GPFSEL4         ((volatile uint32_t*)(MMIO_BASE+0x00200010))
#define GPFSEL5         ((volatile uint32_t*)(MMIO_BASE+0x00200014))
#define GPSET0          ((volatile uint32_t*)(MMIO_BASE+0x0020001C))
#define GPSET1          ((volatile uint32_t*)(MMIO_BASE+0x00200020))
#define GPCLR0          ((volatile uint32_t*)(MMIO_BASE+0x00200028))
#define GPLEV0          ((volatile uint32_t*)(MMIO_BASE+0x00200034))
#define GPLEV1          ((volatile uint32_t*)(MMIO_BASE+0x00200038))
#define GPEDS0          ((volatile uint32_t*)(MMIO_BASE+0x00200040))
#define GPEDS1          ((volatile uint32_t*)(MMIO_BASE+0x00200044))
#define GPHEN0          ((volatile uint32_t*)(MMIO_BASE+0x00200064))
#define GPHEN1          ((volatile uint32_t*)(MMIO_BASE+0x00200068))
#define GPPUD           ((volatile uint32_t*)(MMIO_BASE+0x00200094))
#define GPPUDCLK0       ((volatile uint32_t*)(MMIO_BASE+0x00200098))
#define GPPUDCLK1       ((volatile uint32_t*)(MMIO_BASE+0x0020009C))

#define AUX_ENABLE      ((volatile uint32_t*)(MMIO_BASE+0x00215004))
#define AUX_MU_IO       ((volatile uint32_t*)(MMIO_BASE+0x00215040))
#define AUX_MU_IER      ((volatile uint32_t*)(MMIO_BASE+0x00215044))
#define AUX_MU_IIR      ((volatile uint32_t*)(MMIO_BASE+0x00215048))
#define AUX_MU_LCR      ((volatile uint32_t*)(MMIO_BASE+0x0021504C))
#define AUX_MU_MCR      ((volatile uint32_t*)(MMIO_BASE+0x00215050))
#define AUX_MU_LSR      ((volatile uint32_t*)(MMIO_BASE+0x00215054))
#define AUX_MU_MSR      ((volatile uint32_t*)(MMIO_BASE+0x00215058))
#define AUX_MU_SCRATCH  ((volatile uint32_t*)(MMIO_BASE+0x0021505C))
#define AUX_MU_CNTL     ((volatile uint32_t*)(MMIO_BASE+0x00215060))
#define AUX_MU_STAT     ((volatile uint32_t*)(MMIO_BASE+0x00215064))
#define AUX_MU_BAUD     ((volatile uint32_t*)(MMIO_BASE+0x00215068))
// qemu hack to see serial output
#define UART0_DR        ((volatile uint32_t*)(MMIO_BASE+0x00201000))
#define UART0_CR        ((volatile uint32_t*)(MMIO_BASE+0x00201030))

/* timing stuff */
uint64_t getmicro(void){uint32_t h=*STMR_H,l=*STMR_L;if(h!=*STMR_H){h=*STMR_H;l=*STMR_L;} return(((uint64_t)h)<<32)|l;}
/* delay cnt clockcycles */
void delay(uint32_t cnt) { while(cnt--) { asm volatile("nop"); } }
/* delay cnt microsec */
void delaym(uint32_t cnt) {uint64_t t=getmicro();cnt+=t;while(getmicro()<cnt); }

/* UART stuff */
void uart_send(uint32_t c) { do{asm volatile("nop");}while(!(*AUX_MU_LSR&0x20)); *AUX_MU_IO=c; *UART0_DR=c; }
char uart_getc() {char r;do{asm volatile("nop");}while(!(*AUX_MU_LSR&0x01));r=(char)(*AUX_MU_IO);return r=='\r'?'\n':r;}
void uart_hex(uint64_t d,int c) { uint32_t n;c<<=3;c-=4;for(;c>=0;c-=4){n=(d>>c)&0xF;n+=n>9?0x37:0x30;uart_send(n);} }
void uart_putc(char c) { if(c=='\n') uart_send((uint32_t)'\r'); uart_send((uint32_t)c); }
void uart_puts(char *s) { while(*s) uart_putc(*s++); }
void uart_dump(void *ptr,uint32_t l) {
    uint64_t a,b;
    unsigned char c;
    for(a=(uint64_t)ptr;a<(uint64_t)ptr+l*16;a+=16) {
        uart_hex(a,8); uart_puts(": ");
        for(b=0;b<16;b++) {
            uart_hex(*((unsigned char*)(a+b)),1);
            uart_putc(' ');
            if(b%4==3)
                uart_putc(' ');
        }
        for(b=0;b<16;b++) {
            c=*((unsigned char*)(a+b));
            uart_putc(c<32||c>=127?'.':c);
        }
        uart_putc('\n');
    }
}
void uart_exc(uint64_t idx, uint64_t esr, uint64_t elr, uint64_t spsr, uint64_t far)
{
    uint32_t r;
    puts("\nBOOTBOOT-EXCEPTION");
    uart_puts(" #");
    uart_hex(idx,1);
    uart_puts(":\n  ESR_EL1 ");
    uart_hex(esr,8);
    uart_puts(" ELR_EL1 ");
    uart_hex(elr,8);
    uart_puts("\n SPSR_EL1 ");
    uart_hex(spsr,8);
    uart_puts(" FAR_EL1 ");
    uart_hex(far,8);
    uart_putc('\n');
    while(r!='\n' && r!=' ') r=uart_getc();
    uart_puts("\n\n"); delaym(1000);
    *PM_WATCHDOG = PM_WDOG_MAGIC | 1;
    *PM_RTSC = PM_WDOG_MAGIC | PM_RTSC_FULLRST;
    while(1);
}
#define VIDEOCORE_MBOX  (MMIO_BASE+0x0000B880)
#define MBOX_READ       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x0))
#define MBOX_POLL       ((volatile uint32_t*)(VIDEOCORE_MBOX+0x10))
#define MBOX_SENDER     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x14))
#define MBOX_STATUS     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x18))
#define MBOX_CONFIG     ((volatile uint32_t*)(VIDEOCORE_MBOX+0x1C))
#define MBOX_WRITE      ((volatile uint32_t*)(VIDEOCORE_MBOX+0x20))
#define MBOX_REQUEST    0
#define MBOX_RESPONSE   0x80000000
#define MBOX_FULL       0x80000000
#define MBOX_EMPTY      0x40000000
#define MBOX_CH_POWER   0
#define MBOX_CH_FB      1
#define MBOX_CH_VUART   2
#define MBOX_CH_VCHIQ   3
#define MBOX_CH_LEDS    4
#define MBOX_CH_BTNS    5
#define MBOX_CH_TOUCH   6
#define MBOX_CH_COUNT   7
#define MBOX_CH_PROP    8

/* mailbox functions */
void mbox_write(uint8_t ch, volatile uint32_t *mbox)
{
    do{asm volatile("nop");}while(*MBOX_STATUS & MBOX_FULL);
    *MBOX_WRITE = (((uint32_t)((uint64_t)mbox)&~0xF) | (ch&0xF));
}
uint32_t mbox_read(uint8_t ch)
{
    uint32_t r;
    while(1) {
        do{asm volatile("nop");}while(*MBOX_STATUS & MBOX_EMPTY);
        r=*MBOX_READ;
        if((uint8_t)(r&0xF)==ch)
            return (r&~0xF);
    }
}
uint8_t mbox_call(uint8_t ch, volatile uint32_t *mbox)
{
    mbox_write(ch,mbox);
    return mbox_read(ch)==(uint32_t)((uint64_t)mbox) && mbox[1]==MBOX_RESPONSE;
}

/* string.h */
uint32_t strlen(unsigned char *s) { uint32_t n=0; while(*s++) n++; return n; }
void memcpy(void *dst, void *src, uint32_t n){uint8_t *a=dst,*b=src;while(n--) *a++=*b++; }
int memcmp(void *s1, void *s2, uint32_t n){uint8_t *a=s1,*b=s2;while(n--){if(*a!=*b){return *a-*b;}a++;b++;} return 0; }
/* other string functions */
int atoi(unsigned char *c) { int r=0;while(*c>='0'&&*c<='9') {r*=10;r+=*c++-'0';} return r; }
int oct2bin(unsigned char *s, int n){ int r=0;while(n-->0){r<<=3;r+=*s++-'0';} return r; }
int hex2bin(unsigned char *s, int n){ int r=0;while(n-->0){r<<=4;
    if(*s>='0' && *s<='9')r+=*s-'0';else if(*s>='A'&&*s<='F')r+=*s-'A'+10;s++;} return r; }

#if DEBUG
#define DBG(s) puts(s)
#else
#define DBG(s)
#endif

/* sdcard */
#define EMMC_ARG2           ((volatile uint32_t*)(MMIO_BASE+0x00300000))
#define EMMC_BLKSIZECNT     ((volatile uint32_t*)(MMIO_BASE+0x00300004))
#define EMMC_ARG1           ((volatile uint32_t*)(MMIO_BASE+0x00300008))
#define EMMC_CMDTM          ((volatile uint32_t*)(MMIO_BASE+0x0030000C))
#define EMMC_RESP0          ((volatile uint32_t*)(MMIO_BASE+0x00300010))
#define EMMC_RESP1          ((volatile uint32_t*)(MMIO_BASE+0x00300014))
#define EMMC_RESP2          ((volatile uint32_t*)(MMIO_BASE+0x00300018))
#define EMMC_RESP3          ((volatile uint32_t*)(MMIO_BASE+0x0030001C))
#define EMMC_DATA           ((volatile uint32_t*)(MMIO_BASE+0x00300020))
#define EMMC_STATUS         ((volatile uint32_t*)(MMIO_BASE+0x00300024))
#define EMMC_CONTROL0       ((volatile uint32_t*)(MMIO_BASE+0x00300028))
#define EMMC_CONTROL1       ((volatile uint32_t*)(MMIO_BASE+0x0030002C))
#define EMMC_INTERRUPT      ((volatile uint32_t*)(MMIO_BASE+0x00300030))
#define EMMC_INT_MASK       ((volatile uint32_t*)(MMIO_BASE+0x00300034))
#define EMMC_INT_EN         ((volatile uint32_t*)(MMIO_BASE+0x00300038))
#define EMMC_CONTROL2       ((volatile uint32_t*)(MMIO_BASE+0x0030003C))
#define EMMC_SLOTISR_VER    ((volatile uint32_t*)(MMIO_BASE+0x003000FC))

// command flags
#define CMD_NEED_APP        0x80000000
#define CMD_RSPNS_48        0x00020000
#define CMD_ERRORS_MASK     0xfff9c004
#define CMD_RCA_MASK        0xffff0000

// COMMANDs
#define CMD_GO_IDLE         0x00000000
#define CMD_ALL_SEND_CID    0x02010000
#define CMD_SEND_REL_ADDR   0x03020000
#define CMD_CARD_SELECT     0x07030000
#define CMD_SEND_IF_COND    0x08020000
#define CMD_STOP_TRANS      0x0C030000
#define CMD_READ_SINGLE     0x11220010
#define CMD_READ_MULTI      0x12220032
#define CMD_SET_BLOCKCNT    0x17020000
#define CMD_APP_CMD         0x37000000
#define CMD_SET_BUS_WIDTH   (0x06020000|CMD_NEED_APP)
#define CMD_SEND_OP_COND    (0x29020000|CMD_NEED_APP)
#define CMD_SEND_SCR        (0x33220010|CMD_NEED_APP)

// STATUS register settings
#define SR_READ_AVAILABLE   0x00000800
#define SR_DAT_INHIBIT      0x00000002
#define SR_CMD_INHIBIT      0x00000001
#define SR_APP_CMD          0x00000020

// INTERRUPT register settings
#define INT_DATA_TIMEOUT    0x00100000
#define INT_CMD_TIMEOUT     0x00010000
#define INT_READ_RDY        0x00000020
#define INT_CMD_DONE        0x00000001

#define INT_ERROR_MASK      0x017E8000

// CONTROL register settings
#define C0_SPI_MODE_EN      0x00100000
#define C0_HCTL_HS_EN       0x00000004
#define C0_HCTL_DWITDH      0x00000002

#define C1_SRST_DATA        0x04000000
#define C1_SRST_CMD         0x02000000
#define C1_SRST_HC          0x01000000
#define C1_TOUNIT_DIS       0x000f0000
#define C1_TOUNIT_MAX       0x000e0000
#define C1_CLK_GENSEL       0x00000020
#define C1_CLK_EN           0x00000004
#define C1_CLK_STABLE       0x00000002
#define C1_CLK_INTLEN       0x00000001

// SLOTISR_VER values
#define HOST_SPEC_NUM       0x00ff0000
#define HOST_SPEC_NUM_SHIFT 16
#define HOST_SPEC_V3        2
#define HOST_SPEC_V2        1
#define HOST_SPEC_V1        0

// SCR flads
#define SCR_SD_BUS_WIDTH_4  0x00000400
#define SCR_SUPP_SET_BLKCNT 0x02000000
 
#define ACMD41_VOLTAGE      0x00ff8000
#define ACMD41_CMD_COMPLETE 0x80000000
#define ACMD41_CMD_CCS      0x40000000
#define ACMD41_ARG_HC       0x51ff8000

#define SD_OK                0
#define SD_TIMEOUT          -1
#define SD_ERROR            -2

uint32_t sd_scr[2], sd_ocr, sd_rca, sd_err, sd_hv;

/**
 * Wait for data or command ready
 */
int sd_status(uint32_t mask)
{
    int cnt = 500000; while((*EMMC_STATUS & mask) && !(*EMMC_INTERRUPT & INT_ERROR_MASK) && cnt--) delaym(1);
    return (cnt <= 0 || (*EMMC_INTERRUPT & INT_ERROR_MASK)) ? SD_ERROR : SD_OK;
}

/**
 * Wait for interrupt
 */
int sd_int(uint32_t mask)
{
    uint32_t r, m=mask | INT_ERROR_MASK;
    int cnt = 1000000; while(!(*EMMC_INTERRUPT & m) && cnt--) delaym(1);
    r=*EMMC_INTERRUPT;
    if(cnt<=0 || (r & INT_CMD_TIMEOUT) || (r & INT_DATA_TIMEOUT) ) { *EMMC_INTERRUPT=r; return SD_TIMEOUT; } else
    if(r & INT_ERROR_MASK) { *EMMC_INTERRUPT=r; return SD_ERROR; }
    *EMMC_INTERRUPT=mask;
    return 0;
}

/**
 * Send a command
 */
int sd_cmd(uint32_t code, uint32_t arg)
{
    int r=0;
    sd_err=SD_OK;
    if(code&CMD_NEED_APP) {
        r=sd_cmd(CMD_APP_CMD|(sd_rca?CMD_RSPNS_48:0),sd_rca);
        if(sd_rca && !r) { DBG("BOOTBOOT-ERROR: failed to send SD APP command\n"); sd_err=SD_ERROR;return 0;}
        code &= ~CMD_NEED_APP;
    }
    if(sd_status(SR_CMD_INHIBIT)) { DBG("BOOTBOOT-ERROR: EMMC busy\n"); sd_err= SD_TIMEOUT;return 0;}
#if SD_DEBUG
    uart_puts("EMMC: Sending command ");uart_hex(code,4);uart_puts(" arg ");uart_hex(arg,4);uart_putc('\n');
#endif
    *EMMC_INTERRUPT=*EMMC_INTERRUPT; *EMMC_ARG1=arg; *EMMC_CMDTM=code;
    if(code==CMD_SEND_OP_COND) delaym(1000); else 
    if(code==CMD_SEND_IF_COND || code==CMD_APP_CMD) delaym(100);
    if((r=sd_int(INT_CMD_DONE))) {DBG("BOOTBOOT-ERROR: failed to send EMMC command\n");sd_err=r;return 0;}
    r=*EMMC_RESP0;
    if(code==CMD_GO_IDLE || code==CMD_APP_CMD) return 0; else
    if(code==(CMD_APP_CMD|CMD_RSPNS_48)) return r&SR_APP_CMD; else
    if(code==CMD_SEND_OP_COND) return r; else
    if(code==CMD_SEND_IF_COND) return r==arg? SD_OK : SD_ERROR; else
    if(code==CMD_ALL_SEND_CID) {r|=*EMMC_RESP3; r|=*EMMC_RESP2; r|=*EMMC_RESP1; return r; } else
    if(code==CMD_SEND_REL_ADDR) {
        sd_err=(((r&0x1fff))|((r&0x2000)<<6)|((r&0x4000)<<8)|((r&0x8000)<<8))&CMD_ERRORS_MASK;
        return r&CMD_RCA_MASK;
    }
    return r&CMD_ERRORS_MASK;
    // make gcc happy
    return 0;
}

/**
 * read a block from sd card and return the number of bytes read
 * returns 0 on error.
 */
int sd_readblock(uint64_t lba, uint8_t *buffer, uint32_t num)
{
#if SD_DEBUG
    uart_puts("sd_readblock lba ");uart_hex(lba,4);uart_puts(" num ");uart_hex(num,4);uart_putc('\n');
#endif
    if(sd_status(SR_DAT_INHIBIT)) {sd_err=SD_TIMEOUT; return 0;}
    int transferCmd = ( num == 1 ? CMD_READ_SINGLE : CMD_READ_MULTI);
    int r;
    if( num > 1 && (sd_scr[0] & SCR_SUPP_SET_BLKCNT)) {
    sd_cmd(CMD_SET_BLOCKCNT,num);
    if(sd_err) return 0;
    }
    *EMMC_BLKSIZECNT = (num << 16) | 512;
    sd_cmd(transferCmd,lba);
    if(sd_err) return 0;
    
    int c = 0, d;
    uint32_t *buf=(uint32_t *)buffer;
    while( c < num ) {
        if((r=sd_int(INT_READ_RDY))){DBG("BOOTBOOT-ERROR: Timeout waiting for ready to read\n");sd_err=r;return 0;}
        for(d=0;d<128;d++) buf[d] = *EMMC_DATA;
        c++; buf+=128;
    }
    
    if( num > 1 && !(sd_scr[0] & SCR_SUPP_SET_BLKCNT)) sd_cmd(CMD_STOP_TRANS,0);
    return sd_err!=SD_OK || c!=num? 0 : num*512;
}

/**
 * set SD clock to frequency in Hz
 */
int sd_clk(uint32_t f)
{
    uint32_t d,c=41666666/f,x,s=32,h=0;
    int cnt = 100000;
    while((*EMMC_STATUS & (SR_CMD_INHIBIT|SR_DAT_INHIBIT)) && cnt--) delaym(1);
    if(cnt<=0) {
        DBG("BOOTBOOT-ERROR: timeout waiting for inhibit flag\n");
        return SD_ERROR;
    }

    *EMMC_CONTROL1 &= ~C1_CLK_EN; delaym(10);
    x=c-1; if(!x) s=0; else {
        if(!(x & 0xffff0000u)) { x <<= 16; s -= 16; }
        if(!(x & 0xff000000u)) { x <<= 8;  s -= 8; }
        if(!(x & 0xf0000000u)) { x <<= 4;  s -= 4; }
        if(!(x & 0xc0000000u)) { x <<= 2;  s -= 2; }
        if(!(x & 0x80000000u)) { x <<= 1;  s -= 1; }
        if(s>0) s--;
        if(s>7) s=7;
    }
    if(sd_hv>HOST_SPEC_V2) d=c; else d=(1<<s);
    if(d<=2) {d=2;s=0;}
#if SD_DEBUG
    uart_puts("sd_clk divisor ");uart_hex(d,4);uart_puts(", shift ");uart_hex(s,4);uart_putc('\n');
#endif
    if(sd_hv>HOST_SPEC_V2) h=(d&0x300)>>2;
    d=(((d&0x0ff)<<8)|h);
    *EMMC_CONTROL1=(*EMMC_CONTROL1&0xffff003f)|d; delaym(10);
    *EMMC_CONTROL1 |= C1_CLK_EN; delaym(10);
    cnt=10000; while(!(*EMMC_CONTROL1 & C1_CLK_STABLE) && cnt--) delaym(10);
    if(cnt<=0) {
        DBG("BOOTBOOT-ERROR: failed to get stable clock\n");
        return SD_ERROR;
    }
    return SD_OK;
}

/**
 * initialize EMMC to read SDHC card
 */
int sd_init()
{
    int r,cnt;
    // GPIO_CD
    r=*GPFSEL4; r&=~(7<<(7*3)); *GPFSEL4=r;
    *GPPUD=2; delay(150); *GPPUDCLK1=(1<<15); delay(150); *GPPUD=0; *GPPUDCLK1=0;
    r=*GPHEN1; r|=1<<15; *GPHEN1=r;

    // GPIO_CLK, GPIO_CMD
    r=*GPFSEL4; r|=(7<<(8*3))|(7<<(9*3)); *GPFSEL4=r;
    *GPPUD=2; delay(150); *GPPUDCLK1=(1<<16)|(1<<17); delay(150); *GPPUD=0; *GPPUDCLK1=0;

    // GPIO_DAT0, GPIO_DAT1, GPIO_DAT2, GPIO_DAT3
    r=*GPFSEL5; r|=(7<<(0*3)) | (7<<(1*3)) | (7<<(2*3)) | (7<<(3*3)); *GPFSEL5=r;
    *GPPUD=2; delay(150); 
    *GPPUDCLK1=(1<<18) | (1<<19) | (1<<20) | (1<<21);
    delay(150); *GPPUD=0; *GPPUDCLK1=0;
    
    sd_hv = (*EMMC_SLOTISR_VER & HOST_SPEC_NUM) >> HOST_SPEC_NUM_SHIFT;
#if SD_DEBUG
    uart_puts("EMMC: GPIO set up\n");
#endif
    // Reset the card.
    *EMMC_CONTROL0 = 0; *EMMC_CONTROL1 |= C1_SRST_HC;
    cnt=10000; do{delaym(10);} while( (*EMMC_CONTROL1 & C1_SRST_HC) && cnt-- );
    if(cnt<=0) {
        DBG("BOOTBOOT-ERROR: failed to reset EMMC\n");
        return SD_ERROR;
    }
#if SD_DEBUG
    uart_puts("EMMC: reset OK\n");
#endif
    *EMMC_CONTROL1 |= C1_CLK_INTLEN | C1_TOUNIT_MAX;
    delaym(10);
    // Set clock to setup frequency.
    if((r=sd_clk(400000))) return r;
    *EMMC_INT_EN   = 0xffffffff;
    *EMMC_INT_MASK = 0xffffffff;
    sd_scr[0]=sd_scr[1]=sd_rca=sd_err=0;
    sd_cmd(CMD_GO_IDLE,0);
    if(sd_err) return sd_err;

    sd_cmd(CMD_SEND_IF_COND,0x000001AA);
    if(!sd_err) {
        cnt=6; r=0; while(!(r&ACMD41_CMD_COMPLETE) && cnt--) {
            delay(400);
            r=sd_cmd(CMD_SEND_OP_COND,ACMD41_ARG_HC);
            if(sd_err!=SD_TIMEOUT && sd_err!=SD_OK ) {
                DBG("BOOTBOOT-ERROR: EMMC ACMD41 returned error\n");
                return sd_err;
            }
        }
        if(!(r&ACMD41_CMD_COMPLETE) || !cnt ) return SD_TIMEOUT;
        if(!(r&ACMD41_VOLTAGE) || !(r&ACMD41_CMD_CCS)) return SD_ERROR;
    } else
        return sd_err;

    sd_cmd(CMD_ALL_SEND_CID,0);

    sd_rca = sd_cmd(CMD_SEND_REL_ADDR,0);
    if(sd_err) return sd_err;

    if((r=sd_clk(25000000))) return r;

    sd_cmd(CMD_CARD_SELECT,sd_rca);
    if(sd_err) return sd_err;

    if(sd_status(SR_DAT_INHIBIT)) return SD_TIMEOUT;
    *EMMC_BLKSIZECNT = (1<<16) | 8;
    sd_cmd(CMD_SEND_SCR,0);
    if(sd_err) return sd_err;
    if(sd_int(INT_READ_RDY)) return SD_TIMEOUT;

    r=0; cnt=100000; while(r<2 && cnt) {
        if( *EMMC_STATUS & SR_READ_AVAILABLE )
            sd_scr[r++] = *EMMC_DATA;
        else
            delaym(1);
    }
    if(r!=2) return SD_TIMEOUT;
    if(sd_scr[0] & SCR_SD_BUS_WIDTH_4) {
        sd_cmd(CMD_SET_BUS_WIDTH,sd_rca|2);
        if(sd_err) return sd_err;
        *EMMC_CONTROL0 |= C0_HCTL_DWITDH;
    }
    return SD_OK;
}

// get filesystem drivers for initrd
#include "../../etc/include/fsZ.h"
#include "fs.h"

/*** other defines and structs ***/
typedef struct {
    uint32_t type[4];
    uint8_t  uuid[16];
    uint64_t start;
    uint64_t end;
    uint64_t flags;
    uint8_t  name[72];
} efipart_t;

typedef struct {
    char        jmp[3];
    char        oem[8];
    uint16_t    bps;
    uint8_t     spc;
    uint16_t    rsc;
    uint8_t     nf;
    uint16_t    nr;
    uint16_t    ts16;
    uint8_t     media;
    uint16_t    spf16;
    uint16_t    spt;
    uint16_t    nh;
    uint32_t    hs;
    uint32_t    ts32;
    uint32_t    spf32;
    uint32_t    flg;
    uint32_t    rc;
    char        vol[6];
    char        fst[8];
    char        dmy[20];
    char        fst2[8];
} __attribute__((packed)) bpb_t;

typedef struct {
    char        name[8];
    char        ext[3];
    char        attr[9];
    uint16_t    ch;
    uint32_t    attr2;
    uint16_t    cl;
    uint32_t    size;
} __attribute__((packed)) fatdir_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;/* offset of bitmaps in file */
    uint16_t flags;     /* original PSF2 has 32 bit flags */
    uint8_t hotspot_x;  /* addition to OS/Z */
    uint8_t hotspot_y;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
    uint8_t glyphs;
} __attribute__((packed)) font_t;

extern volatile unsigned char _binary_font_psf_start;
/* current cursor position */
int kx, ky;
/* maximum coordinates */
int maxx, maxy;

/*** common variables ***/
file_t env;         // environment file descriptor
file_t initrd;      // initrd file descriptor
file_t core;        // kernel file descriptor
BOOTBOOT *bootboot; // the BOOTBOOT structure

// default environment variables. M$ states that 1024x768 must be supported
int reqwidth = 1024, reqheight = 768;
char *kernelname="sys/core";
unsigned char *kne;

// alternative environment name
char *cfgname="sys/config";

/**
 * Get a linear frame buffer
 */
int GetLFB(uint32_t width, uint32_t height)
{
    font_t *font = (font_t*)&_binary_font_psf_start;

    //query natural width, height if not given
    if(width==0 && height==0) {
        mbox[0] = 8*4;
        mbox[1] = MBOX_REQUEST;
        mbox[2] = 0x40003;  //get phy wh
        mbox[3] = 8;
        mbox[4] = 8;
        mbox[5] = 0;
        mbox[6] = 0;
        mbox[7] = 0;
        if(mbox_call(MBOX_CH_PROP,mbox) && mbox[5]!=0) {
            width=mbox[5];
            height=mbox[6];
        }
    }
    //if we already have a framebuffer, release it
    if(bootboot->fb_ptr!=NULL) {
        mbox[0] = 8*4;
        mbox[1] = MBOX_REQUEST;
        mbox[2] = 0x48001;  //release buffer
        mbox[3] = 8;
        mbox[4] = 8;
        mbox[5] = (uint32_t)(((uint64_t)bootboot->fb_ptr));
        mbox[6] = (uint32_t)(((uint64_t)bootboot->fb_ptr)>>32);
        mbox[7] = 0;
        mbox_call(MBOX_CH_PROP,mbox);
    }
    //check minimum resolution
    if(width<800 || height<600) {
        width=800; height=600;
    }
    mbox[0] = 31*4;
    mbox[1] = MBOX_REQUEST;

    mbox[2] = 0x48003;  //set phy wh
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = width;
    mbox[6] = height;

    mbox[7] = 0x48004;  //set virt wh
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = width;
    mbox[11] = height;
    
    mbox[12] = 0x48009; //set virt offset
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0;
    mbox[16] = 0;
    
    mbox[17] = 0x48005; //set depth
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = 32;      //only RGBA supported

    mbox[21] = 0x40001; //get framebuffer
    mbox[22] = 8;       //response size
    mbox[23] = 8;       //request size
    mbox[24] = PAGESIZE;//buffer alignment
    mbox[25] = 0;
    
    mbox[26] = 0x40008; //get pitch
    mbox[27] = 4;
    mbox[28] = 4;
    mbox[29] = 0;

    mbox[30] = 0;       //Arnold Schwarzenegger

    if(mbox_call(MBOX_CH_PROP,mbox) && mbox[20]==32 && mbox[23]==(MBOX_RESPONSE|8) && mbox[25]!=0) {
        mbox[24]&=0x3FFFFFFF;
        bootboot->fb_width=mbox[5];
        bootboot->fb_height=mbox[6];
        bootboot->fb_scanline=mbox[29];
        bootboot->fb_ptr=(void*)((uint64_t)mbox[24]);
        bootboot->fb_size=mbox[25];
        bootboot->fb_type=FB_ARGB;
        kx=ky=0;
        maxx=bootboot->fb_width/(font->width+1);
        maxy=bootboot->fb_height/font->height;
        return 1;
    }
    return 0;
}

/**
 * display one literal unicode character
 */
void putc(char c)
{
    font_t *font = (font_t*)&_binary_font_psf_start;
    unsigned char *glyph = (unsigned char*)&_binary_font_psf_start +
     font->headersize + (c>0&&c<font->numglyph?c:0)*font->bytesperglyph;
    int offs = (ky * font->height * bootboot->fb_scanline) + (kx * (font->width+1) * 4);
    int x,y, line,mask;
    int bytesperline=(font->width+7)/8;
    for(y=0;y<font->height;y++){
        line=offs;
        mask=1<<(font->width-1);
        for(x=0;x<font->width;x++){
            *((uint32_t*)((uint64_t)bootboot->fb_ptr + line))=((int)*glyph) & (mask)?0xFFFFFF:0;
            mask>>=1;
            line+=4;
        }
        *((uint32_t*)((uint64_t)bootboot->fb_ptr + line))=0;
        glyph+=bytesperline;
        offs+=bootboot->fb_scanline;
    }
    // send it to serial too
    uart_putc(c);
}

/**
 * display a string
 */
void puts(char *s)
{
    while(*s) {
        if(*s=='\r') {
            uart_putc(*s);
            kx=0;
        } else
        if(*s=='\n') {
            uart_putc(*s);
            kx=0;ky++;
        } else {
            putc(*s);
            kx++;
            if(kx>=maxx) {
                kx=0; ky++;
            }
        }
        s++;
    }
}

void ParseEnvironment(uint8_t *env)
{
    uint8_t *end=env+PAGESIZE;
    DBG(" * Environment\n");
    env--; env[PAGESIZE]=0; kne=NULL;
    while(env<end) {
        env++;
        // failsafe
        if(env[0]==0)
            break;
        // skip white spaces
        if(env[0]==' '||env[0]=='\t'||env[0]=='\r'||env[0]=='\n')
            continue;
        // skip comments
        if((env[0]=='/'&&env[1]=='/')||env[0]=='#') {
            while(env<end && env[0]!='\r' && env[0]!='\n' && env[0]!=0){
                env++;
            }
            env--;
            continue;
        }
        if(env[0]=='/'&&env[1]=='*') {
            env+=2;
            while(env[0]!=0 && env[-1]!='*' && env[0]!='/')
                env++;
        }
        // parse screen dimensions
        if(!memcmp(env,"screen=",7)){
            env+=7;
            reqwidth=atoi(env);
            while(env<end && *env!=0 && *(env-1)!='x') env++;
            reqheight=atoi(env);
        }
        // get kernel's filename
        if(!memcmp(env,"kernel=",7)){
            env+=7;
            kernelname=(char*)env;
            while(env<end && env[0]!='\r' && env[0]!='\n' &&
                env[0]!=' ' && env[0]!='\t' && env[0]!=0)
                    env++;
            kne=env;
            *env=0;
            env++;
        }
    }
}

/**
 * bootboot entry point
 */
int bootboot_main(uint64_t hcl)
{
    uint32_t np,sp,r,pa,mp;
    efipart_t *part;
    volatile bpb_t *bpb;
    uint64_t entrypoint=0, *paging, reg;
    MMapEnt *mmap;

    /* initialize UART */
    *UART0_CR = 0;         // turn off UART0 or real hw. qemu doesn't care
    *AUX_ENABLE |=1;       // enable UART1, mini uart
    *AUX_MU_IER = 0;
    *AUX_MU_CNTL = 0;
    *AUX_MU_LCR = 3;       // 8 bits
    *AUX_MU_MCR = 0;
    *AUX_MU_IER = 0;
    *AUX_MU_IIR = 0xc6;    // disable interrupts
    *AUX_MU_BAUD = 270;    // 115200 baud
    r=*GPFSEL1;
    r&=~((7<<12)|(7<<15)); // gpio14, gpio15
    r|=(2<<12)|(2<<15);    // alt5
    *GPFSEL1 = r;
    *GPPUD = 0;
    delay(150);
    *GPPUDCLK0 = (1<<14)|(1<<15);
    delay(150);
    *GPPUDCLK0 = 0;        // flush GPIO setup
    *AUX_MU_CNTL = 3;      // enable Tx, Rx

    /* create bootboot structure */
    bootboot = (BOOTBOOT*)&__bootboot;
    memcpy((void*)&bootboot->magic,BOOTBOOT_MAGIC,4);
    bootboot->protocol = PROTOCOL_STATIC;
    bootboot->loader_type = LOADER_RPI;
    bootboot->size = 128;
    bootboot->pagesize = PAGESIZE;
    bootboot->aarch64.mmio_ptr = MMIO_BASE;
    // set up a framebuffer so that we can write on screen
    if(!GetLFB(0, 0)) goto viderr;
    puts("Booting OS...\n");

    /* check for system timer presence and 4k granule and at least 36 bits address */
    asm volatile ("mrs %0, id_aa64mmfr0_el1" : "=r" (reg));
    pa=reg&0xF;
    if(getmicro()==0 || reg&(0xF<<28) || pa<1) {
        puts("BOOTBOOT-PANIC: Hardware not supported\n");
        uart_puts("ID_AA64MMFR0_EL1 ");
        uart_hex(reg,8);
        uart_puts(" SYSTIMER ");
        uart_puts(getmicro()==0?"none\n":"ok\n");
        goto error;
    }

    /* initialize SDHC card reader in EMMC */
    if(sd_init()) {
        puts("BOOTBOOT-PANIC: Unable to initialize SDHC card\n");
        goto error;
    }

    /* read and parse GPT table */
    r=sd_readblock(1,(unsigned char*)&__diskbuf,1);
    if(r==0 || memcmp((void*)&__diskbuf, "EFI PART", 8)) {
diskerr:
        puts("BOOTBOOT-PANIC: No boot partition\n");
        goto error;
    }
    // get number of partitions and size of partition entry
    np=*((uint32_t*)((char*)&__diskbuf+80)); sp=*((uint32_t*)((char*)&__diskbuf+84));
    // read GPT entries
    r=sd_readblock(*((uint32_t*)((char*)&__diskbuf+72)),(unsigned char*)&__diskbuf,(np*sp+511)/512);
    if(r==0) goto diskerr;
    part=NULL;
    for(r=0;r<np;r++) {
        part = (efipart_t*)((char*)&__diskbuf+r*sp);
            // ESP?
        if((part->type[0]==0xC12A7328 && part->type[1]==0x11D2F81F) ||
            // bootable?
            part->flags&4 ||
            // or OS/Z root partition for this architecture?
            (part->type[0]==0x5A2F534F && (part->type[1]&0xFFFF)==0xAA64 && part->type[3]==0x746F6F72))
            break;
    }
    if(part==NULL) goto diskerr;
    r=sd_readblock(part->start,(unsigned char*)&_end,part->end-part->start+1);
    if(r==0) goto diskerr;
    DBG(" * Initrd loaded\n");
    initrd.ptr=NULL; initrd.size=0;
    //is it a FAT partition?
    bpb=(bpb_t*)&_end;
    if(!memcmp((void*)bpb->fst,"FAT16",5) || !memcmp((void*)bpb->fst2,"FAT32",5)) {
        // locate BOOTBOOT directory
        uint32_t data_sec, root_sec, sec=0;
        fatdir_t *dir, *dir2;
        data_sec=root_sec=((bpb->spf16?bpb->spf16:bpb->spf32)*bpb->nf)+bpb->rsc;
        if(bpb->spf16>0) {
            //sec=bpb->nr; WARNING gcc generates a code that cause unaligned exception
            sec=*((uint32_t*)&bpb->nf);
            sec>>=8;
            sec&=0xFFFF;
            sec<<=5;
            sec+=511;
            sec>>=9;
            data_sec+=sec;
        } else {
            root_sec+=(bpb->rc-2)*bpb->spc;
        }
        dir=(fatdir_t*)((char*)&_end+root_sec*512);
        while(dir->name[0]!=0 && memcmp(dir->name,"BOOTBOOT   ",11)) dir++;
        if(dir->name[0]!='B') goto diskerr;
        sec=((dir->cl+(dir->ch<<16)-2)*bpb->spc)+data_sec;
        dir=dir2=(fatdir_t*)((char*)&_end+sec*512);
        // locate environment and initrd
        while(dir->name[0]!=0) {
            sec=((dir->cl+(dir->ch<<16)-2)*bpb->spc)+data_sec;
            if(!memcmp(dir->name,"CONFIG     ",11)) {
                memcpy((void*)&__environment,(void*)((char*)&_end+sec*512),dir->size<PAGESIZE?dir->size:PAGESIZE-1);
            } else
            if(!memcmp(dir->name,"INITRD     ",11)) {
                initrd.ptr=(uint8_t*)&_end+sec*512;
                initrd.size=dir->size;
            }
            dir++;
        }
        // if initrd not found, try architecture specific name
        if(initrd.size==0) {
            dir=dir2;
            while(dir->name[0]!=0) {
                if(!memcmp(dir->name,"AARCH64    ",11)) {
                    sec=((dir->cl+(dir->ch<<16)-2)*bpb->spc)+data_sec;
                    initrd.ptr=(uint8_t*)&_end+sec*512;
                    initrd.size=dir->size;
                    break;
                }
                dir++;
            }
        }
    } else {
        // initrd is on the entire partition
        initrd.ptr=(uint8_t*)&_end;
        initrd.size=r;
    }
    if(initrd.ptr==NULL || initrd.size==0) {
        puts("BOOTBOOT-PANIC: INITRD not found\n");
        goto error;
    }
#if INITRD_DEBUG
    uart_puts("Initrd at ");uart_hex((uint64_t)initrd.ptr,4);uart_putc(' ');uart_hex(initrd.size,4);uart_putc('\n');
#endif
    // uncompress if it's compressed
    if(initrd.ptr[0]==0x1F && initrd.ptr[1]==0x8B) {
        unsigned char *addr,f;
        volatile TINF_DATA d;
        DBG(" * Gzip compressed initrd\n");
        // skip gzip header
        addr=initrd.ptr+2;
        if(*addr++!=8) goto gzerr;
        f=*addr++; addr+=6;
        if(f&4) { r=*addr++; r+=(*addr++ << 8); addr+=r; }
        if(f&8) { while(*addr++ != 0); }
        if(f&16) { while(*addr++ != 0); }
        if(f&2) addr+=2;
        d.source = addr;
        memcpy((void*)&d.destSize,initrd.ptr+initrd.size-4,4);
        // decompress
        d.bitcount = 0;
        d.bfinal = 0;
        d.btype = -1;
        d.curlen = 0;
        d.dest = (unsigned char*)((uint64_t)(initrd.ptr+initrd.size+PAGESIZE-1)&~(PAGESIZE-1));
        initrd.ptr=(uint8_t*)d.dest;
        initrd.size=d.destSize;
#if INITRD_DEBUG
        uart_puts("Inflating to ");uart_hex((uint64_t)d.dest,4);uart_putc(' ');uart_hex(d.destSize,4);uart_putc('\n');
#endif
        puts("Inflating image...\r");
        do { r = uzlib_uncompress(&d); } while (!r);
        puts("                  \r");
        if (r != TINF_DONE) {
gzerr:      puts("BOOTBOOT-PANIC: Unable to uncompress\n");
            goto error;
        }
    }
    // copy the initrd to it's final position, making it properly aligned
    if((uint64_t)initrd.ptr!=(uint64_t)&_end) {
        memcpy((void*)&_end, initrd.ptr, initrd.size);
    }
    bootboot->initrd_ptr=(uint64_t)&_end;
    // round up to page size
    bootboot->initrd_size=(initrd.size+PAGESIZE-1)&~(PAGESIZE-1);
#if INITRD_DEBUG
    // dump initrd in memory
    uart_dump((void*)bootboot->initrd_ptr,1);
#endif

    // if no config, locate it in uncompressed initrd
    if(1||*((uint8_t*)&__environment)==0) {
        r=0; env.ptr=NULL;
        while(env.ptr==NULL && fsdrivers[r]!=NULL) {
            env=(*fsdrivers[r++])((unsigned char*)bootboot->initrd_ptr,cfgname);
        }
        if(env.ptr!=NULL)
            memcpy((void*)&__environment,(void*)(env.ptr),env.size<PAGESIZE?env.size:PAGESIZE-1);
    }

    // parse config
    ParseEnvironment((unsigned char*)&__environment);

    // locate sys/core
    entrypoint=0;
    r=0; core.ptr=NULL;
    while(core.ptr==NULL && fsdrivers[r]!=NULL) {
        core=(*fsdrivers[r++])((unsigned char*)bootboot->initrd_ptr,kernelname);
    }
    if(kne!=NULL)
        *kne='\n';
    // scan for the first executable
    if(core.ptr==NULL || core.size==0) {
        puts(" * Autodetecting kernel\n");
        core.size=0;
        r=bootboot->initrd_size;
        core.ptr=(uint8_t*)bootboot->initrd_ptr;
        while(r-->0) {
            Elf64_Ehdr *ehdr=(Elf64_Ehdr *)(core.ptr);
            pe_hdr *pehdr=(pe_hdr*)(core.ptr + ((mz_hdr*)(core.ptr))->peaddr);
            if((!memcmp(ehdr->e_ident,ELFMAG,SELFMAG)||!memcmp(ehdr->e_ident,"OS/Z",4))&&
                ehdr->e_ident[EI_CLASS]==ELFCLASS64&&
                ehdr->e_ident[EI_DATA]==ELFDATA2LSB&&
                ehdr->e_machine==EM_AARCH64&&
                ehdr->e_phnum>0){
                    core.size=1;
                    break;
                }
            if(((mz_hdr*)(core.ptr))->magic==MZ_MAGIC && pehdr->magic == PE_MAGIC && 
                pehdr->machine == IMAGE_FILE_MACHINE_ARM64 && pehdr->file_type == PE_OPT_MAGIC_PE32PLUS) {
                    core.size=1;
                    break;
                }
            core.ptr++;
        }
    }
    if(core.ptr==NULL || core.size==0) {
        puts("BOOTBOOT-PANIC: Kernel not found in initrd\n");
        goto error;
    } else {
        Elf64_Ehdr *ehdr=(Elf64_Ehdr *)(core.ptr);
        pe_hdr *pehdr=(pe_hdr*)(core.ptr + ((mz_hdr*)(core.ptr))->peaddr);
        if((!memcmp(ehdr->e_ident,ELFMAG,SELFMAG)||!memcmp(ehdr->e_ident,"OS/Z",4))&&
            ehdr->e_ident[EI_CLASS]==ELFCLASS64&&
            ehdr->e_ident[EI_DATA]==ELFDATA2LSB&&
            ehdr->e_machine==EM_AARCH64&&
            ehdr->e_phnum>0){
                DBG(" * Parsing ELF64\n");
                Elf64_Phdr *phdr=(Elf64_Phdr *)((uint8_t *)ehdr+ehdr->e_phoff);
                for(r=0;r<ehdr->e_phnum;r++){
                    if(phdr->p_type==PT_LOAD && phdr->p_vaddr>>48==0xffff && phdr->p_offset==0) {
                        core.size = ((phdr->p_filesz+PAGESIZE-1)/PAGESIZE)*PAGESIZE;
                        entrypoint=ehdr->e_entry;
                        break;
                    }
                    phdr=(Elf64_Phdr *)((uint8_t *)phdr+ehdr->e_phentsize);
                }
        } else
        if(((mz_hdr*)(core.ptr))->magic==MZ_MAGIC && pehdr->magic == PE_MAGIC && 
            pehdr->machine == IMAGE_FILE_MACHINE_ARM64 && pehdr->file_type == PE_OPT_MAGIC_PE32PLUS) {
                DBG(" * Parsing PE32+\n");
                core.size = (pehdr->entry_point-pehdr->code_base) + pehdr->text_size + pehdr->data_size;
                entrypoint = pehdr->entry_point;
        }
    }
    if(core.size<2 || entrypoint==0) {
        puts("BOOTBOOT-PANIC: Kernel is not a valid executable\n");
#if DEBUG
        // dump executable
        uart_dump((void*)core.ptr,16);
#endif
        goto error;
    }
    // is core page aligned?
    if((uint64_t)core.ptr&(PAGESIZE-1)) {
        memcpy((void*)(bootboot->initrd_ptr+bootboot->initrd_size), core.ptr, core.size);
        core.ptr=(uint8_t*)(bootboot->initrd_ptr+bootboot->initrd_size);
    }
    core.size = (core.size+PAGESIZE-1)&~(PAGESIZE-1);

    /* generate memory map to bootboot struct */
    DBG(" * Memory Map\n");
    mmap=(MMapEnt *)&bootboot->mmap;

    // everything before the bootboot struct is free
    mmap->ptr=0; mmap->size=(uint64_t)&__bootboot | MMAP_FREE;
    mmap++; bootboot->size+=sizeof(MMapEnt);

    // mark bss reserved 
    mmap->ptr=(uint64_t)&__bootboot; mmap->size=((uint64_t)&_end-(uint64_t)&__bootboot) | MMAP_RESERVED;
    mmap++; bootboot->size+=sizeof(MMapEnt);

    // after bss and before initrd is free
    mmap->ptr=(uint64_t)&_end; mmap->size=(bootboot->initrd_ptr-(uint64_t)&_end) | MMAP_FREE;
    mmap++; bootboot->size+=sizeof(MMapEnt);

    // initrd is reserved (and add aligned core's area to it)
    r=bootboot->initrd_size;
    if((uint64_t)core.ptr==bootboot->initrd_ptr+r) r+=core.size;
    mmap->ptr=bootboot->initrd_ptr; mmap->size=r | MMAP_RESERVED;
    mmap++; bootboot->size+=sizeof(MMapEnt);
    r+=(uint32_t)bootboot->initrd_ptr;

    mbox[0]=8*4;
    mbox[1]=0;
    mbox[2]=0x10005; // get memory size
    mbox[3]=8;
    mbox[4]=0;
    mbox[5]=0;
    mbox[6]=0;
    mbox[7]=0;
    if(!mbox_call(MBOX_CH_PROP, mbox))
        // on failure (should never happen) assume 64Mb memory max
        mbox[6]=64*1024*1024;

    // everything after initrd to the top of memory is free
    mp=mbox[6]-r;
    mmap->ptr=r; mmap->size=mp | MMAP_FREE;
    mmap++; bootboot->size+=sizeof(MMapEnt);

    // MMIO area
    mmap->ptr=MMIO_BASE; mmap->size=((uint64_t)0x40000000-MMIO_BASE) | MMAP_MMIO;
    mmap++; bootboot->size+=sizeof(MMapEnt);

#if MEM_DEBUG
    /* dump memory map */
    mmap=(MMapEnt *)&bootboot->mmap;
    for(r=128;r<bootboot->size;r+=sizeof(MMapEnt)) {
        uart_hex(MMapEnt_Ptr(mmap),8);
        uart_putc(' ');
        uart_hex(MMapEnt_Ptr(mmap)+MMapEnt_Size(mmap)-1,8);
        uart_putc(' ');
        uart_hex(MMapEnt_Type(mmap),1);
        uart_putc(' ');
        switch(MMapEnt_Type(mmap)) {
            case MMAP_FREE: uart_puts("free"); break;
            case MMAP_RESERVED: uart_puts("reserved"); break;
            case MMAP_ACPIFREE: uart_puts("acpifree"); break;
            case MMAP_ACPINVS: uart_puts("acpinvs"); break;
            case MMAP_MMIO: uart_puts("mmio"); break;
            default: uart_puts("unknown"); break;
        }
        uart_putc('\n');
        mmap++;
    }
#endif

    /* get linear framebuffer if requested resolution different than current */
    DBG(" * Screen VideoCore\n");
    if(reqwidth!=bootboot->fb_width || reqheight!=bootboot->fb_height) {
        if(!GetLFB(reqwidth, reqheight)) {
viderr:
            puts("BOOTBOOT-PANIC: VideoCore error, no framebuffer\n");
            goto error;
        }
    }

    /* create MMU translation tables in __paging */
    paging=(uint64_t*)&__paging;
    // LLBR0, identity L2
    paging[0]=(uint64_t)((uint8_t*)&__paging+3*PAGESIZE)|0b11|(1<<10); //AF=1,Block=1,Present=1
    // identity L2 2M blocks
    mp>>=21;
    np=MMIO_BASE>>21;
    for(r=1;r<512;r++)
        paging[r]=(uint64_t)(((uint64_t)r<<21))|0b01|(1<<10)|(r>=np?1<<2:0);
    // identity L3
    for(r=0;r<512;r++)
        paging[3*512+r]=(uint64_t)(r*PAGESIZE)|0b11|(1<<10);
    // LLBR1, core L2
    for(r=0;r<4;r++)
        paging[512+480+r]=(uint64_t)((uint8_t*)&__paging+(4+r)*PAGESIZE)|0b11|(1<<10); //map framebuffer
    paging[512+511]=(uint64_t)((uint8_t*)&__paging+2*PAGESIZE)|0b11|(1<<10);// pointer to core L3
    // core L3
    paging[2*512+0]=(uint64_t)((uint8_t*)&__bootboot)|0b11|(1<<10);  // p, b, AF
    paging[2*512+1]=(uint64_t)((uint8_t*)&__environment)|0b11|(1<<10);
    for(r=0;r<(core.size/PAGESIZE)+1;r++)
        paging[2*512+2+r]=(uint64_t)((uint8_t *)core.ptr+(uint64_t)r*PAGESIZE)|0b11|(1<<10);
    paging[2*512+511]=(uint64_t)((uint8_t*)&__paging+8*PAGESIZE)|0b11|(1<<10); // core stack
    // core L3 (lfb)
    for(r=0;r<512;r++)
        paging[4*512+r]=(uint64_t)((uint8_t*)bootboot->fb_ptr+r*PAGESIZE)|0b11|(1<<10); //map framebuffer

#if MEM_DEBUG
    /* dump page translation tables */
    uart_puts("\nTTBR0\n L2 ");
    uart_hex((uint64_t)&__paging,8);
    uart_puts("\n  ");
    for(r=0;r<4;r++) { uart_hex(paging[r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=mp-4;r<mp;r++) { uart_hex(paging[r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=np;r<np+4;r++) { uart_hex(paging[r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=508;r<512;r++) { uart_hex(paging[r],8); uart_putc(' '); }
    uart_puts("\n L3 "); uart_hex((uint64_t)&paging[3*512],8); uart_puts("\n  ");
    for(r=0;r<4;r++) { uart_hex(paging[3*512+r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=508;r<512;r++) { uart_hex(paging[3*512+r],8); uart_putc(' '); }

    uart_puts("\n\nTTBR1\n L2 ");
    uart_hex((uint64_t)&paging[512],8);
    uart_puts("\n  ... (skipped 480) ... ");
    for(r=480;r<484;r++) { uart_hex(paging[512+r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=508;r<512;r++) { uart_hex(paging[512+r],8); uart_putc(' '); }
    uart_puts("\n L3 "); uart_hex((uint64_t)&paging[2*512],8); uart_puts("\n  ");
    for(r=0;r<6;r++) { uart_hex(paging[2*512+r],8); uart_putc(' '); }
    uart_puts("...\n  ... ");
    for(r=508;r<512;r++) { uart_hex(paging[2*512+r],8); uart_putc(' '); }
    uart_puts("\n\n");
#endif
    // enable paging
    reg=(0xCC << 0) |    // normal, in/out write back, non-alloc
        (0x04 << 8) |    // device, nGnRE
        (0x00 <<16);     // coherent, nGnRnE
    asm volatile ("msr mair_el1, %0" : : "r" (reg));
    reg=(0b00LL << 37) | // TBI=0, no tagging
        ((uint64_t)pa << 32) | // IPS=autodetected
        (0b10LL << 30) | // TG1=4k
        (0b11LL << 28) | // SH1=3 inner
        (0b11LL << 26) | // ORGN1=3 write back
        (0b11LL << 24) | // IRGN1=3 write back
        (0b0LL  << 23) | // EPD1 undocumented by ARM DEN0024A Fig 12-5, 12-6
        (34LL   << 16) | // T1SZ=34, 2 levels
        (0b00LL << 14) | // TG0=4k
        (0b11LL << 12) | // SH0=3 inner
        (0b11LL << 10) | // ORGN0=3 write back
        (0b11LL << 8) |  // IRGN0=3 write back
        (0b0LL  << 7) |  // EPD0 undocumented by ARM DEN0024A Fig 12-5, 12-6
        (34LL   << 0);   // T0SZ=34, 2 levels
    asm volatile ("msr ttbr0_el1, %0" : : "r" ((uint64_t)&__paging));
    asm volatile ("msr ttbr1_el1, %0" : : "r" ((uint64_t)&__paging+PAGESIZE));
    asm volatile ("msr tcr_el1, %0; isb" : : "r" (reg));
    asm volatile ("mrs %0, sctlr_el1" : "=r" (reg));
    // set mandatory reserved bits
    reg|=0xC00800;
    reg&=~( (1<<25) |   // clear EE, little endian translation tables
            (1<<24) |   // clear E0E
            (1<<19) |   // clear WXN
            (1<<12) |   // clear I, no instruction cache
            (1<<4) |    // clear SA0
            (1<<3) |    // clear SA
            (1<<2) |    // clear C, no cache at all
            (1<<1));    // clear A, no aligment check
    reg|=(1<<0)|(1<<12)|(1<<2); // set M enable MMU, I instruction cache, C data cache
    asm volatile ("msr sctlr_el1, %0; isb" : : "r" (reg));

    // jump to core's _start
#if DEBUG
    uart_puts(" * Entry point ");
    uart_hex(entrypoint,8);
    uart_putc('\n');
#endif
    jumptokernel(entrypoint);

    // Wait until Enter or Space pressed, then reboot
error:
    while(r!='\n' && r!=' ') r=uart_getc();
    uart_puts("\n\n"); delaym(1000);

    // reset
    *PM_WATCHDOG = PM_WDOG_MAGIC | 1;
    *PM_RTSC = PM_WDOG_MAGIC | PM_RTSC_FULLRST;
    while(1);
}
