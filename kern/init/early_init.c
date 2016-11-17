/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 *
 * This file is part of AIM.
 *
 * AIM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <aim/console.h>
#include <aim/device.h>
#include <aim/early_kmmap.h>
#include <aim/init.h>
#include <aim/mmu.h>
#include <aim/panic.h>
#include <aim/kalloc.h>
#include <aim/pmm.h>
#include <aim/vmm.h>
#include <aim/initcalls.h>
#include <drivers/io/io-mem.h>
#include <drivers/io/io-port.h>
#include <platform.h>
#include <arch-init.h>
#include <arch-sync.h>
#include <mutex.h>
#include <asm.h>

void set_cr_mmu();

static inline
int early_devices_init(void)
{
#ifdef IO_MEM_ROOT
	if (io_mem_init(&early_memory_bus) < 0)
		return EOF;
#endif /* IO_MEM_ROOT */

#ifdef IO_PORT_ROOT
	if (io_port_init(&early_port_bus) < 0)
		return EOF;
#endif /* IO_PORT_ROOT */
	return 0;
}

typedef struct address_range_descriptor {
	uint64_t base;
	uint64_t length;
	uint64_t type;
} ARD;

#define ARD_ENTRY_ADDR 0x9000
#define ARD_COUNT_ADDR 0x8990
#define ARD_ENTRY_TARGET 3

static uint64_t __addr_base, __addr_length;

void get_mem_config() {
	// uint32_t n = (*(void **)ARD_COUNT_ADDR - (void *)ARD_ENTRY_ADDR) / sizeof(ARD);
	ARD *entry = (ARD *)ARD_ENTRY_ADDR;
	kprintf("Selected address range descriptor is :\n");
	kprintf("\t[%x%x, +%x%x], %x\n", 
		(uint32_t)entry[ARD_ENTRY_TARGET].base & 0xffffffff,
		(uint32_t)(entry[ARD_ENTRY_TARGET].base >> 32),
		(uint32_t)entry[ARD_ENTRY_TARGET].length & 0xffffffff,
		(uint32_t)(entry[ARD_ENTRY_TARGET].length >> 32), 
		(uint32_t)entry[ARD_ENTRY_TARGET].type
	);
	*(uint64_t *)((void *)&__addr_base) // - KERN_BASE) 
		= entry[ARD_ENTRY_TARGET].base;
	*(uint64_t *)((void *)&__addr_length) // - KERN_BASE) 
		= entry[ARD_ENTRY_TARGET].length;
}

__noreturn
void master_early_init(void)
{
	/* clear address-space-related callback handlers */
	early_mapping_clear();
	mmu_handlers_clear();
	/* prepare early devices like memory bus and port bus */

	if (early_devices_init() < 0)
		goto panic;
	/* other preperations, including early secondary buses */
	if (early_console_init(
		EARLY_CONSOLE_BUS,
		EARLY_CONSOLE_BASE,
		EARLY_CONSOLE_MAPPING
	) < 0)
		panic("Early console init failed.\n");
	kputs("Hello, world!\n");

	
	get_mem_config();

	arch_early_init();	// only go back to master_early_continue

	goto panic;

panic:
    sleep1();
    while(1);   // to suppress __noreturn warning, never here
    
}



extern addr_t *__early_buf_end;
void master_early_simple_alloc(void *start, void *end);
int page_allocator_init() {
	addr_t p_start = premap_addr(&__early_buf_end);
    if(__addr_base > p_start)
    	p_start = __addr_base;
    addr_t p_end = premap_addr(KERN_BASE + PHYSTOP);
    if(__addr_length + __addr_base < p_end)
    	p_end = __addr_length + __addr_base;
    page_alloc_init(p_start, p_end);
    kprintf("2. page allocator using [0x%p, 0x%p)\n", 
    	(void *)(uint32_t)p_start, (void *)(uint32_t)p_end
    );
    kprintf("\twith free space 0x%llx\n", get_free_memory());
    return 0;
}

//#define MZYDEBUG
#ifdef MZYDEBUG
static uint32_t last = 1678844458;
static uint32_t rand_local(uint32_t max) {
    return (last = ((last<<3) + last + 7) % max);
}

void alloc_test() {
    #define NTEST 1000
    struct pages pg = {0,0,0};
    addr_t temp_addr[NTEST];
    uint32_t temp_size[NTEST];
    kprintf("Available memory: 0x%lx\n", get_free_memory());
    for(int i=0; i<NTEST; ++i) {
        pg.size = rand_local(40960);
        temp_size[i] = pg.size;
        alloc_pages(&pg);
        temp_addr[i] = pg.paddr;
        kprintf("TEST: palloc [0x%lx, ",  pg.paddr);
        kprintf("+0x%lx]\n", pg.size);
    }
    kprintf("Available memory: 0x%lx", get_free_memory());

    for(int i=0; i<NTEST; ++i) {
        pg.size = temp_size[i];
        pg.paddr = temp_addr[i];
        free_pages(&pg);
        kprintf("TEST: free [0x%lx, ",  pg.paddr);
        kprintf("+0x%lx]\n", pg.size);
    }
    kprintf("Available memory: 0x%lx", get_free_memory());

    temp_addr[0] = pgalloc();
    kprintf("TEST: palloc at 0x%x\n",  temp_addr[0]);
}
#endif

void master_early_continue() {
	// using [__end, +EARLY_BUF)
    master_early_simple_alloc(
    	(void *)premap_addr((uint32_t)&__end), 
    	(void *)premap_addr(&__early_buf_end)
    );
    kprintf("1. early simple allocator using [0x%p, 0x%p)\n", 
    	(void *)premap_addr((uint32_t)&__end),
    	(void *)premap_addr(&__early_buf_end)
    );

	page_allocator_init();

    kprintf("3. later simple allocator depends on page allocator\n");
    master_later_alloc();

    // alloc_test();


    void __global_mzy();
    __global_mzy();
    
    mpinit();
    lapic_init();
    seginit();
    picinit();
    ioapic_init();
    do_initcalls();
    trap_init();

    
    /*
    kprintf("try int 0x20\n");
    
    __asm__ __volatile__ (
    	"mov $0x20, %%eax;"
    	" int $0x80;"
    	// "int $0x20;"
    	::
    );

    */
    
    startothers();

    // kputs("Successfully start other processors\n");

    // sti();

    // void panic_other_cpus();
    // panic_other_cpus();

    void main_test();
    main_test();
    // panic("Done with tests\n");

}

void inf_loop() {
    while(1);
}

#define CPU3_PRINT(x) if(quick_cpunum() == 5) kprintf("%d ",(x))

#define NAP 5
static lock_t lk = LOCK_INITIALIZER;
static semaphore_t sem = SEM_INITIALIZER(NAP);
static int critical_count = 300;
volatile static bool para_test_done = false;
void para_test() {
    semaphore_dec(&sem);
    while(sem.val > 0)              // sync all cpu to start together
        ;
    while(1) {
        spin_lock(&lk);             // enter critical section for countdown
        if(critical_count == 0) {   // about to finish the test
            kprintf("\ncpu %d done", quick_cpunum());
            semaphore_inc(&sem);    // submit work
            para_test_done = true;
            spin_unlock(&lk);       // unlock late for ordered output
            return;
        }
        kprintf("%d ", critical_count--);   // countdown
        spin_unlock(&lk);
    }
}

void main_test() {

    while(!para_test_done)
        ;
    int loop_count = 0;
    while(!(para_test_done && (sem.val == sem.limit)))  // every CPU submit
        ;

    kprintf("\n");
    panic("All processors finished para_test\n"); // panic all cpu
}



uint32_t __get_eip() {
  uint32_t eip;
  asm volatile(
    "push %%eax;"
    "call temp_get_pc_ax;"
    "mov %%eax, %0;"
    "pop %%eax;"
    "temp_get_pc_ax:"
    "  mov (%%esp), %%eax;"
    "  ret;"
    : "=m"(eip)
    :
  );
  return eip;
}