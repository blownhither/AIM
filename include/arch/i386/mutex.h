#ifndef _MUTEX_H_
#define _MUTEX_H_ 

#include <proc.h>

struct mutex {
	uint32_t locked;

	char *desc;
	struct cpu *cpu;
};

void initlock(struct mutex *m, char *desc);
int acquire(struct mutex *m);
int single_acquire(struct mutex *m);
int release(struct mutex *m);
bool holding(struct mutex *m);

void pushcli();
void popcli();

#define MUTEX_INITIALIZER {0, NULL, NULL}

#endif
