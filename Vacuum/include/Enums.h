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
        UNKNOWN_COMMAND_ERROR   = 3
    };

    enum VacuumState : uint8_t
    {
        IDLE        = 0,
        COLLECTING  = 1,
        DROPPING    = 2,
        CLEANING    = 3,
        STOP        = 4,
        ERROR       = 5
    };
    
    enum VacuumCommand : uint8_t
    {
        TO_IDLE             = 0,
        TO_COLLECTING       = 1,
        TO_DROPPING         = 2,
        TO_CLEANING         = 3,
        TO_STOP             = 4,
        CLEAR_ERRORS        = 5
    };

    constexpr uint8_t STATE_TRANSITION_MASK[6] = {
        0b111011,  // IDLE: can go to states 0,1,3,4,5
        0b110110,  // COLLECTING: can go to 1,2,4,5
        0b111101,  // DROPPING: can go to 0,2,3,4,5
        0b111001,  // CLEANING: can go to 0,3,4,5
        0b110001,  // STOP: can go to 0,4,5
        0b100001   // ERROR: can only go to 5
    };

    constexpr bool can_transition(VacuumState current, VacuumState target)
    {
        return (STATE_TRANSITION_MASK[current] >> target) & 1;
    }
    
    enum ServoLidState : uint8_t
    {
        CLOSED = 78,
        OPEN = 47
    };
}

#endif