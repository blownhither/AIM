/* Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
 *
 * This file is part of AIMv6.
 *
 * AIMv6 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AIMv6 is distributed in the hope that it will be useful,
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

#include "sys/types.h"
#include "aim/init.h"
#include "aim/kalloc.h"
#include "aim/mmu.h"


void set_control_registers();

__noreturn 
void master_early_init(void)
{
	arch_early_init();
    
    
    set_control_registers();    // also jmp to new target


	goto panic;

panic:
    sleep1();
    while(1);   // to suppress __noreturn warning, never here
    
}


void inf_loop() {
    while(1);
}

void continue_early_init(void) {
    kinit1((void *)__end, (void *)postmap_addr(4*1024*1024));
    inf_loop();
}




