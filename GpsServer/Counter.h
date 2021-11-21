#ifndef COUNTER_H
#define COUNTER_H

#include <map>
#include <stdint.h>
/**
\c Counter class Counts stuff like packets/bytes sent/received and holds information about time intervals
    (e.g how much stuff was counted during last 10s/60s/3600s)
    Intervals are identified by integer keys. Easiest would be to simply use corresponding timer values (10, 60 etc)

 @author Kalle Last <kalle@errapartengineering.com>
*/
class Counter
{
    uint64_t total;                             ///< Holds total count
    std::map<int, uint64_t> intervals;          ///< Holds count for each interval, gets set when reading it
public:
    Counter();

    /** Returns specified interval counter change since last read and resets the counter */
    uint64_t countAtInterval(int num);

    /** Returns total amount counted so far*/
    uint64_t getTotal() const;

    /** Adds one unit to \c total */
    void operator++();

    /** Adds \c amount units to \c total */
    void operator+=(const int amount);
};


#endif
