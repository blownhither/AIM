#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <proc.h>
#include <asm.h>
#include <segment.h>
#include <arch-init.h>
#include <arch-mmu.h>
#include <aim/console.h>
#include <aim/pmm.h>
#include <libc/string.h>


struct cpu cpus[NCPU];
int ismp;
int ncpu;
uchar ioapicid;


void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu and proc -- these are private per cpu.
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);

  // Initialize cpu-local storage.
  // Note: cpu is defined in proc.h as %gs
  // TODO: cpu = c;
  // set_gs_cpu(c);
  // TODO: proc = 0;

}

// Common CPU setup code.
void
mpmain(void)
{
  //cprintf("cpu%d: starting\n", cpunum());
  kprintf("cpu%d: starting\n", cpunum());
  idt_init();       // load idt register

  panic("This cpu is on!");

  //TODO: xchg(&c->started, 1);

  //TODO: scheduler();     // start running processes
}

// Other CPUs jump here from entryother.S.
static void
mpenter(void)
{
  //TODO: switchkvm();
  seginit();
  lapic_init();
  mpmain();
}

// extern pde_t entrypgdir[];  // For entry.S
void entryother_start();  // found in entryothers.S
void entryother_end();

struct segdesc mp_gdt[3] = {
  SEG(0,0,0,0),
  SEG(STA_X|STA_R, 0, 0xffffffff, 0),
  SEG(STA_W, 0, 0xffffffff, 0)
};

void
startothers(void)
{
  // extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // Write entry code to unused memory at 0x7000.
  // The linker has placed the image of entryother.S in
  // _binary_entryother_start.
  code = (uchar *)(0x7000);
  
  //TODO: if we trust 0x7000?
  //TODO: memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);
  memmove(code, (void *)entryother_start, (uint)(entryother_end - entryother_start));

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == cpus+cpunum())  // We've started already.
      continue;

    // Tell entryother.S what stack to use, where to enter, and what
    // pgdir to use. We cannot use kpgdir yet, because the AP processor
    // is running in low  memory, so we use entrypgdir for the APs too.
    // stack = (char *)kalloc();
    stack = (char *)(uint32_t)pgalloc();
    *(void**)(code-4) = stack + KSTACKSIZE;	// used as temp stack
    *(void**)(code-8) = mpenter;	// used as callback
    *(int**)(code-12) = (void *) kva2pa(entrypgdir);
    *(struct segdesc **)(code-16) = mp_gdt;
    lapicstartap(c->apicid, kva2pa(code));

    // wait for cpu to finish mpmain()
    while(c->started == 0)
      ;
  }
}