/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
TRAPHANDLER_NOEC(_trap_DIVIDE, T_DIVIDE);
TRAPHANDLER_NOEC(_trap_DEBUG, T_DEBUG);
TRAPHANDLER_NOEC(_trap_NMI, T_NMI);
TRAPHANDLER_NOEC(_trap_BRKPT, T_BRKPT);
TRAPHANDLER_NOEC(_trap_OFLOW, T_OFLOW);
TRAPHANDLER_NOEC(_trap_BOUND, T_BOUND);
TRAPHANDLER_NOEC(_trap_ILLOP, T_ILLOP);
TRAPHANDLER_NOEC(_trap_DEVICE, T_DEVICE);
TRAPHANDLER(_trap_DBLFLT, T_DBLFLT);
TRAPHANDLER_NOEC(_trap_COPROC, T_DEFAULT);
TRAPHANDLER(_trap_TSS, T_TSS);
TRAPHANDLER(_trap_SEGNP, T_SEGNP);
TRAPHANDLER(_trap_STACK, T_STACK);
TRAPHANDLER(_trap_GPFLT, T_GPFLT);
TRAPHANDLER(_trap_PGFLT, T_PGFLT);
TRAPHANDLER_NOEC(_trap_RES, T_DEFAULT);
TRAPHANDLER_NOEC(_trap_FPERR, T_FPERR);
TRAPHANDLER(_trap_ALIGN, T_ALIGN);
TRAPHANDLER_NOEC(_trap_MCHK, T_MCHK);
TRAPHANDLER_NOEC(_trap_SIMDERR, T_SIMDERR);

TRAPHANDLER_NOEC(_trap_syscall, T_SYSCALL);

TRAPHANDLER_NOEC(_trap_default, T_DEFAULT);

TRAPHANDLER_NOEC(_irq_timer, IRQ_OFFSET + IRQ_TIMER);
TRAPHANDLER_NOEC(_irq_kbd, IRQ_OFFSET + IRQ_KBD);
TRAPHANDLER_NOEC(_irq_serial, IRQ_OFFSET + IRQ_SERIAL);
TRAPHANDLER_NOEC(_irq_spurious, IRQ_OFFSET + IRQ_SPURIOUS);
TRAPHANDLER_NOEC(_irq_ide, IRQ_OFFSET + IRQ_IDE);
TRAPHANDLER_NOEC(_irq_error, IRQ_OFFSET + IRQ_ERROR);

/*
 * Lab 3: _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */
.globl _alltraps;
.type _alltraps, @function;
.align 2;
_alltraps:
	subq $8,%rsp
	movw %es, (%rsp)
	subq $8,%rsp
	movw %ds, (%rsp)
	PUSHA

	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	movq %rsp, %rdi
	call trap

	movw (%rsp), %es
	add $8, %rsp
	movw (%rsp), %ds
	add $8, %rsp
	iret
