/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/memlayout.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>
#include <kern/pci.h>
#if defined(TEST_EPT_MAP)
int test_ept_map(void);
#endif

uint64_t end_debug;

static void boot_aps(void);

extern unsigned char mpentry_start[], mpentry_end[];

#ifdef VMM_GUEST

static void boot_virtual_aps(void);

int64_t vmcall(int num, int check, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
    int64_t ret;
    asm volatile("vmcall\n" : "=a" (ret) : "a" (num), "d" (a1), "c" (a2), "b" (a3), "D" (a4), "S" (a5) : "cc", "memory");
    if(check && ret > 0) panic("vmcall %d returned %d (> 0)", num, ret);
    return ret;
}
#endif


void
i386_init(void)
{
	/* __asm __volatile("int $12"); */

	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);

#ifdef VMM_GUEST
	/* Guest VMX extension exposure check */
	{
		uint32_t ecx = 0;
		cpuid(0x1, NULL, NULL, &ecx, NULL);
		if (ecx & 0x20)
			panic("[ERR] VMX extension exposed to guest.\n");
		else
			cprintf("VMX extension hidden from guest.\n");
	}
#endif

#ifndef VMM_GUEST
	extern char end[];
	end_debug = read_section_headers((0x10000+KERNBASE), (uintptr_t)end);
#endif

	// Lab 2 memory management initialization functions
	x64_vm_init();

	// Lab 3 user environment initialization functions
	env_init();
	trap_init();

#ifdef LAB4
#ifndef VMM_GUEST
	// Lab 4 multiprocessor initialization functions
	mp_init();
	lapic_init();
#endif

	// Lab 4 multitasking initialization functions
	pic_init();
#endif // LAB4
#ifdef LAB6
#ifndef VMM_GUEST  // Does not work in guest mode
	// Lab 6 hardware initialization functions
	time_init();
	pci_init();
#endif
#endif // LAB6

	// Acquire the big kernel lock before waking up APs
	// Your code here:

#ifdef LAB4
#ifndef VMM_GUEST
	// Starting non-boot CPUs
	boot_aps();
#endif
#endif // LAB4




#ifdef LAB5
	// Start fs.
	ENV_CREATE(fs_fs, ENV_TYPE_FS);
#endif

// LAB 2
#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	// Touch all you want.
#if defined(TEST_EPT_MAP)
	test_ept_map();
#endif

	ENV_CREATE(user_hello, ENV_TYPE_USER);
#endif // TEST*

#ifdef LAB4
	// Should not be necessary - drains keyboard because interrupt has given up.
	kbd_intr();

	// Schedule and run the first user environment!
	sched_yield();
#else
	// We only have one user environment for now, so just run it.
	env_run(&envs[0]);
#endif
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
static void
boot_aps(void)
{
	extern unsigned char mpentry_start[], mpentry_end[];
	void *code;
	struct CpuInfo *c;

	// Write entry code to unused memory at MPENTRY_PADDR
	code = KADDR(MPENTRY_PADDR);
	memmove(code, mpentry_start, mpentry_end - mpentry_start);
	// Boot each AP one at a time
	for (c = cpus; c < cpus + ncpu; c++) {
		if (c == cpus + cpunum())  // We've started already.
			continue;

		// Tell mpentry.S what stack to use 
		mpentry_kstack = percpu_kstacks[c - cpus] + KSTKSIZE;
		// Start the CPU at mpentry_start
		lapic_startap(c->cpu_id, PADDR(code));
		// Wait for the CPU to finish some basic setup in mp_main()
		while(c->cpu_status != CPU_STARTED)
			;
	}
}

// Setup code for APs
void
mp_main(void)
{
	// We are in high EIP now, safe to switch to kern_pgdir 
	lcr3(boot_cr3);
	cprintf("SMP: CPU %d starting\n", cpunum());

	lapic_init();
	env_init_percpu();
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up

	// Now that we have finished some basic setup, call sched_yield()
	// to start running processes on this CPU.  But make sure that
	// only one CPU can enter the scheduler at a time!
	//
	// Your code here:

	// Remove this after you finish Exercise 4
	for (;;);
}


/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic on CPU %d at %s:%d: ", cpunum(), file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
