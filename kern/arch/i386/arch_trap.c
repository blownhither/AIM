#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <arch-trap.h>
#include <arch-mmu.h>
#include <segment.h>

#define NIDT 256

static struct gatedesc idt[NIDT];
extern uint32_t vectors[];

void trap_init(void) {
	init_vectors();	// prepare int vectors

	// int not enabled in this function
}

void init_vectors() {
	for(int i=0; i < NIDT; ++i) {
		SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
	}
	SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

	// initlock(&ticklock, "time");
}

void trap(struct trapframe *tf) {
	if(tf->trapno == T_SYSCALL) {

	}
	else {
		panic("Implement me!");
	}
}