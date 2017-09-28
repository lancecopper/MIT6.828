// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a listing of function call frames", mon_backtrace },
	{	"showmappings", "Display physical page mappings and permission bits", mon_showmappings },

};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	int i;
	struct Eipdebuginfo info;
	uint32_t ebp, eip, *args;
	const char *eip_fn_name;

	ebp = read_ebp();
	eip = *((uint32_t *)ebp + 1);
	debuginfo_eip(eip, &info);
	cprintf("Stack backtrace:\n");

	while(ebp){
	    cprintf(" ebp %08x eip %08x args", ebp, eip);
	    args = (uint32_t *)ebp + 2;
	    for(i = 0; i < 5; i++){
	        cprintf(" %08x", args[i]);
	    }
	    cprintf("\n       %s:%d: ", info.eip_file, info.eip_line);
	    eip_fn_name = info.eip_fn_name;
	    for(i = info.eip_fn_namelen; i > 0; i--){
	        cprintf("%c", *eip_fn_name);
	        eip_fn_name += 1;
	    }
	    cprintf("+%u\n", (unsigned)(eip - info.eip_fn_addr));
	    ebp = ((uint32_t *)ebp)[0];
	    eip = ((uint32_t *)ebp)[1];
	    debuginfo_eip(eip, &info);
	}
	return 0;
}

int mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	extern pte_t * pgdir_walk(pde_t *pgdir, const void *va, int create);
	extern pte_t * kern_pgdir;
	uintptr_t va, begin_addr, end_addr;
	physaddr_t phaddr;
	pte_t　*pte;

	if(argc != 3){
		cprintf("Usage: showmappings begin_addr end_addr\n");
	}
	begin_addr = strtol(argv[1], NULL, 16);
	end_addr = strtol(argv[2], NULL, 16);
	for(va = begin_addr; va < end_addr; va += PGSIZE){
		pte = pgdir_walk(kern_pgdir, va, 0);
		phaddr = (physaddr_t) (pte) & ~0xFFF;
		printf("%x\n", );
	}
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
