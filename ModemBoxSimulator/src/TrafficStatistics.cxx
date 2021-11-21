
#include "TrafficStatistics.h"

#include <utils/util.h>
#include <fxutils/fxutil.h>

using namespace utils;
using namespace fxutils;

enum {
    NCOLS = 10,
};

/* ============================================================= */
TrafficBlock::TrafficBlock(FXComposite* parent)
{
    tfPings = new FXTextField(parent, NCOLS);
    tfDataPackets = new FXTextField(parent, NCOLS);
    tfTotalBytes = new FXTextField(parent, NCOLS);
    tfTime = new FXTextField(parent, NCOLS);
}

/* ------------------------------------------------------------- */
static void
set_textfield_to_time(
    FXTextField*    tf
                      )
{
    my_time mt(my_time_of_now());
    fxprintf(tf, "%02d:%02d:%02d.%01d", mt.hour, mt.minute, mt.second, (mt.millisecond+50)/100);
}

/* ------------------------------------------------------------- */
void
TrafficBlock::registerPacket(
    const utils::CMR&   cmr)
{
    if (cmr.type == CMR::PING) {
        registerPing();
    } else {
        ++data_packets_;
        total_bytes_ += static_cast<unsigned int>(cmr.data.size()) + 6; // 6 is CMR overhead for a packet.
        fxprintf(tfDataPackets, "%d", data_packets_);
        fxprintf(tfTotalBytes, "%d", total_bytes_);
        set_textfield_to_time(tfTime);
    }
}

/* ------------------------------------------------------------- */
void
TrafficBlock::registerPing()
{
    ++pings_;
    ++total_bytes_;
    fxprintf(tfPings, "%d", pings_);
    fxprintf(tfTotalBytes, "%d", total_bytes_);
    set_textfield_to_time(tfTime);
}

/* ------------------------------------------------------------- */
void
TrafficBlock::clear()
{
    pings_ = 0;
    data_packets_ = 0;
    total_bytes_ = 0;

    tfPings->setText("0");
    tfDataPackets->setText("0");
    tfTotalBytes->setText("0");
    tfTime->setText("<never>");
}

/* ============================================================= */
TrafficStatistics::TrafficStatistics(
    FXComposite*    parent,
    const char*     name)
{
    new FXLabel(parent, name);
    tfStatus = new FXTextField(parent, NCOLS);
    tbInput = std::auto_ptr<TrafficBlock>(new TrafficBlock(parent));
    tbOutput = std::auto_ptr<TrafficBlock>(new TrafficBlock(parent));

    clear();
}

/* ------------------------------------------------------------- */
void
TrafficStatistics::clear()
{
    tfStatus->setText("Closed.");
    tbInput->clear();
    tbOutput->clear();
}

