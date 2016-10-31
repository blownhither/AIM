#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <arch-trap.h>
#include <arch-mmu.h>

#define NIDT 256

static struct gatedesc idt[NIDT];
extern uint32_t vectors[];

void trap_init(void) {
	
}

void init_vectors() {


}

void trap(struct trapframe *tf) {
	if(tf->trapno == T_SYSCALL) {
		
	}
}