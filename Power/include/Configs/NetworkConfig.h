// NetworkConfig.h
#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <cstdint>

namespace NetworkConfig
{
    // --- Network adresses ---
    constexpr const char* const MQTT_SERVER_IP      = "192.168.1.1"; 
    constexpr const char* const MQTT_ID             = "Vacuum"; // !!!requires clarification!!!

    // MAC and IP are defined in Network.cpp.

    // --- MQTT configuration ---
    constexpr uint16_t MQTT_PORT                    = 1883;
    constexpr uint16_t MQTT_FEEDBACK_DELAY          = 100;
    constexpr uint16_t HANDLE_NETWORK_TASK_TIMEOUT  = 100;
    
    // --- MQTT Topics ---
    // Only two needed by design (_enc topic for feedback, _cmd topic for command), so these names are totally ok.
    constexpr const char* const TOPIC_ENC           = "Vacuum_enc";
    constexpr const char* const TOPIC_CMD           = "Vacuum_cmd";
    constexpr uint8_t TOPIC_CMD_LENGTH                = 10; // Length of TOPIC_CMD

    // --- Network Safety ---
    constexpr uint16_t MQTT_SAFETY_TIMEOUT          = 1000;
    constexpr uint16_t RECONNECTION_TIMEOUT         = 5000;

    constexpr uint16_t MAX_FEEDBACK_JSON_PAYLOAD    = 128; // 80 bytes max for mixed-type 11-element array.
    constexpr uint16_t MAX_COMMAND_JSON_PAYLOAD     = 16; // 9 bytes for single uint8_t.

    struct MqttFeedbackMessage
    {
        char topic[64];
        char payload[MAX_FEEDBACK_JSON_PAYLOAD];
    };

    // Neopixel indicator configuration.
    constexpr uint8_t NUMPIXELS = 8;

    constexpr uint32_t RGB_GREEN            = ((uint32_t)0 << 16) | ((uint32_t)255 << 8) | 0;
    constexpr uint32_t RGB_RED              = ((uint32_t)255 << 16) | ((uint32_t)0 << 8) | 0;
}

#endif