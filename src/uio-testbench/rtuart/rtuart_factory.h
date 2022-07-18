#pragma once

struct rtuart;
struct rtuart * rtuart_create(const char * name, const char * bus_name);
void rtuart_cleanup(struct rtuart * uart);
