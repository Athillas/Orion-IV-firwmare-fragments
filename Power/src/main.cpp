#include <Arduino.h>
#include <freertos/queue.h>

#include "Network.h"
#include "Hardware.h"
#include "Pins.h"
#include "States.h"
#include "Configs/HardwareConfig.h"

struct States::AppState app_state = {
    .error_state = Enums::AppErrors::NO_ERROR,
    .switches_state = 0
};

SemaphoreHandle_t serial_mtx{}, app_state_mtx{};
QueueHandle_t feedback_queue{}, command_queue{};
TaskHandle_t hardware_task{};

void setup()
{
    vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));
    Serial.begin(HardwareConfig::SERIAL_BAUD_RATE);
    vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));

    Serial.println("\n--- SYSTEM BOOT START ---");

    serial_mtx = xSemaphoreCreateMutex();
    app_state_mtx = xSemaphoreCreateMutex();

    feedback_queue = xQueueCreate(30, sizeof(char[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD]));

    assert(feedback_queue != 0);
    
    if(feedback_queue == 0)
    {
        app_state.error_state = AppErrors::HARDWARE_ERROR;
        if(xSemaphoreTake(serial_mtx, HardwareConfig::MTX_TAKE_SHORT))
        {
            Serial.println("[INIT] Failed to init feedback_queue");
            abort();
            xSemaphoreGive(serial_mtx);
        }
    }

    command_queue = xQueueCreate(5, sizeof(uint8_t));

    assert(command_queue != 0);

    if(command_queue == 0)
    {
        app_state.error_state = AppErrors::HARDWARE_ERROR;
        if(xSemaphoreTake(serial_mtx, HardwareConfig::MTX_TAKE_SHORT))
        {
            Serial.println("[INIT] Failed to init command_queue");
            abort();
            xSemaphoreGive(serial_mtx);
        }
    }

    Pins::init_pins();
    vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));

    bool is_init_network = Network::init(app_state, app_state_mtx, serial_mtx, command_queue, feedback_queue, hardware_task);
    
    if(xSemaphoreTake(serial_mtx, HardwareConfig::MTX_TAKE_SHORT))
    {
        Serial.println("[PINS] Init successful.");

        if(!is_init_network)
        {
            Serial.println("[NETWORK] Failed to initialize network! Aborting...");
            //abort();
            //return;
        }
        else
        {
            Serial.println("[NETWORK] Init successful.");
        }
        xSemaphoreGive(serial_mtx);
    }

    bool is_init_hardware = Hardware::init(&command_queue, &feedback_queue, &serial_mtx);

    if(xSemaphoreTake(serial_mtx, HardwareConfig::MTX_TAKE_SHORT))
    {
        if(!is_init_hardware)
        {
            Serial.println("[HARDWARE] Failed to initialize hardware! Aborting...");
            // abort();
            // return;
        }
        else
        {
            Serial.println("[HARDWARE] Init successful.");
            Serial.println("--- SYSTEM BOOT FINISHED ---\n\n");
        }
        xSemaphoreGive(serial_mtx);
    }
    vTaskDelete(NULL);
}

void loop()
{
}
