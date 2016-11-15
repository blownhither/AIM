#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <mutex.h>
#include <asm.h>
#include <proc.h>

void initlock(struct mutex *m, char *desc){
	m->desc = desc;
	m->locked = 0;
	m->cpu = NULL;
}

int single_acquire(struct mutex *m) {
	pushcli();
	//TODO: test holding here
	//while(xchg(&m->locked, 1) != 0)
	//	;
	if(xchg(&m->locked, 1) != 0) {
		popcli();
		return 0;
	}
	// __snyc_synchronize();
	//TODO: set up lock cpu info
	return 1;
}

int acquire(struct mutex *m) {
	pushcli();
	//TODO: test holding here
	//while(xchg(&m->locked, 1) != 0)
	//	;
	while(xchg(&m->locked, 1) != 0) {
		return 0;
	}
	// __snyc_synchronize();
	//TODO: set up lock cpu info
	return 1;
}

int release(struct mutex *m){
	//TODO: Test holding
	if(!m->locked)
		panic("Trying to release unlocked lock");
	m->cpu = NULL;
	// __snyc_synchronize();
	asm volatile("movl $0, %0" : "+m" (m->locked) : );
	popcli();
	return 1;
}

bool holding(struct mutex *m) {
	panic("Implement me: need cpu info in lock");
}

void pushcli() {
	int eflags = readeflags();
	cli();

	struct cpu *cpu = get_gs_cpu();

	if(cpu->ncli == 0) {
		cpu->intena = eflags & FL_IF;
	}
	cpu->ncli ++;
}

void popcli() {
	if(readeflags() & FL_IF) 
		panic("popcli: Interruptable");

	struct cpu *cpu = get_gs_cpu();
	
	if(--cpu->ncli < 0)
		panic("popcli: Illegal behaviour");
	if(cpu->ncli == 0 && cpu->intena) {
		sti();
	}
}