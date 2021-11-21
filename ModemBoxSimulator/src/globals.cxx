#include <windows.h>
#include "globals.h"

namespace System {
    long currentTimeMillis()
    {
        return ::GetTickCount();
    }
} // namespace System
