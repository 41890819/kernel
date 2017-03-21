/*
 * LCD driver data for BYD_BM8766U
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ECX336AF_H
#define _ECX336AF_H

/**
 * @gpio_reset  : spi chip reset
 * @gpio_lcd_cs : spi chip select
 * @gpio_lcd_clk: spi clk
 * @gpio_lcd_sdi: spi in  [cpu -----> lcd]
 * @gpio_lcd_sdo: spi out [lcd -----> cpu]
 */
struct platform_ecx336af_data {
	unsigned int gpio_reset;
	unsigned int gpio_spi_cs;
	unsigned int gpio_spi_clk;
	unsigned int gpio_spi_sdo;
	unsigned int gpio_spi_sdi;
	struct regulator *power_1v8;
};

#endif /* _ECX336AF_H */

