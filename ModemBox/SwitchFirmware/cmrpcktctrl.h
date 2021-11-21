#ifndef _CMDPCKTCTRL_H
#define _CMDPCKTCTRL_H

enum {

	CMR_STX						= 0x02,
	CMR_ETX 					= 0x03,
	CMR_NULL 					= 0x00,
	CMR_MAX_DATABLOCK_LENGTH 	= 0xff - 6,
	CMR_MAX_PACKET_LENGTH		= 0xff,
	CMR_TYPE_PING				= 0x0000,
	CMR_TYPE_RTCM				= 0xbc05,
	CMR_TYPE_SERIAL				= 0xbc06,
	CMR_TYPE_DIGNET				= 0xbc07,
};

enum {

	CMRDECODE_OK,
	CMRDECODE_ERROR,
	CMRDECODE_COMPLETE

} CMRDECODE;

typedef struct {
	uint8_t *CmrBuffer;
	uint16_t CmrLength;
	uint16_t Type;
	uint8_t *DataBlock;
	uint8_t Length;
	uint8_t Phase;
	uint8_t BytesLeft;
	uint8_t CheckSum;
} CMR_RXPACKET;


void CMR_Init(CMR_RXPACKET *Packet, uint8_t *CmrBuffer);
uint8_t CMR_DecodePacket(CMR_RXPACKET *Packet, uint8_t data);
uint8_t CMR_EncodePacket(uint8_t *datablock, uint8_t length, uint16_t type, uint8_t *buffer, uint8_t *cmr_length);


#endif
