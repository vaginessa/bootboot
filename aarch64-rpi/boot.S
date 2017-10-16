/*
 * loader/aarch64-rpi/boot.S
 * 
 * Copyright 2017 Public Domain BOOTBOOT bztsrc@github
 *
 * This file is part of the BOOTBOOT Protocol package.
 * @brief Boot loader for the Raspberry Pi 3+ ARMv8
 *
 */
.section ".text.boot"

.global _start
.global jumptokernel

/*********************************************************************
 *                Entry point called by start.elf                    *
 *********************************************************************/
_start:
    // magic
    b       1f
    .ascii  "BOOTBOOT"
    // read cpu id, stop slave cores
1:  mrs     x7, mpidr_el1
    and     x7, x7, #3
    cbz     x7, 2f
1:  wfe
    b       1b
2:  // set stack before our code
    ldr     x1, =_start
    // set up EL1
    mrs     x0, CurrentEL
    cmp     x0, #4
    beq     1f
    msr     sp_el1, x1
    // set up exception handlers
    ldr     x1, =_vectors
    msr     vbar_el2, x1
    // enable CNTP for EL1
    mrs     x0, cnthctl_el2
    orr     x0, x0, #3
    msr     cnthctl_el2, x0
    msr     cntvoff_el2, xzr
    // initialize virtual MPIDR
    mrs     x0, midr_el1
    mrs     x2, mpidr_el1
    msr     vpidr_el2, x0
    msr     vmpidr_el2, x2
    // disable coprocessor traps
    mov     x0, #0x33FF
    msr     cptr_el2, x0
    msr     hstr_el2, xzr
    mov     x0, #(3 << 20)
    msr     cpacr_el1, x0
    // Setup SCTLR access
    mov     x0, #(1 << 31)      // AArch64
    orr     x0, x0, #(1 << 1)   // SWIO hardwired on Pi3
    msr     hcr_el2, x0
    mrs     x0, hcr_el2
    mov     x2, #0x0800
    movk    x2, #0x30d0, lsl #16
    msr     sctlr_el1, x1
    // change exception level to EL1
    mov     x2, #0x3c4
    msr     spsr_el2, x2
    adr     x2, 1f
    msr     elr_el2, x2
    eret
1:  // clear bss
    ldr     x2, =__bss_start
    ldr     w3, =__bss_size
1:  cbz     w3, 2f
    str     xzr, [x2], #8
    sub     w3, w3, #1
    cbnz    w3, 1b
2:  mov     sp, x1
    // set up exception handlers
    ldr     x1, =_vectors
    msr     vbar_el1, x1
    // jump to C code
    bl      bootboot_main
1:  wfe
    b       1b

jumptokernel:
    // move stack to the top of memory. We waste 16 bytes here
    // to create an address in the negative range
    mov     sp, #-16
    // set PC to kernel entry point
    mov     x30, x0
    ret

    .align  11
_vectors:
    .align  7
    mov     x0, #0
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #1
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #2
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #3
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc

    .align  7
    mov     x0, #4
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #5
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #6
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #7
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc

    .align  7
    mov     x0, #8
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #9
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #10
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
    .align  7
    mov     x0, #11
    mrs     x1, esr_el1
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    mrs     x4, far_el1
    b       uart_exc
