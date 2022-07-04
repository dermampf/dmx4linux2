// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_PC16C550
#define DEFINED_PC16C550

struct rtuart;
struct rtuart * pc16c550_create(const unsigned long base_clock);

#endif
