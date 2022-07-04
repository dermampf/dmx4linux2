// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 2)
	return 1;

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("open uio");
        exit(EXIT_FAILURE);
    }

    while (1) { /* some condition here */
        uint32_t info = 1; /* unmask */

        ssize_t nb = write(fd, &info, sizeof(info));
        if (nb != (ssize_t)sizeof(info)) {
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }

        /* Wait for interrupt */
        nb = read(fd, &info, sizeof(info));
        if (nb == (ssize_t)sizeof(info)) {
            /* Do something in response to the interrupt. */
            printf("Interrupt #%u!\n", info);
        }
    }

    close(fd);
    exit(EXIT_SUCCESS);
}
