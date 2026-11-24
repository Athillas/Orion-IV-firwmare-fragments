// Network.cpp
#include <Arduino.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <SPI.h>

#include <Adafruit_NeoPixel.h>

#include "Configs/NetworkConfig.h"
#include "Configs/HardwareConfig.h"
#include "Network.h"
#include "Pins.h"

namespace
{
    struct States::NetworkState network_state_{0};
    struct States::AppState *app_state{};
    SemaphoreHandle_t *as_mtx_{}, *serial_mtx_{};
    QueueHandle_t *cmd_queue_{}, *feedback_queue_{};
    TaskHandle_t *hardware_task_handle_{};
    Adafruit_NeoPixel pixels(NetworkConfig::NUMPIXELS, Pins::NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    uint8_t MAC[6]                                  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x56 };
    uint8_t IP[4]                                   = { 192, 168, 1, 56 };
    uint8_t failed_attempts = 0;

    bool ustr_to_u8(const unsigned char *str, const uint8_t str_len, uint8_t *res)
    {
        const char *char_str = reinterpret_cast<const char*>(str);
        for(uint8_t i = 0; i < str_len; i++)
        {
            if(!isdigit(char_str[i])) return false;
        }
        *res = atoi(char_str);
        return true;
    }

    void restart_wiznet()
    {
        digitalWrite(Pins::WIZ_RST_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));
        digitalWrite(Pins::WIZ_RST_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));
    }

    void reconnect(bool &is_connected, bool &is_subscribed)
    {
        using namespace NetworkConfig;
        is_connected = network_state_.client.connect(MQTT_ID);
        is_subscribed = false;

        failed_attempts++;

        pixels.fill(is_connected ? RGB_GREEN : RGB_RED);
        pixels.show();

        char const* msg = is_connected ?
            "[NETWORK] Client connected successfully." :
            "[NETWORK] Failed to connect to the client. Reattempting in 5000ms";

        if(xSemaphoreTake(*serial_mtx_, HardwareConfig::MTX_TAKE_SHORT) == pdTRUE)
        {
            Serial.println(msg);
            xSemaphoreGive(*serial_mtx_);
        }

        if(failed_attempts > 5)
        {
            if(xSemaphoreTake(*serial_mtx_, HardwareConfig::MTX_TAKE_SHORT) == pdTRUE)
            {
                Serial.println("[NETWORK] 5 failed attempts reached, restarting WizNet...");
                xSemaphoreGive(*serial_mtx_);
            }
            restart_wiznet();
        }

        vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT * 50));
    }

    void networkTask(void *parameters)
    {
        using namespace NetworkConfig;
        char payload[MAX_FEEDBACK_JSON_PAYLOAD];

        for(;;)
        {
            bool is_connected = network_state_.client.connected();
            static bool was_connected = !is_connected; // Has to differ from is_connected for the first run so that Neopixel fires at all.

            bool is_subscribed{false}, is_feedback_published{false};

            if(is_connected != was_connected)
            {
                was_connected = is_connected;
                uint32_t col = is_connected ? RGB_GREEN : RGB_RED;

                pixels.fill(col);
                pixels.show();
            }

            if(!is_connected)
            {
                xTaskNotify(*hardware_task_handle_, AppErrors::NETWORK_ERROR, eSetBits);
                reconnect(is_connected, is_subscribed);
                continue;
            }

            bool is_payload_received{false}, is_payload_published{false};

            network_state_.client.loop();

            if(!is_subscribed)
                is_subscribed = network_state_.client.subscribe(TOPIC_CMD);


            if (payload[0] != '\0' && is_subscribed)
            {
                is_payload_received = xQueueReceive(*feedback_queue_, &(payload), static_cast<TickType_t>(50));

                if(is_payload_received)
                {
                    is_payload_published = network_state_.client.publish(TOPIC_ENC, payload, false);
                }
            }

            if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
            {
                if(!is_payload_received)
                    Serial.println("[NETWORK] Failed to receive from feedback_queue!");

                if(!is_payload_published)
                    Serial.println("[NETWORK] Failed to publish feedback!");

                if(payload[0] == '\0')
                    Serial.println("[NETWORK] Feedback message is empty");
                
                if(!is_subscribed)
                    Serial.println("[NETWORK] Client is not subscribed!");

                xSemaphoreGive(*serial_mtx_);
            }

            vTaskDelay(HANDLE_NETWORK_TASK_TIMEOUT);
        }
    }
}

void callback(const char* topic, const byte* payload, uint16_t length)
{
    if(strncmp(topic, NetworkConfig::TOPIC_CMD, NetworkConfig::TOPIC_CMD_LENGTH) != 0)
    {
        return;
    }
    else if (length > NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD)
    {
        if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
        {
            Serial.println("[CALLBACK] Packet size exceeds the MAX_JSON_PAYLOAD!");
            xSemaphoreGive(*serial_mtx_);
        }
        return;
    }
    
    network_state_.lastMqttCmdTime = millis();
    uint8_t cmd = 0;

    if(!ustr_to_u8(payload, length, &cmd))
	{
        app_state->error_state = AppErrors::UNKNOWN_COMMAND_ERROR;
        if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
        {
            Serial.print("[CALLBACK] Failed to deserialize the JSON!");
            xSemaphoreGive(*serial_mtx_);
        }
		return;
	}

    if(!xQueueSend(*cmd_queue_, reinterpret_cast<void*>(cmd), static_cast<TickType_t>(0)))
    {
        app_state->error_state = AppErrors::STATE_TRANSITION_ERROR;
        
        if(xSemaphoreTake(*serial_mtx_, 50) == pdTRUE)
        {
            Serial.print("[CALLBACK] Failed to append to cmd_queue!");
            xSemaphoreGive(*serial_mtx_);
        }
		return;
    }
}

bool Network::init(struct States::AppState &a_s,
        SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m,
        QueueHandle_t &cmd_queue, QueueHandle_t &feedback_queue,
        TaskHandle_t &hardware_task)
{
    using namespace HardwareConfig;

    restart_wiznet();
    SPI.begin(Pins::WIZ_SCLK_PIN, Pins::WIZ_MISO_PIN, Pins::WIZ_MOSI_PIN, Pins::WIZ_SCN_PIN);
    Ethernet.init(Pins::WIZ_SCN_PIN);
    Ethernet.begin(MAC, IP);

    if(Ethernet.hardwareStatus() == EthernetHardwareStatus::EthernetNoHardware)
    {
        if(xSemaphoreTake(s_m, MTX_TAKE_SHORT) == pdTRUE)
        {
            Serial.println("[NETWORK] No WizNet found, recheck the wiring!");
            xSemaphoreGive(s_m);
        }
        return false;
    }
    else
    {
        if(xSemaphoreTake(s_m, MTX_TAKE_SHORT) == pdTRUE)
        {
            Serial.print("[NETWORK] Hardware connected. IP: "); Serial.print(Ethernet.localIP()); Serial.println(); 
            xSemaphoreGive(s_m);
        }
    }

    network_state_.client.setClient(network_state_.eth_client);
    network_state_.client.setCallback(callback);
    if(!network_state_.client.setBufferSize(NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD))
    {
        if(xSemaphoreTake(s_m, MTX_TAKE_SHORT) == pdTRUE)
        {
            Serial.println("[NETWORK] Failed to set network buffer size!");
            xSemaphoreGive(s_m);
        }
        return false;
    }

    hardware_task_handle_ = &hardware_task;
    serial_mtx_ = &s_m;
    as_mtx_ = &a_s_m;
    cmd_queue_ = &cmd_queue;
    feedback_queue_ = &feedback_queue;
    if(xSemaphoreTake(a_s_m, MTX_TAKE_SHORT) == pdTRUE)
    {
        app_state = &a_s;
        xSemaphoreGive(a_s_m);
    }
    assert(app_state != nullptr);
    
    bool is_task_created = xTaskCreate(
        networkTask,
        "NETWORK",
        2048,
        NULL,
        1,
        NULL
    ) == pdPASS;

    if(!is_task_created)
    {
        if(xSemaphoreTake(s_m, MTX_TAKE_SHORT) == pdTRUE)
        {
            Serial.println("[NETWORK] Failed to create networkTask!");
            xSemaphoreGive(s_m);
        }
        return false;
    }

    return true;
}