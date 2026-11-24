// main.cpp
#include <Arduino.h>
#include <PubSubClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Pins.h"
#include "Vacuum.h"
#include "Network.h"
#include "Feedback.h"

#include "Configs/HardwareConfig.h"
#include "Configs/NetworkConfig.h"

#include "States.h"
#include "Enums.h"

States::AppState app_state = {
    .collecting_time_ms = Vacuum::CTIME_DEFAULT,
    .dropping_time_ms = Vacuum::DTIME_DEFAULT,
    .vacuum_state = VacuumState::IDLE,
    .vacuum_command = VacuumCommand::TO_IDLE,
    .error_state = AppErrors::NO_ERROR,
    .lid_state = ServoLidState::CLOSED,
    .is_running_drill = false,
    .is_running_vacuum = false,
    .is_running_vibro = false
    //.is_ready = false
};

SemaphoreHandle_t app_state_mutex, serial_mutex, feedback_message_mutex; // App State, Serial and Feedback Message Semaphores

char feedback_message[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD] {0};

void setup()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.begin(HardwareConfig::SERIAL_BAUD_RATE);
    vTaskDelay(pdMS_TO_TICKS(5000));

    app_state_mutex = xSemaphoreCreateMutex();
    serial_mutex = xSemaphoreCreateMutex();
    feedback_message_mutex = xSemaphoreCreateMutex();

    if(xSemaphoreTake(serial_mutex, 50))
    {
        Serial.println("\n--- SYSTEM BOOT ---\n\n");
        Serial.println("[PINS] Initializing...");
        xSemaphoreGive(serial_mutex);
    }

    Pins::init();

    if(xSemaphoreTake(serial_mutex, 50))
    {
        Serial.println("[PINS] Initialization finished.\n");
        xSemaphoreGive(serial_mutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));

    Vacuum::init(app_state, app_state_mutex, serial_mutex);
    vTaskDelay(pdMS_TO_TICKS(10));

    Network::init(feedback_message, app_state, feedback_message_mutex, app_state_mutex, serial_mutex);
    vTaskDelay(pdMS_TO_TICKS(10));

    Feedback::init(feedback_message, app_state, feedback_message_mutex, app_state_mutex, serial_mutex);

    if(xSemaphoreTake(serial_mutex, 50))
    {
        Serial.println("\n\n--- SYSTEM BOOT FINISHED ---\n");
        xSemaphoreGive(serial_mutex);
    }

    vTaskDelete(NULL);
}

void loop()
{
    Serial.println("Shouldn't get there...");
    vTaskDelay(pdMS_TO_TICKS(1000));
}