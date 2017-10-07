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

#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

extern pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
extern pte_t *kern_pgdir;
extern size_t npages;
extern struct PageInfo *pages;

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
	{	"showmp", "Display physical page mappings and permission bits", mon_showmp },
	{ "changeperm", "Change permission bits", mon_changeperm},
	{	"dump",
		"dump Dump the contents of a range of memory given either a virtual or physical address range",
		mon_dump},

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

int
mon_showmp(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va, begin, end;
	physaddr_t phaddr;
	pte_t *pte;

	if(argc != 3){
	  cprintf("Usage: showmappings begin end\n");
		return 0;
	}
	begin = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	end = ROUNDUP(strtol(argv[2], NULL, 16), PGSIZE);
	if (end > 0xffffffff) {
	    cprintf("end overflow\n");
	    return 0;
	}
	// cprintf("begin: %x, end: %x\n", begin, end);
	for(va = begin; va <= end; va += PGSIZE){
	  cprintf("va: %08x, ", va);
	  pte = pgdir_walk(kern_pgdir, (void *)va, 0);
	  if(!pte){
	    cprintf("unmapped\n");
	  }else{
	    phaddr = (physaddr_t) (*pte) & ~0xFFF;
	    cprintf("pa: %08x, ", phaddr);
	    cprintf("PTE_P: %x, PTE_W: %x, PTE_U: %x\n", (*pte) & PTE_P, (*pte) & PTE_W && 1, (*pte) & PTE_U && 1);
	  }
		if(va == 0xfffff000)
			break;
	}
	return 0;
}

int mon_changeperm(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va;
	pte_t *	pte;
	int opflag;
	char *pnt;
	int perm = 0;

	if(argc != 4){
		cprintf("Usage: changeperm va op flags\n");
		return 0;
	}
	va = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	pte = pgdir_walk(kern_pgdir, (void *)va, 0);
	if(!pte){
		cprintf("unmapped\n");
		return 0;
	}
	if(!strcmp(argv[2], "set"))
		opflag = 0;
	else if(!strcmp(argv[2], "clear"))
		opflag = 1;
	else{
		cprintf("unrecoginized op, permitted op: 'set', 'clear'\n");
		return 0;
	}
	switch(opflag){
		case 0:{
			perm = 0;
			for(pnt = argv[3]; *pnt != '\0'; pnt++){
				switch (*pnt){
					case 'p':
					case 'P':{
						perm |= PTE_P;
						break;
					}
					case 'w':
					case 'W':{
						perm |= PTE_W;
						break;
					}
					case 'u':
					case 'U':{
						perm |= PTE_U;
						break;
					}
					default:{
						cprintf("unrecoginized perm_bit, permitted perm_bit: 'P', 'W', 'U'\n");
					}
				}
			}
			*pte |= perm;
			break;
		}
		case 1:{
			perm = ~0;
			for(pnt = argv[3]; *pnt != '\0'; pnt++)
				switch (*pnt){
					case 'p':
					case 'P':{
						perm &= ~PTE_P;
						break;
					}
					case 'w':
					case 'W':{
						perm &= ~PTE_W;
						break;
					}
					case 'u':
					case 'U':{
						perm &= ~PTE_U;
						break;
					}
					default:{
						cprintf("unrecoginized perm_bit, permitted perm_bit: 'P', 'W', 'U'\n");
					}
				}
			*pte &= perm;
			break;
		}
		default:{
			cprintf("unexpected error: illegal opflag!\n");
			return 0;
		}
	}
	return 0;
}

int mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t va, vbegin, vend;
	physaddr_t pa, pbegin, pend;
	int addrflag, out_calc;
	pte_t *pte = NULL;

	if(argc != 4){
		cprintf("Usage: dump va/pa begin end\n");
		return 0;
	}
	if(!strcmp(argv[1], "va")){
		addrflag = 0;
	}else if(!strcmp(argv[1], "pa")){
		addrflag = 1;
	}else{
		cprintf("unrecoginized addrflag, permitted addrflag: 'va', 'pa'\n");
		return 0;
	}
	switch ((addrflag)) {
		case 0:{
			vbegin = strtol(argv[2], NULL, 16);
			vend = strtol(argv[3], NULL, 16);
			if (vend > 0xffffffff) {
			    cprintf("end overflow\n");
			    return 0;
			}
			out_calc = 0;
			for(va=ROUNDDOWN(vbegin, 4); va<=ROUNDUP(vend, 4);){
				if(!pte || ROUNDDOWN(va, PGSIZE) == va){
					pte = pgdir_walk(kern_pgdir, (void *)va, 0);
				}
				if(!pte){
					cprintf("\n\n0x%08x~0x%08x: unmapped\n", va, ROUNDDOWN(va, PGSIZE) + PGSIZE);
					va = ROUNDDOWN(va, PGSIZE) + PGSIZE;
					out_calc = 0;
				}else{
					if(!out_calc)
						cprintf("\n0x%08x:  ", va);
					cprintf("0x%08x,	", *((uint32_t *)va));
					va += 4;
				}
				out_calc = (out_calc + 1) % 4;
			}
			cprintf("\n");
			break;
		}
		case 1:{
			pbegin = strtol(argv[2], NULL, 16);
			pend = strtol(argv[3], NULL, 16);
			if (pend >= npages * PGSIZE) {
			    cprintf("end overflow\n");
			    return 0;
			}
			out_calc = 0;
			for(pa = ROUNDDOWN(pbegin, 4); pa <= ROUNDUP(pend, 4); pa += 4){
			 	if(!out_calc)
			 		cprintf("\n0x%08x:  ", pa);
	 			cprintf("0x%08x,	", *(uint32_t *)(KADDR(pa)));
			 	out_calc = (out_calc + 1) % 4;
			}
			cprintf("\n");
			break;
		}
		default:
			cprintf("unexpected error: illegal addrflag!\n");
			return 0;
	}
	return 0;
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
