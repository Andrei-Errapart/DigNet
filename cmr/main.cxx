/**
vim:	ts=4
vim:	shiftwidth=4
*/

#include <vector>

#include <memory>           // std::auto_ptr

#include <ctype.h>          // isalpha, etc.
#include <stdio.h>          // printf
#include <string.h>         // strcasecmp
#include <utils/Serial.h>
#include <utils/hxio.h>
#include <utils/CMR.h>
#include "cmr.h"

#if defined(_MSC_VER) && !defined(strcasecmp)
#define strcasecmp  stricmp
#endif

/****************************************************************************/
static int
print_usage()
{
    printf("Usage:\n");
    printf("\tcmr decode com-port,baudrate\n");
    printf("\tcmr decode filename\n");
    printf("\tcmr encode filename type com-port,baudrate\n");
    printf("where:\n");
    printf("\tcom-port,baudrate\tSerial port name and baud rate.\n");
    printf("\ttype\t\t\thexadecimal type.\n");
    printf("\tfilename\t\tName of the input file.\n");
    return 0;
}

/****************************************************************************/
static void
print_cmr_data_block(
    const unsigned int      type,
    const unsigned char*    data_block,
    const unsigned char     length)
{
    printf("CMR 0x%04x [0x%02x] = { ", type, length);
    for (unsigned int i=0; i<length; ++i) {
        printf("%02x", data_block[i]);
        printf(" ");
    }
    printf("} = \"");
    for (unsigned int i=0; i<length; ++i) {
        const unsigned char c = data_block[i];
        printf("%c", isprint(c) ? c : '.');
    }
    printf("\"\n");
}

/****************************************************************************/
static void
decode( const char* port_spec,
        const char* port_spec2)
{
    std::auto_ptr<utils::SerialPort>    sp2;
    bool    is_port = strlen(port_spec)>=4
                        && toupper(port_spec[0])=='C'
                        && toupper(port_spec[1])=='O'
                        && toupper(port_spec[2])=='M'
                        && isdigit(port_spec[3]);
    // 1. Open serial port/file.
    utils::SerialPort   sp;
    utils::hxio::IO     f;
    if (is_port) {
        printf("Opening port %s.\n", port_spec);
        sp.open(port_spec, 9600);
    } else {
        printf("Opening file %s.\n", port_spec);
        f.open(port_spec, "rb");
    }
    if (port_spec2 != 0) {
        std::string spec2(port_spec2);
        printf("Opening port %s.\n", port_spec2);
        sp2 = std::auto_ptr<utils::SerialPort>(new utils::SerialPort());
        sp2->open(spec2, 9600);
    }

    // 2. Setup rx buffer.
    CMR_RXPACKET    rxpacket;
    uint8_t         rxbuffer[256];
    cmr_rxpacket_init(&rxpacket, rxbuffer);

    printf("Listening/reading.\n");
    // 3. Feed the baby forever.
    for (;;) {
        char    sbuf[24];
        const int   n = is_port ? sp.read(sbuf, sizeof(sbuf)) : f.read(sbuf, sizeof(sbuf));
        if (n>0) {
            for (int i=0; i<n; ++i) {
                const unsigned char cc = static_cast<const unsigned char>(sbuf[i]);
                const CMRDECODE r = cmr_decode(&rxpacket, sbuf[i]);
                switch (r) {
                case CMRDECODE_OK:
                    printf(".");
                    fflush(stdout);
                    break;
                case CMRDECODE_ERROR:
                    if (cc!=0 && isprint(cc)) {
                        printf("%c", cc);
                    } else {
                        printf(" %02x", cc);
                    }
                    fflush(stdout);
                    break;
                case CMRDECODE_COMPLETE:
                    printf("Decoded.\n");
                    print_cmr_data_block(rxpacket.Type, rxpacket.DataBlock, rxpacket.Length);
                    if (sp2.get() != NULL) {
                        // KALLE SPECIAL
                        utils::timer::sleep(500 * 1000);
                        sp2->write((const char*)rxpacket.CmrBuffer, rxpacket.CmrLength);
                    }
                }
            }
        } else if (!is_port) {
            // eof.
            return;
        }
    }
}

/****************************************************************************/
static void
encode( const char*         filename,
        const unsigned int  type,
        const char*         port_spec)
{
    // 1. Read the file (not too big).
    std::vector<unsigned char>  datablock;
    utils::hxio::read_file(datablock, filename);
    if (datablock.size() > CMR_MAX_DATABLOCK_LENGTH)
    {
        printf("encode: file size %d bytes is bigger than maximum allowed %d\n",
                datablock.size(), CMR_MAX_DATABLOCK_LENGTH);
        return;
    }

    // 2. Encode data using cmr_encode.
    unsigned char   cmr_packet[256] = { 0 };
    uint8_t         cmr_length = 0;
#if (0)
    if (cmr_encode(&datablock[0], datablock.size(), type, cmr_packet, &cmr_length) == 0) {
#else
    CMR_TXPACKET    packet;
    cmr_txpacket_init(&packet, type, &datablock[0], datablock.size());
    for (;cmr_length <= CMR_MAX_PACKET_LENGTH;) {
        int16_t c = cmr_encode(&packet);
        if (c>=0) {
            cmr_packet[cmr_length] = c;
            ++cmr_length;
        } else {
            break;
        }
    }
    if (true) {
#endif
        // 3. Print contents.
        printf("CMR encoding success, type 0x%04x, data length: 0x%02x bytes, CMR length: 0x%2x bytes\n",
                type, datablock.size(), cmr_length);
        printf("CMR Packet follows:\n");
        for (unsigned int i=0; i<cmr_length; ++i) {
            printf("%02x", cmr_packet[i]);
            if (i+1 < cmr_length) {
                printf(" ");
            }
        }
        printf("\n");

        // 4. check the cmr...
        if (false) {
            utils::CMRDecoder cmr_assembler;
            utils::CMR          packet;
            std::vector<unsigned char>  stream(1);
            for (unsigned int i=0; i<cmr_length; ++i) {
                stream[0] = cmr_packet[i];
                cmr_assembler.feed(stream);
            }
            if (cmr_assembler.pop(packet)) {
                printf("Check: got a packet, type 0x%04x, data length: %0x02x.\n",
                    packet.type, packet.data.size());
                bool    contents_ok = true;
                for (unsigned int i=0; i<packet.data.size(); ++i) {
                    if (packet.data[i] != datablock[i]) {
                        contents_ok = false;
                        break;
                    }
                }
                printf("Check: %s\n", contents_ok ? "Content OK" : "Mismatch!");
            } else {
                printf("Check: failed.\n");
            }
        }

        // 5. Write it to the serial port.
        printf("Writing data to serial port %s\n", port_spec);
        utils::SerialPort  sp;
        sp.open(port_spec, 9600);
        sp.write(reinterpret_cast<const char*>(cmr_packet), cmr_length);
        sp.close();
    } else {
        printf("CMR encoding failed. No data output.\n");
    }
}

/****************************************************************************/
int
main(	int	argc,
	char**	argv)
{
    TRACE_SETFILE("disabled");
//    srand(time(0));

    try {
        if ((argc==3 || argc==4) && strcasecmp(argv[1], "decode")==0) {
            decode(argv[2], argc==4 ? argv[3] : NULL);
        } else if (argc==5 && strcasecmp(argv[1], "encode")==0) {
            unsigned int    type = 0x00;
            const int       r = sscanf(argv[3], "%x", &type);
            if (r==1) {
                encode(argv[2], type, argv[4]);
            } else {
                printf("Error: given type (%s) is not a hexadecimal number.\n", argv[3]);
                print_usage();
            }
        } else {
            return print_usage();
        } 
    } catch (const std::exception& e) {
        printf("Exception: %s\n", e.what());
        return 1;
    }
    printf("Finished.\n");
	return 0;
}

