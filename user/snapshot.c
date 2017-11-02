// display how to get a snapshot of a child process,
// and restart it from the snapshot.

#include <inc/string.h>
#include <inc/lib.h>


envid_t dumbfork(void);

void
umain(int argc, char **argv)
{
	envid_t who;
	int i, r;
	// fork a child process
	who = dumbfork();

	// print a message and yield to the other a few times
  for (i = 0; i < (who ? 20 : 20); i++) {
		cprintf("%d: I am the %s!\n", i, who ? "parent" : "child");
    if(who && i == 5){
      if ((r = sys_page_alloc(0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
    		panic("sys_page_alloc: %e", r);
      sys_checkpoint(who, UTEMP);
    }
    if(who && i == 10){
      sys_restart(who, UTEMP);
      if ((r = sys_page_unmap(0, UTEMP)) < 0)
    		panic("sys_page_unmap: %e", r);
    }
		sys_yield();
	}
}

void
duppage(envid_t dstenv, void *addr)
{
	int r;
	// This is NOT what you should do in your fork.
	if ((r = sys_page_alloc(dstenv, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if ((r = sys_page_map(dstenv, addr, 0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, addr, PGSIZE);
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
}

envid_t
dumbfork(void)
{
	envid_t envid;
	uint8_t *addr;
	int r;
	extern unsigned char end[];

	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		// thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// We're the parent.
	// Eagerly copy our entire address space into the child.
	// This is NOT what you should do in your fork implementation.
	for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE)
		duppage(envid, addr);
	// Also copy the stack we are currently running on.
	duppage(envid, ROUNDDOWN(&addr, PGSIZE));

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}
