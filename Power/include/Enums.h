// Enums.h
#ifndef ENUMS_H
#define ENUMS_H

#include <cstdint>

namespace Enums
{
    enum AppErrors : uint8_t
    {
        NO_ERROR                = 0,
        HARDWARE_ERROR          = 1,
        STATE_TRANSITION_ERROR  = 2,
        UNKNOWN_COMMAND_ERROR   = 3,
        NETWORK_ERROR           = 4
    };
}

#endif