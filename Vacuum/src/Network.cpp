// Network.cpp
#include <Arduino.h>
#include <Ethernet.h>
#include <SPI.h>

#include "Configs/NetworkConfig.h"

#include "Pins.h"
#include "Enums.h"
#include "States.h"
#include "JsonArrayParser.h"
#include "Network.h"

using namespace States;

namespace
{
    uint8_t MAC_[6];
    uint8_t IP_[4];
    struct States::NetworkState network_state_{};
    
    char *feedback_message = nullptr;
    struct States::AppState *app_state = nullptr;
    
    SemaphoreHandle_t *as_mtx_ =  nullptr;
    SemaphoreHandle_t *serial_mtx_ = nullptr;  // Added for serial protection
    SemaphoreHandle_t *fm_mutex_ = nullptr;
    
}

void callback(const char* topic, const byte* payload, uint16_t length);
void handleNetworkTask(void *parameters);

void Network::init(char (&f_m)[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD], struct States::AppState &a_s, SemaphoreHandle_t &f_m_m, SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m)
{
    network_state_.client.setClient(network_state_.eth_client);

    digitalWrite(Pins::RST_PIN, LOW); vTaskDelay(pdMS_TO_TICKS(100));
    digitalWrite(Pins::RST_PIN, HIGH); vTaskDelay(pdMS_TO_TICKS(200));  
    SPI.begin(Pins::SCK_PIN, Pins::MISO_PIN, Pins::MOSI_PIN, Pins::CS_PIN);

    serial_mtx_ = &s_m;
    as_mtx_ = &a_s_m;
    fm_mutex_ = &f_m_m;

    if(xSemaphoreTake(s_m, 50) == pdTRUE)
    {
        Serial.println("[NETWORK] Initializing...");
        xSemaphoreGive(s_m);
    }

    if(xSemaphoreTake(a_s_m, 50) == pdTRUE)
    {
        app_state = &a_s;
        xSemaphoreGive(a_s_m);
    }

    if(xSemaphoreTake(f_m_m, 50) == pdTRUE)
    {
        feedback_message = f_m;
        xSemaphoreGive(f_m_m);
    }

    memcpy(MAC_, NetworkConfig::MAC, 6);
    memcpy(IP_, NetworkConfig::IP, 4);

    Ethernet.init(Pins::CS_PIN);
    Ethernet.begin(MAC_, IP_);

    if(xSemaphoreTake(s_m, 50) == pdTRUE)
    {
        if(Ethernet.hardwareStatus() != EthernetHardwareStatus::EthernetNoHardware)
        {
            Serial.print("[NETWORK] Hardware connected. IP: "); Serial.print(Ethernet.localIP()); Serial.println(); 
        }
        else
        {
            Serial.println("[NETWORK] No WizNet found, recheck the wiring!");
        }
        xSemaphoreGive(s_m);
    }

    bool is_task_created = xTaskCreate(
        handleNetworkTask,
        "NETWORK",
        8192,
        NULL,
        1,
        NULL
    ) == pdPASS;

    network_state_.client.setServer(NetworkConfig::MQTT_SERVER_IP, NetworkConfig::MQTT_PORT);
    // network_state_.client.connect(NetworkConfig::MQTT_ID);
    network_state_.client.setCallback(callback);
    network_state_.client.setBufferSize(NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD);
    
    if(xSemaphoreTake(s_m, 50) == pdTRUE)
    {
        if(is_task_created)
        {
            Serial.println("[NETWORK] Initialization finished.\n");
        }
        else
        {
            Serial.println("[NETWORK] Failed to create HandleNetwork task!");
        }

        xSemaphoreGive(s_m);
    }
}   

void callback(const char* topic, const byte* payload, uint16_t length)
{
    static uint32_t cmds[3];
    
    if(strncmp(topic, NetworkConfig::TOPIC_CMD, NetworkConfig::TOPIC_CMD_LENGTH) != 0)
    {
        return;
    }

    if(length > NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD)
    {
        if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
        {

            Serial.println("[CALLBACK] Packet size exceeds the MAX_JSON_PAYLOAD!");
            xSemaphoreGive(*serial_mtx_);
        }
        return;
    }
    
    network_state_.lastMqttCmdTime = millis();

    
    // "[0, 5000, 5000]"

    bool res = JsonArrayParser::deserialize_json_u32_array(
        cmds, 3,
        payload, length
    );
    
    if(!res)
	{
        if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
        {
            Serial.print("[CALLBACK] Failed to deserialize the JSON!");
            xSemaphoreGive(*serial_mtx_);
        }
		return;
	}

    VacuumCommand vacuum_command = static_cast<VacuumCommand>(cmds[0]);
    uint32_t ctime_ms = cmds[1], dtime_ms = cmds[2];

    if(ctime_ms == 0 || dtime_ms == 0)
        return; // time cannot be 0

    if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
    {
        Serial.print("\n[CALLBACK] Parsed command array: [");
        Serial.print(vacuum_command);
        Serial.print(',');
        Serial.print(ctime_ms);
        Serial.print(',');
        Serial.print(dtime_ms);
        Serial.print("]\n");
        xSemaphoreGive(*serial_mtx_);
    }
    
    if(xSemaphoreTake(*as_mtx_, 50) == pdTRUE)
    {
        if(vacuum_command > VacuumCommand::CLEAR_ERRORS)
        {
            app_state->error_state = AppErrors::UNKNOWN_COMMAND_ERROR;
        }
        else
        {
            app_state->vacuum_command = static_cast<VacuumCommand>(vacuum_command);
            app_state->collecting_time_ms = ctime_ms;
            app_state->dropping_time_ms = dtime_ms;
        }
        xSemaphoreGive(*as_mtx_);
    }
}

void handleNetworkTask(void *parameters)
{
    static char local_payload[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD];
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    for(;;)
    {
        if(!network_state_.client.connected())
        {
            if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
            {
                Serial.println("[NETWORK] Disconnected. Attempting to connect to the client...");
                xSemaphoreGive(*serial_mtx_);
            }
            bool is_connected = false;
            if(network_state_.client.connect(NetworkConfig::MQTT_ID))
            {
                is_connected = true;
                network_state_.client.subscribe(NetworkConfig::TOPIC_CMD);
            }
            else
            {
                is_connected = false;
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
            {
                if(is_connected)
                {
                    Serial.println("[NETWORK] Client connected successfully.");
                }
                else
                {
                    Serial.println("[NETWORK] Failed to connect to the client. Reattempting in 5000ms");
                }
                xSemaphoreGive(*serial_mtx_);
            }
        }
        else
        {
            network_state_.client.loop();

            if(xSemaphoreTake(*fm_mutex_, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                strncpy(local_payload, feedback_message, NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD);
                xSemaphoreGive(*fm_mutex_);
            }

            if (local_payload[0] != '\0')
            {
                network_state_.client.publish(NetworkConfig::TOPIC_ENC, local_payload, false);
            }
            else
            {
                if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
                {
                    Serial.println("[NETWORK] Feedback message is empty");
                    xSemaphoreGive(*serial_mtx_);
                }
            }
        }
        
        vTaskDelay(Network::HANDLE_NETWORK_TASK_TIMEOUT);
        // vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(Network::HANDLE_NETWORK_TASK_TIMEOUT));
    }
}