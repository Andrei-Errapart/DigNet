/**
vim:	ts=4
vim:	shiftwidth=4
*/

#ifndef cmr_h_
#define	cmr_h_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
	typedef __int8	int8_t;
	typedef __int16	int16_t;
	typedef __int32	int32_t;
	typedef __int64 int64_t;
	typedef unsigned __int8		uint8_t;
	typedef unsigned __int16	uint16_t;
	typedef unsigned __int32	uint32_t;
	typedef unsigned __int64	uint64_t;
#elif defined(__GNUC__)
    // GCC :)
#include <stdint.h> // uint8_t, uint16_t, etc.
#else
# error oops
#endif

/*--------------------------------------------------------------------------*/
/** \file CMR format, see file "CMR_Modified.doc". */
enum {
    /// CMR start packet.
    CMR_STX     = 0x02,
    /// CMR end packet.
    CMR_ETX     = 0x03,
    /// CMR null packet, i.e. PING.
    CMR_NULL    = 0x00,
    /// Maximum datablock length.
    CMR_MAX_DATABLOCK_LENGTH    = 0xFF-6,
    /// Maximum packet length.
    CMR_MAX_PACKET_LENGTH       = 0xFF,
    /// CMR ping. This is encoded specially.
    CMR_TYPE_PING               = 0x0000,
    /// RTCM stream.
    CMR_TYPE_RTCM               = 0xBC05,
    /// Serial stream.
    CMR_TYPE_SERIAL             = 0xBC06,
};

/*--------------------------------------------------------------------------*/
/// Return value of \c cmr_decode
typedef enum {
    /// Byte was accepted.
    CMRDECODE_OK,
    /// An error occured, byte was not accepted.
    CMRDECODE_ERROR,
    /// Packet completed. See CMR_RXPACKET::DataBlock for contents.
    CMRDECODE_COMPLETE,
} CMRDECODE;

/*--------------------------------------------------------------------------*/
/// Receive data structure.
typedef struct {
    /// Raw CMR receive buffer.
    uint8_t*    CmrBuffer;
    /// Raw CMR packet length (number of cmr bytes successfully decoded so far).
    uint8_t     CmrLength;

    /// Decoded: Message type.
    uint16_t    Type;
    /// Decoded: Pointer to the data block inside \c RawRxBuffer.
    uint8_t*    DataBlock;
    /// Decoded: Length of the \c DataBlock.
    uint8_t     Length;

    /// Does it contain complete packet? Non-zero iff complete.
    uint8_t     IsComplete_;
    /// Internal use: Accumulated checksum.
    uint8_t     Checksum_;
} CMR_RXPACKET;

/*--------------------------------------------------------------------------*/
/// Transmit data structure.
typedef struct {
    /// Source message type.
    uint16_t        Type;
    /// Source data block.
    const uint8_t*  DataBlock;
    /// Length of the \c DataBlock.
    uint8_t         Length;

    /// Internal use: Accumulated checksum.
    uint8_t         Checksum_;
    /// Internal use: Encoded byte counter.
    uint8_t         EncodedCount_;
    /// Internal use: Total number of bytes to be encoded.
    uint8_t         TotalCount_;
} CMR_TXPACKET;

/*--------------------------------------------------------------------------*/
/** Initialize receive packet.
\param  Packet      Receive packet.
\param  CmrBuffer   Data buffer. This must have room for at least 255 bytes.
*/
void
cmr_rxpacket_init(
    CMR_RXPACKET* Packet,
    uint8_t*      CmrBuffer
    );

/*--------------------------------------------------------------------------*/
/** Decode the CMR data stream, one byte at a time. Upon successful
packet completion the receive buffer contents are valid up to next call.

Single bytes 0x00 are recognized as packets of type 0x00 and data block length 0.

Computational complexity: O(1).
\param      rxbuffer    Receive buffer, initialized by \c cmr_init.
\param[in]  c           Byte from the data stream...
\return CMRDECODE_COMPLETE iff packet complete, otherwise contents is not valid.
        Fields of interest: RawRxBuffer and Raw
*/
CMRDECODE
cmr_decode( CMR_RXPACKET*   rxbuffer,
            const uint8_t   c);

/*--------------------------------------------------------------------------*/
/** Initialize transmit packet.
\param      Packet      Transmit packet.
\param[in]  Type        Type of the data block. Packets of type 0x0000 are encoded as single byte, 0x00.
\param[in]  DataBlock   Data block to be transmitted.
\param[in]  Length      Length of the \c DataBlock. Valid range is 0...CMR_MAX_DATABLOCK_LENGTH, including.
*/
void
cmr_txpacket_init(
    CMR_TXPACKET*   Packet,
    const uint16_t  Type,
    const uint8_t*  DataBlock,
    const uint8_t   Length
    );

/*--------------------------------------------------------------------------*/
/** Encode RTCM data stream by the given packet, one byte at a time.
End of stream is signalled by return value -1.

Computational complexity: O(1).
\param  Packet  to be encoded.
\return Next byte in the 
*/
int16_t
cmr_encode(
    CMR_TXPACKET*   Packet
    );

/** Wrap datablock into CMR packet.

Computational complexity: O(N), where N=length.
\param[in]  data    Data to be encoded.
\param[in]  length  Length of data. Maximum value is 249 (CMR_MAX_DATABLOCK_LENGTH).
\param[in]  type    Type of data.
\param[in]  buffer          Buffer to receive CMR packet. This must have room for at least 255 (CMR_MAX_PACKET_LENGTH) bytes.
\param[out] cmr_length      Length of total CMR packet.
\return 0 iff successful, -1 otherwise.
*/
int
cmr_encode_once(
    uint8_t*        datablock,
    const uint8_t   length,
    const uint16_t  type,
    uint8_t*        buffer,
    uint8_t*        cmr_length
            );

#ifdef __cplusplus
}
#endif

#endif /* cmr_h_ */

