// States.h
#ifndef STATES_H
#define STATES_H

#include <cstdint>
#include <PubSubClient.h>
#include <Ethernet.h>

#include "Enums.h"

using namespace Enums;

namespace States
{
    struct AppState
    {
        AppErrors error_state;
        uint8_t switches_state;
    };
    
    struct NetworkState
    {
        EthernetClient eth_client;
        PubSubClient client;
        uint32_t lastMqttCmdTime;
        uint32_t lastReconnectAttempt;
    };

    struct FeedbackState
    {
        float tempc_;
        float bat_1_v;
        float bat_2_v;
        float bat_3_v;
        float bat_4_v;
        uint16_t adc_wl;
        uint16_t adc_ram;
        uint16_t adc_elec;
        uint16_t adc_sci;
        uint16_t adc_inne;
        uint8_t switches_state;
    };
}

#endif