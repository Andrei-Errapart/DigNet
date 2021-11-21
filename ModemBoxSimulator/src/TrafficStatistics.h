#ifndef TrafficStatistics_h_
#define TrafficStatistics_h_

#include <fx.h>
#include <utils/CMR.h>

/** \file Traffic statistics.

Use class TrafficStatistics

Column ordering:
1. Name
2. Input pings
3. Input data packets.
4. Input total bytes.
5. Time of last input.
6. Output pings
7. Output data packets.
8. Output total bytes.
9. Time of last output.
*/

/* ============================================================= */
class TrafficBlock {
private:
    unsigned int    pings_;
    unsigned int    data_packets_;
    unsigned int    total_bytes_;
public:
    /// New column of traffic statistics.
    TrafficBlock(
        FXComposite*    parent
        );

    FXTextField*    tfPings;
    FXTextField*    tfDataPackets;
    FXTextField*    tfTotalBytes;
    FXTextField*    tfTime;

    /// Register 1 packet.
    void
    registerPacket(
        const utils::CMR&   cmr);
    /// Register 1 ping.
    void
    registerPing();

    /// Clear all fields.
    void
    clear();
}; // class TrafficBlock

/* ============================================================= */
class TrafficStatistics {
public:
    /// New column of traffic statistics.
    TrafficStatistics(
        FXComposite*    parent,
        const char*     name);

    /// Clear all fields, set status to zero.
    void
    clear();

    FXTextField*    tfStatus;

    std::auto_ptr<TrafficBlock> tbInput;
    std::auto_ptr<TrafficBlock> tbOutput;
}; // class TrafficStatistics

#endif /* TrafficStatistics_h_ */