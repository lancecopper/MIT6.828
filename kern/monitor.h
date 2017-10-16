#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

extern void env_pop_tf(struct Trapframe *tf);

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmp(int argc, char **argv, struct Trapframe *tf);
int mon_changeperm(int argc, char **argv, struct Trapframe *tf);
int mon_dump(int argc, char **argv, struct Trapframe *tf);
int debug_si(int argc, char **argv, struct Trapframe *tf);
int debug_continue(int argc, char **argv, struct Trapframe *tf);
int debug_pr_tf(int argc, char **argv, struct Trapframe *tf);
int debug_exit(int argc, char **argv, struct Trapframe *tf);



#endif	// !JOS_KERN_MONITOR_H
