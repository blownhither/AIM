/* Copyright (C) 2016 Gan Quan <coin2028@hotmail.com>
 * Copyright (C) 2016 David Gao <davidgao1001@gmail.com>
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
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <aim/console.h>
#include <aim/device.h>
#include <aim/mmu.h>
#include <aim/vmm.h>
#include <aim/gfp.h>

#include <uart-ns16550-hw.h>

#ifdef i386
#define NS16550_PORTIO		/* cases where NS16550 is on a port I/O bus */
#endif

/* internal routines */

static struct chr_device __early_uart_ns16550 = {
	.class = DEVCLASS_CHR,
};

static inline
void __uart_ns16550_init(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);

	if (bus_write8 == NULL)
		return;		/* should panic? */

	/* TODO: check if the following configuration works across all
	 * UARTs */
	bus_write8(bus, inst->base, UART_FIFO_CONTROL,
	    UART_FCR_RTB_4 | UART_FCR_RST_TRANSMIT | UART_FCR_RST_RECEIVER |
	    UART_FCR_ENABLE);
	bus_write8(bus, inst->base, UART_LINE_CONTROL, UART_LCR_DLAB);
	bus_write8(bus, inst->base, UART_DIVISOR_LSB,
	    (UART_FREQ / UART_BAUDRATE) & 0xff);
	bus_write8(bus, inst->base, UART_DIVISOR_MSB,
	    ((UART_FREQ / UART_BAUDRATE) >> 8) & 0xff);
	bus_write8(bus, inst->base, UART_LINE_CONTROL,
	    UART_LCR_DATA_8BIT |
	    UART_LCR_STOP_1BIT |
	    UART_LCR_PARITY_NONE);
}

static inline
void __uart_ns16550_enable(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);

	if (bus_write8 == NULL)
		return;		/* should panic? */

	bus_write8(bus, inst->base, UART_MODEM_CONTROL,
	    UART_MCR_RTSC | UART_MCR_DTRC);
}

static inline
void __uart_ns16550_disable(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);

	if (bus_write8 == NULL)
		return;		/* should panic? */

	bus_write8(bus, inst->base, UART_MODEM_CONTROL, 0);
}

static inline
void __uart_ns16550_enable_interrupt(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);

	if (bus_write8 == NULL)
		return;		/* should panic? */

	bus_write8(bus, inst->base, UART_INTR_ENABLE, UART_IER_RBFI);
}

static inline
void __uart_ns16550_disable_interrupt(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);

	if (bus_write8 == NULL)
		return;		/* should panic? */

	bus_write8(bus, inst->base, UART_INTR_ENABLE, 0);
}

static inline
unsigned char __uart_ns16550_getchar(struct chr_device *inst)
{
	struct bus_device *bus = inst->bus;
	bus_read_fp bus_read8 = bus->bus_driver.get_read_fp(bus, 8);
	uint64_t buf;

	if (bus_read8 == NULL)
		return 0;		/* should panic? */
	do {
		bus_read8(bus, inst->base, UART_LINE_STATUS, &buf);
	} while (!(buf & UART_LSR_DATA_READY));

	bus_read8(bus, inst->base, UART_RCV_BUFFER, &buf);
	return (unsigned char)buf;
}

static inline
int __uart_ns16550_putchar(struct chr_device *inst, unsigned char c)
{
	struct bus_device *bus = inst->bus;
	bus_read_fp bus_read8 = bus->bus_driver.get_read_fp(bus, 8);
	bus_write_fp bus_write8 = bus->bus_driver.get_write_fp(bus, 8);
	uint64_t buf;

	if (bus_read8 == NULL || bus_write8 == NULL)
		return EOF;

	do {
		bus_read8(bus, inst->base, UART_LINE_STATUS, &buf);
	} while (!(buf & UART_LSR_THRE));

	bus_write8(bus, inst->base, UART_TRANS_HOLD, c);
	return 0;
}

/* Meant to register to kernel, so this interface routine is static */
static inline
int early_console_putchar(int c)
{
	__uart_ns16550_putchar(&__early_uart_ns16550, c);
	return 0;
}

static inline
void __early_console_set_bus(struct bus_device *bus, addr_t base)
{
	__early_uart_ns16550.bus = bus;
	__early_uart_ns16550.base = base;
}

static struct bus_device *__mapped_bus;
static addr_t __mapped_base;

static inline
void __jump_handler(void)
{
	__early_console_set_bus(__mapped_bus, __mapped_base);
	set_console(early_console_putchar, DEFAULT_KPUTS);
}

int __early_console_init(struct bus_device *bus, addr_t base, addr_t mapped_base)
{
	__early_console_set_bus(bus, base);
	__mapped_bus = (struct bus_device *)postmap_addr(bus);
	__mapped_base = mapped_base;

	__uart_ns16550_init(&__early_uart_ns16550);
	__uart_ns16550_enable(&__early_uart_ns16550);

	//TODO: 
	set_console(early_console_putchar, (void *)premap_addr(DEFAULT_KPUTS));

	if (jump_handlers_add((generic_fp)__jump_handler) != 0)
		for (;;) ;	/* panic */
	return 0;
}

int __console_init(struct bus_device *bus, addr_t base, addr_t mapped_base)
{
	__early_console_set_bus(bus, base);
	__mapped_bus = (struct bus_device *)postmap_addr(bus);
	__mapped_base = mapped_base;

	__uart_ns16550_init(&__early_uart_ns16550);
	__uart_ns16550_enable(&__early_uart_ns16550);

	//TODO: 
	set_console(early_console_putchar, DEFAULT_KPUTS);

	if (jump_handlers_add((generic_fp)__jump_handler) != 0)
		for (;;) ;	/* panic */
	return 0;
}

#include <platform.h>
#include <aim/initcalls.h>
#include <drivers/io/io-mem.h>
#include <drivers/io/io-port.h>

// static struct chr_device *__global_uart;

static int console_putc(int c) {
	//TODO: check device
	struct chr_device *adev = (struct chr_device *)dev_from_name("uart-ns16550");
	// struct chr_device *adev = &__early_uart_ns16550;
	
	return __uart_ns16550_putchar(adev, c);
}

static int driver_getc(dev_t dev) {
	struct chr_device *adev = (struct chr_device *)dev_from_id(dev);	
	return __uart_ns16550_getchar(adev);
	// return __uart_ns16550_getchar(&__early_uart_ns16550);
}

static int driver_putc(dev_t dev, int c) {
	struct chr_device *adev = (struct chr_device *)dev_from_id(dev);	
	return __uart_ns16550_putchar(adev, c); 
}

static struct chr_driver drv = {
	.class 	= DEVCLASS_CHR,
	.type 	= DRVTYPE_TTY,	//TODO: check _NORMAL?
	.open 	= NULL,
	.close 	= NULL,
	.new 	= NULL,
	.read 	= NULL,
	.write 	= NULL,
	.getc 	= driver_getc,
	.putc 	= driver_putc

};

static int __driver_init(void) {
	
	// addr_t mapped_base;

	// int ret = get_mapped_base(base, type, &mapped_base);
	// if (ret < 0) return ret; // caused by kmmap, may have various reasons

/*
	if (__console_init(
		EARLY_CONSOLE_BUS,
		UART_BASE,
		UART_BASE
	) < 0)
		panic("Early console init failed.\n");
*/
	struct device *port;
	port = dev_from_name("portio");
	
	struct chr_device *uart;
	uart = kmalloc(sizeof(*uart), GFP_ZERO);
	uart->bus = (struct bus_device *)port;	
	//uart->base = port->base;
	uart->base = UART_BASE;

	// __global_uart = uart;

	register_driver(NOMAJOR, &drv);
	initdev(uart, DEVCLASS_CHR, "uart-ns16550", NODEV, &drv);
	dev_add(uart);

	set_console(console_putc, DEFAULT_KPUTS);

	//kprintf("Hello World in __driver_init");

	return 0;
}
//TODO: DEV?
INITCALL_DEV(__driver_init);

#ifdef RAW /* baremetal driver */

#else /* not RAW, or kernel driver */

#endif /* RAW */

