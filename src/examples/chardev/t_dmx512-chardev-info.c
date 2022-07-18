// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 Michael Stickel <michael@cubic.org>
 */
#include <linux/dmx512/dmx512frame.h>

#include <sys/ioctl.h>
#include <linux/dmx512/dmx512_ioctls.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>



unsigned long dmx4linux2_query_version(int dmxfd)
{
    unsigned long dmx4linux_version = 0;
    int ret = ioctl(dmxfd, DMX512_IOCTL_VERSION, &dmx4linux_version);
    if (ret < 0)
      {
        perror("DMX512_IOCTL_VERSION");
        return 0;
      }
    return dmx4linux_version;
}

void dmx4linux2_free_card_info(struct dmx4linux2_card_info * card_info)
{
    if (card_info)
    {
        if (card_info->tuples)
            free(card_info->tuples);
        free(card_info);
    }
}


struct dmx4linux2_card_info *dmx4linux2_query_card_info(int dmxfd)
{
    struct dmx4linux2_card_info * card_info = malloc(sizeof(*card_info));
    if (!card_info)
        return 0;

    bzero(card_info, sizeof(*card_info));
    int ret = ioctl(dmxfd, DMX512_IOCTL_QUERY_CARD_INFO, card_info);
    if (ret < 0)
    {
        dmx4linux2_free_card_info(card_info);
        return 0;
    }

    card_info->tuples = malloc(card_info->tuple_count*sizeof(struct dmx4linux2_key_value_tuple));
    bzero(card_info->tuples, card_info->tuple_count*sizeof(struct dmx4linux2_key_value_tuple));

    ret = ioctl(dmxfd, DMX512_IOCTL_QUERY_CARD_INFO, card_info);
    if (ret < 0)
      {
        perror("DMX512_IOCTL_QUERY_CARD_INFO @2");
        dmx4linux2_free_card_info(card_info);
        return 0;
      }
    return card_info;
}

int  dmx4linux2_free_port_info(struct dmx4linux2_port_info * portinfo)
{
  return -1;
}

struct dmx4linux2_port_info * dmx4linux2_query_port_info(int dmxfd, int portindex)
{
    struct dmx4linux2_port_info * portinfo = malloc(sizeof(*portinfo));
    if (portinfo)
    {
        bzero(portinfo, sizeof(*portinfo));
        portinfo->port_index = portindex;

        int ret = ioctl(dmxfd, DMX512_IOCTL_QUERY_PORT_INFO, portinfo);
        if (ret < 0)
        {
            dmx4linux2_free_port_info(portinfo);
            return 0;
        }

        portinfo->tuples = malloc(portinfo->tuple_count*sizeof(struct dmx4linux2_key_value_tuple));
        bzero(portinfo->tuples, portinfo->tuple_count*sizeof(struct dmx4linux2_key_value_tuple));

        ret = ioctl(dmxfd, DMX512_IOCTL_QUERY_PORT_INFO, portinfo);
        if (ret < 0)
        {
            perror("DMX512_IOCTL_QUERY_PORT_INFO @2");
            dmx4linux2_free_port_info(portinfo);
            return 0;
        }
    }
    return portinfo;
}

int dmx4linux2_change_port_info(int dmxfd,
                                struct dmx4linux2_port_info * portinfo)
{
  return ioctl(dmxfd, DMX512_IOCTL_CHANGE_PORT_INFO, portinfo);
}

int  dmx4linux2_change_one_port_tuple(int dmxfd,
                                      int cardindex,
                                      int portindex,
                                      int key,
                                      const char * value)
{
  struct dmx4linux2_port_info portinfo;
  struct dmx4linux2_key_value_tuple porttuple;
  bzero(&portinfo, sizeof(portinfo));
  bzero(&porttuple, sizeof(porttuple));
  portinfo.card_index = cardindex;
  portinfo.port_index = portindex;
  portinfo.tuple_count = 1;
  portinfo.tuples = &porttuple;
  porttuple.key = key;
  strncpy(porttuple.value, value, sizeof(porttuple.value));
  return dmx4linux2_change_port_info(dmxfd, &portinfo);
}


int dmx4linux2_change_card_info(int dmxfd,
                                struct dmx4linux2_card_info * cardinfo)
{
  return ioctl(dmxfd, DMX512_IOCTL_CHANGE_CARD_INFO, cardinfo);
}

int  dmx4linux2_change_one_card_tuple(int dmxfd,
                                      int cardindex,
                                      int key,
                                      const char * value)
{
  struct dmx4linux2_card_info cardinfo;
  struct dmx4linux2_key_value_tuple cardtuple;
  bzero(&cardinfo, sizeof(cardinfo));
  bzero(&cardtuple, sizeof(cardtuple));
  cardinfo.card_index = cardindex;
  cardinfo.tuple_count = 1;
  cardinfo.tuples = &cardtuple;
  cardtuple.key = key;
  strncpy(cardtuple.value, value, sizeof(cardtuple.value));
  return dmx4linux2_change_card_info(dmxfd, &cardinfo);
}




int main (int argc , char **argv)
{
    const char * card_name = (argc > 1) ? argv[1] : "/dev/dmx-card0";
    int dmxfd = open(card_name, O_RDWR);
    if (dmxfd < 0)
	return 1;

    const unsigned long dmx4linux_version = dmx4linux2_query_version(dmxfd);
    printf ("dmx4linux-version: %d.%d.%d\n",
            (unsigned char)(dmx4linux_version>>16),
            (unsigned char)(dmx4linux_version>>8),
            (unsigned char)dmx4linux_version
            );

    struct dmx4linux2_card_info * card_info = dmx4linux2_query_card_info(dmxfd);
    if (card_info)
    {
        printf ("-- CardInfo Card %d ---\n", card_info->card_index);
        printf ("card index:%d\n",  card_info->card_index);
        printf ("port count:%d\n",  card_info->port_count);
        printf ("tuple count:%d\n", card_info->tuple_count);
        int i;
        for (i = 0; i < card_info->tuple_count; ++i)
          printf ("  %d = %s<%d> %s\n",
                  card_info->tuples[i].key,
                  card_info->tuples[i].value,
                  card_info->tuples[i].maxlength,
                  card_info->tuples[i].changable ? "[mutable]" : ""
              );

        dmx4linux2_free_card_info(card_info);
      }


    struct dmx4linux2_port_info * portinfo = 0;
    int i;
    for (int i = 0; (portinfo = dmx4linux2_query_port_info(dmxfd, i)) != 0; ++i)
    {
        printf ("  -- [%d] PortInfo Port %d.%d ---\n",
                i,
                card_info->card_index,
                portinfo->port_index
            );
        printf ("    card index:%d\n",  portinfo->card_index);
        printf ("    port index:%d\n",  portinfo->port_index);
        printf ("    tuple count:%d\n", portinfo->tuple_count);

        {
            int i;
            for (i = 0; i < portinfo->tuple_count; ++i)
                printf ("      %d = %s<%d> %s\n",
                        portinfo->tuples[i].key,
                        portinfo->tuples[i].value,
                        portinfo->tuples[i].maxlength,
                        portinfo->tuples[i].changable ? "[mutable]" : ""
                    );
        }

        dmx4linux2_free_port_info(portinfo);
    }

    int ret = dmx4linux2_change_one_port_tuple(dmxfd,
                                               0,
                                               0,
                                               DMX4LINUX2_ID_PORT_LABEL,
                                               "My Test Port");
    if (ret < 0)
      perror("set port label");

    ret =dmx4linux2_change_one_card_tuple(dmxfd,
                                          0,
                                          DMX4LINUX2_ID_CUSTOMER_NAME,
                                          "My Customer Name");
    if (ret < 0)
      perror("set card customer name");

    close(dmxfd);
    return 0;
}
