// program to cause a breakpoint trap

#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	cprintf("Enter breakpoint_test!\n");
	asm volatile("int $3");
	for(int i = 0; i < 10; i++)
		cprintf("Round calc: %d\n", i);
	cprintf("Breakpoint_test is over!\n");
}
