#pragma once
struct rtuart;
struct rtuart_bus;
struct rtuart * rtuart_pl011_create(struct rtuart_bus * bus,
				    const unsigned long base_clock_hz);
