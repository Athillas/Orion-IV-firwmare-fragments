// HardwareConfig.h
#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

#include <cstdint>
#include "Pins.h"

namespace HardwareConfig
{
    constexpr uint32_t SERIAL_BAUD_RATE     = 115200;
    constexpr uint32_t TIMEOUT_SHORT        = 100;

    constexpr uint32_t MTX_TAKE_SHORT       = 50;

        
    constexpr float bat_multipliers[4]      = {7.286f, 7.8f, 7.8f, 7.8f};
    constexpr float ADC_VREF                = 3.3f;
    constexpr uint16_t ADC_RES              = 4095;    

    constexpr uint8_t pin_by_offset[7] = {
        Pins::FAN1_ON_PIN, Pins::S_W_L_PIN,
        Pins::S_W_R_PIN, Pins::S_RAM_PIN,
        Pins::S_ELEK_PIN, Pins::S_SCI_PIN,
        Pins::S_INNE_PIN
    };

}

#endif