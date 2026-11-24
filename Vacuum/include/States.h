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
        uint32_t collecting_time_ms;
        uint32_t dropping_time_ms;
        VacuumState vacuum_state;
        VacuumCommand vacuum_command;
        AppErrors error_state;
        ServoLidState lid_state;
        bool is_running_drill;
        bool is_running_vacuum;
        bool is_running_vibro;
        // bool is_ready;
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
        float bldc_ammeter_value;
        float compressor_valve_ammeter_value;
        float load_cell_value;
        uint32_t bldc_temperature_value;
        VacuumCommand vacuum_command;
        VacuumState vacuum_state;
        AppErrors error_state;
        bool is_ready;
    };
}

#endif