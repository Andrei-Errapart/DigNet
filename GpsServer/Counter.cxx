#include "Counter.h"

#include <map>
#include <stdint.h>

using namespace std;

/*****************************************************************************/
Counter::Counter()
{
    total = 0;
}


/*****************************************************************************/
uint64_t
Counter::countAtInterval(
    int num)
{
    if (intervals.find(num) == intervals.end()) {
        intervals[num] = 0;
    }
    const uint64_t count = total-intervals[num];
    intervals[num] = total;
    return count;
}


/*****************************************************************************/
uint64_t
Counter::getTotal() const
{
    return total;
}

/*****************************************************************************/
void 
Counter::operator++()
{
    total++;
}

/*****************************************************************************/
void 
Counter::operator+=(
        const int amount)
{
    total+=amount;
}
