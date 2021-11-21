/**
vim:	ts=4
vim:	shiftwidth=4
*/

#include <assert.h> // programmer's best friend.
#include "cmr.h"

/****************************************************************************/
void
cmr_rxpacket_init(
    CMR_RXPACKET* Packet,
    uint8_t*      CmrBuffer
    )
{
    Packet->CmrBuffer = CmrBuffer;
    Packet->CmrLength = 0;
    Packet->DataBlock = &CmrBuffer[4];
    Packet->IsComplete_ = 0; // false
    Packet->Checksum_ = 0;
}

/****************************************************************************/
CMRDECODE
cmr_decode( CMR_RXPACKET*   Packet,
            const uint8_t   c)
{
    // First byte?
    if (Packet->CmrLength==0 || Packet->IsComplete_) {
        // disqualify buffer.
        Packet->IsComplete_ = 0; // false
        switch (c) {
        // PING?
        case CMR_NULL:
            Packet->Type = CMR_TYPE_PING;
            Packet->DataBlock[0] = CMR_NULL;
            Packet->Length = 1;
            Packet->CmrBuffer[0] = CMR_NULL;
            Packet->CmrLength = 1;
            Packet->IsComplete_ = 1; // true
            return CMRDECODE_COMPLETE;
        // Start?
        case CMR_STX:
            Packet->CmrBuffer[0] = c;
            Packet->CmrLength = 1;
            return CMRDECODE_OK;
        // Skip it.
        default:
            return CMRDECODE_ERROR;
        }
    } else {
        const uint8_t       length = Packet->CmrBuffer[3];
        const uint8_t       cmr_length = Packet->CmrLength + 1;

        // Insert the character.
        Packet->CmrBuffer[Packet->CmrLength] = c;
        Packet->CmrLength = cmr_length;

        if (cmr_length<5) {
            // Header.
            Packet->Checksum_ += c;
        } else if (length+7==cmr_length) {
            // Just received the checksum... really?
            if (Packet->Checksum_ != c) {
                cmr_rxpacket_init(Packet, Packet->CmrBuffer);
                return CMRDECODE_ERROR;
            }
        } else if (length+6==cmr_length) {
            // Final byte... naturally?
            if (c == CMR_ETX) {
                // YES :P
                const uint8_t*      cmr_buffer = Packet->CmrBuffer;
                Packet->Type = (((uint16_t)cmr_buffer[1]) << 8) | cmr_buffer[2];
                Packet->Length = length;
                Packet->IsComplete_ = 0x01; // true;
                return CMRDECODE_COMPLETE;
            } else {
                cmr_rxpacket_init(Packet, Packet->CmrBuffer);
                return CMRDECODE_ERROR;
            }
        } else {
            // Payload.
            Packet->Checksum_ += c;
        }

        // Overflow on next cmr_decode?
        if (Packet->CmrLength == CMR_MAX_PACKET_LENGTH) {
            cmr_rxpacket_init(Packet, Packet->CmrBuffer);
            return CMRDECODE_ERROR;
        }

        return CMRDECODE_OK;
    }
}

/****************************************************************************/
void
cmr_txpacket_init(
    CMR_TXPACKET*   Packet,
    const uint16_t  Type,
    const uint8_t*  DataBlock,
    const uint8_t   Length
    )
{
    Packet->Type = Type;
    Packet->DataBlock = DataBlock;
    Packet->Length = Length;
    Packet->Checksum_ = 0x00;
    Packet->EncodedCount_ = 0;
    Packet->TotalCount_ =
        Type==CMR_TYPE_PING
            ? 1
            : (Length+6);
}

/****************************************************************************/
int16_t
cmr_encode(
    CMR_TXPACKET*   Packet
    )
{
    // How far we are?
    if (Packet->EncodedCount_ < Packet->TotalCount_) {
        const uint8_t   so_far = Packet->EncodedCount_;
        Packet->EncodedCount_ = so_far + 1;

        if (so_far == 0) {
            // First byte, or zero.
            return
                Packet->Type == 0
                    ? CMR_NULL
                    : CMR_STX;
        } else if (so_far == 1) {
            // Type High
            const uint8_t   r = Packet->Type >> 8;
            Packet->Checksum_ += r;
            return r;
        } else if (so_far == 2) {
            // Type Low
            const uint8_t   r = Packet->Type & 0xFF;
            Packet->Checksum_ += r;
            return r;
        } else if (so_far == 3) {
            // Length
            Packet->Checksum_ += Packet->Length;
            return Packet->Length;
        } else if (so_far < Packet->Length + 4) {
            const uint8_t   r = Packet->DataBlock[so_far-4];
            // Payload
            Packet->Checksum_ += r;
            return r;
        } else if (so_far == Packet->Length + 4) {
            // Checksum.
            return Packet->Checksum_;
        } else if (so_far == Packet->Length + 5) {
            // End of story.
            return CMR_ETX;
        }

        // must be error :)
        return -1;
    } else {
        return -1;
    }
}

/****************************************************************************/
int
cmr_encode_once(
            uint8_t*        datablock,
            const uint8_t   length,
            const uint16_t  type,
            uint8_t*        buffer,
            uint8_t*        cmr_length)
{
    if (length <= CMR_MAX_DATABLOCK_LENGTH) {
	    if (type == CMR_TYPE_PING) {
            buffer[0] = CMR_NULL;
            *cmr_length = 1;
	    } else {
            uint8_t checksum;
            unsigned int    i;

            buffer[0] = CMR_STX;
            buffer[1] = type >> 8;
            buffer[2] = type & 0xFF;
            buffer[3] = length;

		    checksum = buffer[1] + buffer[2] + buffer[3];
		    for (i=0; i<length; ++i) {
			    buffer[i + 4] = datablock[i];
			    checksum = checksum + datablock[i];
		    }
		    buffer[length + 4] = checksum;
		    buffer[length + 5] = CMR_ETX; // trailer

            *cmr_length = length + 6;
	    }
        return 0;
    } else {
        return -1;
    }
    return 0;
}
