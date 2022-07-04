// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Michael Stickel <michael@cubic.org>
 */
  struct Uid
  {
    union
    {
      struct
      {
	uint16_t manufacturer;
	uint32_t device;
      };
      uint8_t data[6];
    };
  } STRUCT_PACKED;

Rdm::Packet rdm;
rdm.header.startCode = SC_RDM;
rdm.header.subStartCode = SC_SUB_MESSAGE;
rdm.header.msgLength = x;
rdm.header.dstUid. = x;
rdm.header.srcUid = x;
rdm.header.TN = 0; // Transaction number
rdm.header.portId = 0;
rdm.header.msgCount = 0;
rdm.header.subDevice = 0;
rdm.header.CC = GET_COMMAND;
rdm.header.PID = ;
rdm.header.PDL = 12;
rdm.data[0] = xxx;
