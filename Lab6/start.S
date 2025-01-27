#include "mmu_regs.h"

.section ".text.boot"

.global _start
.global core_timer_enable

_start:  
    mov x28, x0
    // read cpu id, stop slave cores
    bl      from_el2_to_el1

setup_tcr_el1:
    ldr x0, = TCR_CONFIG_DEFAULT
    msr tcr_el1, x0

    ldr x0, =( \
        (MAIR_DEVICE_nGnRnE << (MAIR_IDX_DEVICE_nGnRnE * 8)) | \
        (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)) \
    )
    msr mair_el1, x0

    mov x0, 0x1000 // PGD's page frame at 0x1000
    mov x1, 0x2000 // PUD's page frame at 0x2000

    ldr x2, = BOOT_PGD_ATTR
    orr x2, x1, x2 // combine the physical address of next level page with attribute.
    str x2, [x0]

    ldr x2, = BOOT_PUD_ATTR
    mov x3, 0x00000000
    orr x3, x2, x3
    str x3, [x1] // 1st 1GB mapped by the 1st entry of PUD
    mov x3, 0x40000000
    orr x3, x2, x3
    str x3, [x1, 8] // 2nd 1GB mapped by the 2nd entry of PUD

    msr ttbr0_el1, x0 // load PGD to the bottom translation-based register.
    msr ttbr1_el1, x0 // also load PGD to the upper translation based register.

    mov sp, 0x3c000000
    bl three_level_translation_init

    mrs x2, sctlr_el1
    orr x2 , x2, 1
    msr sctlr_el1, x2 // enable MMU, cache remains disabled

    ldr x2,= boot_rest
    br x2

boot_rest:
    bl      set_exception_vector_table
    mrs     x1, mpidr_el1 //get cpu num
    and     x1, x1, #3 //see if cpu num is 0
    cbz     x1, 2f //cpu 0 jump to 2f (others wfe)
    
    // cpu id > 0, stop
1:  wfe
    b       1b

2:  // cpu id == 0

    // set top of stack just before our code (stack grows to a lower address per AAPCS64)
    //ldr     x1, =_start //ldr: load Register
    //mov     sp, x1 //stack pointer before _start
    //ldr     x0, =0xffff00003c000000
    ldr     x0, =_start
    mov     sp, x0

    // clear bss
    ldr     x1, =__bss_start
    ldr     w2, =__bss_size

3:  cbz     w2, 4f //see if bss is cleared
    str     xzr, [x1], #8 //xzr means zero, clear to zzero from start
    sub     w2, w2, #1 //1 count means 8bit
    cbnz    w2, 3b //if not zero go back

    // jump to C code, should not return
4:  mov x0, x28
    bl      main
    // for failsafe, halt this core too
    b       1b

from_el2_to_el1:
    mov x0, (1 << 31) // EL1 uses aarch64
    msr hcr_el2, x0
    mov x0, 0x5 // 3c5 EL1h (SPSel = 1) with interrupt disabled, 345 enable interrupt
    msr spsr_el2, x0
    msr elr_el2, lr
    eret // return to EL1


.macro save_all
	sub	    sp, sp, 16 * 18
	stp 	x0, x1, [sp ,16 * 0]
	stp 	x2, x3, [sp ,16 * 1]
	stp 	x4, x5, [sp ,16 * 2]
	stp 	x6, x7, [sp ,16 * 3]
	stp 	x8, x9, [sp ,16 * 4]
	stp 	x10, x11, [sp ,16 * 5]
	stp 	x12, x13, [sp ,16 * 6]
	stp 	x14, x15, [sp ,16 * 7]
	stp 	x16, x17, [sp ,16 * 8]
	stp 	x18, x19, [sp ,16 * 9]
	stp 	x20, x21, [sp ,16 * 10]
	stp 	x22, x23, [sp ,16 * 11]
	stp 	x24, x25, [sp ,16 * 12]
	stp 	x26, x27, [sp ,16 * 13]
	stp 	x28, x29, [sp ,16 * 14]
	str 	x30, [sp, 16 * 15]
	mrs	    x28, spsr_el1
	mrs	    x29, elr_el1
	stp 	x28, x29, [sp ,16 * 16]
	mrs	    x28, sp_el0	// For fork
	str	    x28, [sp, 16 * 17]
.endm

.macro load_all
	ldr	    x28, [sp, 16 * 17]
	msr	    sp_el0, x28
	ldp 	x28, x29, [sp ,16 * 16]
	msr	    spsr_el1, x28
	msr	    elr_el1, x29
	ldp 	x0, x1, [sp ,16 * 0]
	ldp 	x2, x3, [sp ,16 * 1]
	ldp 	x4, x5, [sp ,16 * 2]
	ldp 	x6, x7, [sp ,16 * 3]
	ldp 	x8, x9, [sp ,16 * 4]
	ldp 	x10, x11, [sp ,16 * 5]
	ldp 	x12, x13, [sp ,16 * 6]
	ldp 	x14, x15, [sp ,16 * 7]
	ldp 	x16, x17, [sp ,16 * 8]
	ldp 	x18, x19, [sp ,16 * 9]
	ldp 	x20, x21, [sp ,16 * 10]
	ldp 	x22, x23, [sp ,16 * 11]
	ldp 	x24, x25, [sp ,16 * 12]
	ldp 	x26, x27, [sp ,16 * 13]
	ldp 	x28, x29, [sp ,16 * 14]
	ldr 	x30, [sp, 16 * 15]
	add 	sp, sp, 16 * 18
.endm

exception_handler:
    save_all
    bl exception_entry
    load_all
    eret

sync_exception_handler:
    save_all
    mrs x0, esr_el1 // to decide is syscall or not
    mrs x1, elr_el1 // the address return to 
    mov x2, sp      // trapframe
    bl sync_exception_entry
    load_all
    eret

.align 11 // vector table should be aligned to 0x800
.global exception_vector_table
exception_vector_table:
  b exception_handler // branch to a handler function.
  .align 7 // entry size is 0x80, .align will pad 0
  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b sync_exception_handler //el1 to el1 (debug)
  .align 7
  b interrupt_handler //uart, timer
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b sync_exception_handler //el0 to el1 (user program)
  .align 7
  b interrupt_handler //el0 interrupt
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7
  b exception_handler
  .align 7

set_exception_vector_table:
  adr x0, exception_vector_table
  msr vbar_el1, x0
  ret

#define CORE0_TIMER_IRQ_CTRL 0x40000040

core_timer_enable:
    mov x0, 1
    msr cntp_ctl_el0, x0 // enable
    mrs x0, cntfrq_el0
    lsr x0, x0, #5
    msr cntp_tval_el0, x0 // set expired time
    mov x0, 2
    ldr x1, =CORE0_TIMER_IRQ_CTRL
    str w0, [x1] // unmask timer interrupt
    ret

interrupt_handler:
    save_all
    bl timer_scheduler//interrupt_handler_entry
    load_all
    eret


.global switch_to
switch_to:
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr, [x0, 16 * 5]
    mov x9, sp
    str x9, [x0, 16 * 6]

    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    ldp fp, lr, [x1, 16 * 5]
    ldp x9, x0, [x1, 16 * 6]
    mov sp,  x9
    dsb ish // ensure write has completed
    msr ttbr0_el1, x0 // switch translation based address.
    tlbi vmalle1is // invalidate all TLB entries
    dsb ish // ensure completion of TLB invalidatation
    isb // clear pipeline
    msr tpidr_el1, x1
    ret

.global get_current
get_current:
    mrs x0, tpidr_el1
    ret

.global fork_return
fork_return:
    load_all
    eret


.globl enable_irq
enable_irq:
	msr DAIFClr, 0xf // IRQ mask bit
	ret
