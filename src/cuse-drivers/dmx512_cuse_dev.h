/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
#ifndef DEFINED_DMX512_CUSE_DEV
#define DEFINED_DMX512_CUSE_DEV

struct dmx512_cuse_card;
struct dmx512frame;

struct dmx512_cuse_fdwatcher
{
  int   fd;
  void (*callback) (int, void *);
  void *user;
};

struct dmx512_cuse_card_ops
{
    int (*queryCardInfo) (struct dmx512_cuse_card *dmx512,
                          struct dmx4linux2_card_info *cardinfo);
    int (*changeCardInfo) (struct dmx512_cuse_card *dmx512,
                           struct dmx4linux2_card_info *cardinfo);
    int (*queryPortInfo) (struct dmx512_cuse_card *dmx512,
                          struct dmx4linux2_port_info *portinfo,
                          int portIndex);
    int (*changePortInfo) (struct dmx512_cuse_card *dmx512,
                           struct dmx4linux2_port_info *portinfo,
                           int portIndex);
    int (*init)          (struct dmx512_cuse_card * card);
    int (*cleanup)       (struct dmx512_cuse_card * card);
    int (*sendFrame)     (struct dmx512_cuse_card *dmx512,
                          struct dmx512frame *frame);
};

struct dmx512_cuse_card_config
{
    int    cardno;
    void * userpointer;
    struct dmx512_cuse_card_ops *ops;
};

void dmx512_cuse_send_frame(struct dmx512_cuse_card *card,
                            struct dmx512frame *frame);

void dmx512_cuse_handle_received_frame(struct dmx512_cuse_card *card,
                                       struct dmx512frame *frame);

int dmx512_cuse_lowlevel_main(int cuseargc, char ** cuseargv,
                              struct dmx512_cuse_card_config *cards,
                              int num_cards,
                              struct dmx512_cuse_fdwatcher * fdwatchers,
                              int num_fd_watchers);


struct dmx512_cuse_card_config * dmx512_cuse_card_config(struct dmx512_cuse_card *dmx512);


int dmx512_cuse_fdwatcher_add(struct dmx512_cuse_fdwatcher * w);
int dmx512_cuse_fdwatcher_remove(struct dmx512_cuse_fdwatcher * w);

int  dmx512_core_init(void);
void dmx512_core_exit(void);

#endif
