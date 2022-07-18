#ifndef DEFINED_DMX512_RDM_PRIV
#define DEFINED_DMX512_RDM_PRIV
/*
Slot # Description             Data Value                   Remarks
0      START Code              SC_RDM
1      Sub-START Code          SC_SUB_MESSAGE
2      Message Length          25 (Slot # of Checksum High) Range 24 to 255.
3-8    Destination UID         0x123456789ABC
9 - 14 Source UID              0xCBA987654321
15     Transaction Number (TN) 0
16     Port ID / Response Type 0
17     Message Count           0
18 -   Sub-Device              0
19
20     Command Class (CC)      GET_COMMAND
21 -   Parameter ID (PID)      STATUS_MESSAGES
22
23     Parameter Data Length   1                            Range 0 to 231
       (PDL)
24     Parameter Data (PD)     STATUS_ERROR
25     Checksum High
26     Checksum Low

  Total Packet Time (n = number of data slots [1-513]) : 440uS +(n x 44uS ) + ((n -1) x 76uS )
  Inter Slot Time (Max): 2.0ms(TX)  2.1ms(RX)

*/
enum // RDM
{
	SC_RDM = 0xCC,
	SC_SUB_MESSAGE = 0x01,
	
	/* Table A-1: Command Class Defines
	 * RDM Command Classes (Slot 20) */
	DISCOVERY_COMMAND = 0x10,
	DISCOVERY_COMMAND_RESPONSE = 0x11,
	GET_COMMAND = 0x20,
	GET_COMMAND_RESPONSE = 0x21,
	SET_COMMAND = 0x30,
	SET_COMMAND_RESPONSE = 0x31,

	/* Table A-2: Response Type Defines
	 * RDM Response Type (Slot 16) */
	RESPONSE_TYPE_ACK = 0x00,
	RESPONSE_TYPE_ACK_TIMER = 0x01,
	RESPONSE_TYPE_NACK_REASON = 0x02,  /* See Table A-17 */
	RESPONSE_TYPE_ACK_OVERFLOW = 0x03, /* Additional Response Data available beyond single response length. */
	RESPONSE_TYPE_NOREPLY = 0xff, // This is used internally to indicate, that no response shall be send

	/* RDM Parameter ID's (Slot 21-22)
	 *-- Category - Network Management */
	DISC_UNIQUE_BRANCH = 0x0001,
	DISC_MUTE = 0x0002,
	DISC_UN_MUTE = 0x0003,
	PROXIED_DEVICES = 0x0010,
	PROXIED_DEVICE_COUNT = 0x0011,
	COMMS_STATUS = 0x0015,
	
	/*-- Category - Status Collection */
	QUEUED_MESSAGE = 0x0020, /* See Table A-4 */
	STATUS_MESSAGES = 0x0030, /* See Table A-4 */
	STATUS_ID_DESCRIPTION = 0x0031,
	CLEAR_STATUS_ID = 0x0032,
	SUB_DEVICE_STATUS_REPORT_THRESHOLD = 0x0033, /* See Table A-4 */
	
	DMX_POS_STARTCODE = 0,
	RDM_POS_SUBSTARTCODE = 1,
	RDM_POS_LENGTH = 2,
	RDM_POS_DEST_UID = 3,
	RDM_POS_SRC_UID = 9,

	RDM_POS_PORT_ID = 16,
	RDM_POS_RESPONSE_TYPE = 16,
	
	RDM_POS_COMMAND_CLASS = 20,
	RDM_POS_PID = 21, /* 21-22 */
	RDM_POS_PDL = 23,
	RDM_DISCOVER_REPLY_MAX_FRAMESIZE = 24
};
#endif
