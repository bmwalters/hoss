// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

	panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	// LAB 4
	int r;
	int perm = uvpt[pn] & 0xFFF;

	if (perm & (PTE_W | PTE_COW)) {
		perm |= PTE_COW;
	}

	sys_page_map(0, src_pg, envid, dst_pg, perm);
	sys_page_map(0, src_pg, 0, dst_pg, perm);

	return 0;
}

extern void _pgfault_upcall(void);

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	envid_t id;
	int res;
	void *va;

	// LAB 4
	if ((res = set_pgfault_handler(pgfault)) < 0) {
		return res;
	}

	if ((id = sys_exofork()) < 0) {
		return id;
	}

	if (id == 0) {
		// child
		thisenv = &envs[ENVX(sys_getenvid())];
		return id;
	}

	// parent
	for (va = 0; va < UTOP; va += PGSIZE) {
		if ((uvpd[VPD(va)] & PTE_P) != PTE_P) {
			continue;
		}

		if ((uvpt[VPN(va)] & (PTE_P | PTE_U)) != (PTE_P | PTE_U)) {
			continue;
		}

		// TODO: handle read-only pages differently?
		duppage(id, VPN(va));
	}

	if ((res = sys_page_alloc(id, (void *)(UXSTACKTOP - PGSIZE), PTE_W)) < 0) {
		panic("fork: sys_page_alloc: %e\n", res);
	}

	if ((res = sys_env_set_pgfault_upcall(id, (void *)_pgfault_upcall)) < 0) {
		panic("fork: sys_env_set_pgfault_upcall: %e\n", res);
	}

	if ((res = sys_env_set_status(id, ENV_RUNNABLE)) < 0) {
		// should never happen since id was newly created
		panic("fork: sys_env_set_status: %e\n", res);
	}

	return id;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
