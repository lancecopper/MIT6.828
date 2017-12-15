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
	if(!(err & FEC_WR && uvpt[PGNUM(addr)] & PTE_COW)){
		cprintf("err : %x, PTE: %x\n", err, uvpt[PGNUM(addr)] & PTE_SYSCALL);
		panic("attempt to write an unwritable page!");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	if((r = sys_page_alloc(0, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	addr = ROUNDDOWN(addr, PGSIZE);
	memmove(PFTEMP, addr, PGSIZE);
	if((r = sys_page_map(0, PFTEMP, 0, addr, PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	if((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
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
	int r;

	// LAB 4: Your code here.
	uint32_t addr = pn * PGSIZE;
	int perm = PTE_U | PTE_P;
	// LAB 5_ex8: Yourr code here
	if(uvpt[pn] & PTE_SHARE){
		perm |= uvpt[pn] & PTE_SYSCALL;
	} else if(uvpt[pn] & (PTE_W | PTE_COW)){
		perm |= PTE_COW;
	}
	if((r = sys_page_map(0, (void *)addr, envid, (void *)addr, perm)) < 0)
		panic("sys_page_map: %e", r);
	if(perm & PTE_COW && !(perm & PTE_SHARE) && (r = sys_page_map(0, (void *)addr, 0, (void *)addr, perm)) < 0)
		panic("sys_page_map: %e", r);
	return 0;
}


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
	envid_t envid;
	int r, pn;
	extern void _pgfault_upcall(void);
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if(envid < 0)
		panic("sys_exofork: %e", envid);
	if(envid == 0){
		// We're the child.
		// modified for sfork
		// thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// We're the parent.
	for(int i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++){
		if(uvpd[i] & PTE_P){
			for(int j = 0; j < NPTENTRIES; j++){
				pn = PGNUM(PGADDR(i, j, 0));
				if(pn == PGNUM(UXSTACKTOP - PGSIZE))
					continue;
				if(uvpt[pn] & PTE_P)
					duppage(envid, pn);
			}
		}
	}
	if((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if((r = sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), 0, UTEMP, PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, (void *)(UXSTACKTOP - PGSIZE), PGSIZE);
	if((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
	if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall: %e", r);
	// Start the child environment running
	if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

// Challenge!
int
sfork(void)
{
	envid_t envid;
	int r, pn;
	extern void _pgfault_upcall(void);

	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if(envid < 0)
		panic("sys_exofork: %e", envid);
	if(envid == 0){
		return 0;
	}

	for(int i = PDX(UTEXT); i < PDX(UXSTACKTOP); i++){
		if(uvpd[i] & PTE_P){
			for(int j = 0; j < NPTENTRIES; j++){
				pn = PGNUM(PGADDR(i, j, 0));
				if(pn == PGNUM(UXSTACKTOP - PGSIZE))
					continue;
				if(pn == PGNUM(USTACKTOP - PGSIZE))
					duppage(envid, pn);
				if(uvpt[pn] & PTE_P){
					int perm = uvpt[pn] & PTE_SYSCALL;
					uint32_t addr = pn * PGSIZE;
					if((r = sys_page_map(0, (void *)addr, envid, (void *)addr, perm)) < 0)
						panic("sys_page_map: %e", r);;
					}
			}
		}
	}
	if((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if((r = sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), 0, UTEMP, PTE_U | PTE_P | PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, (void *)(UXSTACKTOP - PGSIZE), PGSIZE);
	if((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
	if((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall: %e", r);
	// Start the child environment running
	if((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}
