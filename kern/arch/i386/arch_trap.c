#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <arch-trap.h>
#include <arch-mmu.h>
#include <segment.h>
#include <aim/panic.h>
#include <asm.h>

#define NIDT 256

static struct gatedesc idt[NIDT];
extern uint32_t vectors[];

__noreturn
void trap_return(struct trapframe *tf) {
	while(1);
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
		// a(int num), args: b c d esi edi ebp
		// return at eax
		// handle_syscall(number)
	}
	else {
		panic("Implement me!");
	}
}

// init PIC (i8259)
#define PORT_PIC_MASTER 0x20
#define PORT_PIC_SLAVE  0xA0

void init_i8259(void) {
	/* mask all interrupts */
	outb(PORT_PIC_MASTER + 1, 0xFF);
	outb(PORT_PIC_SLAVE + 1 , 0xFF);
	
	/* start initialization */
	outb(PORT_PIC_MASTER, 0x11);
	outb(PORT_PIC_MASTER + 1, 32);
	outb(PORT_PIC_MASTER + 1, 1 << 2);
	outb(PORT_PIC_MASTER + 1, 0x3);
	outb(PORT_PIC_SLAVE, 0x11);
	outb(PORT_PIC_SLAVE + 1, 32 + 8);
	outb(PORT_PIC_SLAVE + 1, 2);
	outb(PORT_PIC_SLAVE + 1, 0x3);
	outb(PORT_PIC_MASTER, 0x68);
	outb(PORT_PIC_MASTER, 0x0A);
	outb(PORT_PIC_SLAVE, 0x68);
	outb(PORT_PIC_SLAVE, 0x0A);
}

void trap_init(void) {
	init_vectors();	// prepare int vectors

	//TODO: lapic
	//TODO: ioapic

	// init PIC (i8259)
	init_i8259();

	//TODO: outside int ?

	// int not enabled in this function
}



