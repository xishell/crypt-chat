/*
 * DTEK-V RISC-V Boot and Interrupt Handler
 *
 * This file provides:
 * - Boot sequence initialization (_start)
 * - Interrupt and exception handling (_isr_handler)
 * - Context save/restore for trap handling
 * - BSS section initialization
 * - Interrupt enable function
 */

.data
.align 2
welcome_msg: .asciz "================================================\n===== RISC-V Boot-Up Process Now Complete ======\n================================================\n"

.section .text
.align 2
.globl _start

/*
 * Interrupt/Exception Vector Table
 * Entry point for all traps (interrupts and exceptions)
 */
_isr_handler:
	j _isr_routine     /* Jump to ISR service routine */
	j _start           /* Hard reset vector */
	
/*
 * ISR: Interrupt Service Routine
 * Handles all interrupts and exceptions by:
 * 1. Saving processor context
 * 2. Calling appropriate C handler
 * 3. Restoring context
 * 4. Returning via mret
 */
_isr_routine:
	/* Reserve stack space for 31 registers (x1-x31, excluding x2/sp) */
 	addi sp, sp, -124

	/* Save all registers to stack (x2/sp excluded as it's being used) */
	sw x1,   0(sp)   /* ra  - return address */
	sw x3,   4(sp)   /* gp  - global pointer */
	sw x4,   8(sp)   /* tp  - thread pointer */
	sw x5,  12(sp)   /* t0  - temporary */
	sw x6,  16(sp)   /* t1  - temporary */
	sw x7,  20(sp)   /* t2  - temporary */
	sw x8,  24(sp)   /* s0  - saved / frame pointer */
	sw x9,  28(sp)   /* s1  - saved */
	sw x10, 32(sp)   /* a0  - argument/return value */
	sw x11, 36(sp)   /* a1  - argument/return value */
	sw x12, 40(sp)   /* a2  - argument */
	sw x13, 44(sp)   /* a3  - argument */
	sw x14, 48(sp)   /* a4  - argument */
	sw x15, 52(sp)   /* a5  - argument */
	sw x16, 56(sp)   /* a6  - argument */
	sw x17, 60(sp)   /* a7  - argument */
	sw x18, 64(sp)   /* s2  - saved */
	sw x19, 68(sp)   /* s3  - saved */
	sw x20, 72(sp)   /* s4  - saved */
	sw x21, 76(sp)   /* s5  - saved */
	sw x22, 80(sp)   /* s6  - saved */
	sw x23, 84(sp)   /* s7  - saved */
	sw x24, 88(sp)   /* s8  - saved */
	sw x25, 92(sp)   /* s9  - saved */
	sw x26, 96(sp)   /* s10 - saved */
	sw x27, 100(sp)  /* s11 - saved */
	sw x28, 104(sp)  /* t3  - temporary */
	sw x29, 108(sp)  /* t4  - temporary */
	sw x30, 112(sp)  /* t5  - temporary */
	sw x31, 116(sp)  /* t6  - temporary */
	
	/* Read mcause to determine exception or interrupt */
	csrr t0, mcause

	/* Check if interrupt (MSB set when mcause is negative) */
	bltz t0, external_irq

	/* Exception handling: prepare arguments for handle_exception() */
	add a6, t0, zero        /* a6 = mcause */

	/* Check if ecall (mcause == 11): ecall uses a0-a7 for args, don't overwrite */
	addi t1, zero, 11
	beq t0, t1, skip_init_args

	/* For other exceptions, pass mepc as first argument */
	csrr a0, mepc

skip_init_args:
	jal handle_exception

	/* Increment mepc by 4 to skip the faulting instruction */
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0
	j restore

external_irq:
	/* Mask off MSB to get interrupt cause, pass to handle_interrupt() */
	li t0, 0x7FFFFFFF
	csrr t1, mcause
	and a0, t0, t1          /* a0 = interrupt cause (lower 31 bits) */
	jal handle_interrupt
	/* Note: interrupts return to same PC, no mepc increment needed */

restore:
	/* Restore all registers from stack */
	lw x1,   0(sp)   /* ra */
	lw x3,   4(sp)   /* gp */
	lw x4,   8(sp)   /* tp */
	lw x5,  12(sp)   /* t0 */
	lw x6,  16(sp)   /* t1 */
	lw x7,  20(sp)   /* t2 */
	lw x8,  24(sp)   /* s0 */
	lw x9,  28(sp)   /* s1 */
	lw x10, 32(sp)   /* a0 */
	lw x11, 36(sp)   /* a1 */
	lw x12, 40(sp)   /* a2 */
	lw x13, 44(sp)   /* a3 */
	lw x14, 48(sp)   /* a4 */
	lw x15, 52(sp)   /* a5 */
	lw x16, 56(sp)   /* a6 */
	lw x17, 60(sp)   /* a7 */
	lw x18, 64(sp)   /* s2 */
	lw x19, 68(sp)   /* s3 */
	lw x20, 72(sp)   /* s4 */
	lw x21, 76(sp)   /* s5 */
	lw x22, 80(sp)   /* s6 */
	lw x23, 84(sp)   /* s7 */
	lw x24, 88(sp)   /* s8 */
	lw x25, 92(sp)   /* s9 */
	lw x26, 96(sp)   /* s10 */
	lw x27, 100(sp)  /* s11 */
	lw x28, 104(sp)  /* t3 */
	lw x29, 108(sp)  /* t4 */
	lw x30, 112(sp)  /* t5 */
	lw x31, 116(sp)  /* t6 */

	/* Reclaim stack space */
 	addi sp, sp, 124

	/* Return from trap (exception or interrupt) */
	mret

	/* Application entry point */
_start:
	/* Disable interrupts during initialization */
	csrw mie, x0

	/* Set machine trap vector to our ISR handler */
	la t0, _isr_handler
	csrw mtvec, t0

	/* Initialize stack pointer */
	la sp, _stack_end

	/* Initialize global pointer for relaxed addressing */
	la gp, __global_pointer

	/* Clear BSS section (uninitialized data) */
	la t0, _bss_start
	la t1, _bss_end
clear_bss:
	bgeu t0, t1, bss_done
	sw zero, 0(t0)
	addi t0, t0, 4
	j clear_bss
bss_done:

	/* Print welcome message via ecall */
	la a0, welcome_msg
	li a7, 4
	ecall

	/* Call main function */
	jal main

	/* Infinite loop if main returns */
loop:
	j loop

/*
 * enable_interrupt
 * Enables machine-mode interrupts for all DTEK-V devices
 * - Enables timer interrupt (IRQ 16)
 * - Enables switch interrupt (IRQ 17)
 * - Enables button interrupt (IRQ 18)
 * - Sets global interrupt enable (MSTATUS.MIE bit 3)
 */
.globl enable_interrupt
enable_interrupt:
	csrsi mie, 16          /* Enable IRQ 16 - Timer */
	csrsi mie, 17          /* Enable IRQ 17 - Switches */
	csrsi mie, 18          /* Enable IRQ 18 - Button */
	li t0, (1 << 3)        /* Bit 3 = MIE (machine interrupt enable) */
	csrs mstatus, t0       /* Set MSTATUS.MIE */
	ret
