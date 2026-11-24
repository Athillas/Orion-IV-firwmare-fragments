// Feedback.h
#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <PubSubClient.h>

#include "Configs/NetworkConfig.h"

#include "States.h"

using namespace States;

namespace Feedback
{
    void init(char (&f_m)[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD],
        struct States::AppState &a_s,
        SemaphoreHandle_t &f_m_m,
        SemaphoreHandle_t &a_s_m,
        SemaphoreHandle_t &s_m
    );

    constexpr uint8_t FEEDBACK_TASK_TIMEOUT = 100;

    constexpr float LOAD_CELL_CALIBRATION_FACTOR = -35.3f;

    constexpr float AMMETER_VOLTS           = 3.3; // 3.3V for ESP32
    constexpr uint16_t AMMETER_MAX_ADC      = 4095; // 4096 for ESP32
    constexpr float AMMETER_MV_PER_AMPERE   = 185; // 185 for 5A ACS712 version (https://github.com/RobTillaart/ACS712)
    
}

#endif