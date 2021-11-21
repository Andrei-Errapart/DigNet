
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include "sysdefs.h"
#include "cmrpcktctrl.h"



/*****************************************************/
void CMR_Init(CMR_RXPACKET *Packet, uint8_t *CmrBuffer)
/*****************************************************/
{
	Packet->Phase = 0;
	Packet->CmrBuffer = CmrBuffer;
	Packet->DataBlock = &CmrBuffer[4];
}

/*****************************************************/
uint8_t CMR_DecodePacket(CMR_RXPACKET *Packet, uint8_t data)
/*****************************************************/
{
	
	switch(Packet->Phase)
	{	
		case 0:
			switch(data)
			{
				case CMR_NULL:
					Packet->Type = CMR_TYPE_PING;
					Packet->DataBlock[0] = CMR_NULL;
					Packet->Length = 1;
					Packet->CmrBuffer[0] = CMR_NULL;
					Packet->CmrLength = 1;
					return CMRDECODE_COMPLETE;

				case CMR_STX:
					Packet->CmrLength = 0;
					Packet->BytesLeft = 3;
					Packet->CheckSum = 0;
					Packet->CmrBuffer[Packet->CmrLength++] = data;
					Packet->Phase++;			
					break;

				default:
					Packet->Phase = 0;	
					Packet->CmrLength = 0;		
					return CMRDECODE_ERROR;
			}
			break;

		case 1:
			Packet->CmrBuffer[Packet->CmrLength++] = data;
			Packet->CheckSum += data;
			if(!(--(Packet->BytesLeft)))
			{
				Packet->BytesLeft = Packet->CmrBuffer[3];
				Packet->Phase++;
			}
			break;

		case 2:
			Packet->CmrBuffer[Packet->CmrLength++] = data;
			Packet->CheckSum += data;
			if(!(--(Packet->BytesLeft)))
			{
				Packet->Phase++;
			}
			break;

		case 3:
			Packet->CmrBuffer[Packet->CmrLength++] = data;
			if(Packet->CheckSum == data) { Packet->Phase++; break; }
			Packet->Phase = 0;
			return CMRDECODE_ERROR;

		case 4:
			Packet->CmrBuffer[Packet->CmrLength++] = data;
			if(data == CMR_ETX)
			{
				uint8_t *cmr_buffer = Packet->CmrBuffer;

				Packet->Type = ((uint16_t)cmr_buffer[1] << 8) | cmr_buffer[2];;
				Packet->Length = cmr_buffer[3]; 
				Packet->Phase = 0;
				return CMRDECODE_COMPLETE;
			}
			else
			{
				Packet->Phase = 0;
				return CMRDECODE_ERROR;
			}

		default:
			Packet->Phase = 0;
			break;
		
	}

	if(Packet->CmrLength >= CMR_MAX_PACKET_LENGTH)
	{
		Packet->Phase = 0;
		return CMRDECODE_ERROR;
	}

	return CMRDECODE_OK;
}


/*****************************************************/
uint8_t CMR_EncodePacket(uint8_t *datablock, uint8_t length,
	uint16_t type, uint8_t *buffer, uint8_t *cmr_length)
/*****************************************************/
{
	if(length >= CMR_MAX_DATABLOCK_LENGTH) return FALSE;

	if(type == CMR_TYPE_PING)
	{
		buffer[0] = CMR_NULL;
		*cmr_length = 1;
	}
	else
	{
		uint8_t chksum;
		uint8_t *p = buffer;
		int i;

		*p++ = CMR_STX;
		*p++ = type >> 8;
		*p++ = type & 0xff;
		*p++ = length;

		chksum = buffer[1] + buffer[2] + buffer[3];
		for(i = 0; i < length; i++)
		{
			*p++ = datablock[i];
			chksum += datablock[i];
		}

		*p++ = chksum;
		*p++ = CMR_ETX;

		*cmr_length = p - buffer;
	}

	return TRUE;
}
