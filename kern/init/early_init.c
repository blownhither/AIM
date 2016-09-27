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

#include <sys/types.h>
#include <aim/init.h>

void sleep1();

// __noreturn
void master_early_init(void)
{
	arch_early_init();

	goto panic;

panic:
    sleep1();
}


void sleep1(){
    /*  // nice try... but int80 is not yet implemented
    __asm__("mov $0x1,  %%ecx ;mov $0x0,  %%edx ;"::);
    __asm__("pushl  %%eax ;pushl  %%ebx ;pushl  %%ecx ;"::);
    __asm__("mov  %%ecx, -0x8( %%esp);mov  %%edx, -0x4( %%esp);mov $162,  %%eax ;"::);
    __asm__("lea -0x8( %%esp),  %%ebx ;xor  %%ecx,  %%ecx ;int $0x80 ;"::);
    __asm__("pop  %%ecx ;pop  %%ebx ;pop  %%eax ;ret;"::);
    */
    __asm__("cli;"::);
    while(1)
        __asm__("hlt;"::);

}
