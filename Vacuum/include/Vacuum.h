// Vacuum.h
#ifndef VACUUM_H
#define VACUUM_H

#include <cstdint>
#include "Enums.h"
#include "States.h"

using namespace States;

namespace Vacuum
{
    void init(struct States::AppState &a_s, SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m);
    constexpr uint32_t VACUUM_TASK_TIMEOUT  = 100;
    
    constexpr uint8_t PWM_CHANNEL_BLDC  = 0;
    constexpr uint16_t PWM_FREQ_BLDC    = 1000; 
    constexpr uint8_t PWM_RES_BLDC      = 8;

    constexpr uint8_t PWM_CHANNEL_SERVO = 2;
    constexpr uint16_t PWM_FREQ_SERVO   = 50;
    constexpr uint8_t PWM_RES_SERVO	    = 10;

    constexpr uint8_t PWM_BLDC_DUTY     = 255;
    constexpr uint32_t CTIME_DEFAULT    = 5000;
    constexpr uint32_t DTIME_DEFAULT    = 5000;
}

#endif