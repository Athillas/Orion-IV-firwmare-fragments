// Vacuum.cpp
#include <Arduino.h>
#include <cstdint>

#include "Vacuum.h"
#include "Pins.h"

#include "Enums.h"

// TODO: replace the servo lid controls with ledc instead of Servo library

namespace
{
    struct States::AppState *app_state;
    SemaphoreHandle_t *app_state_mutex;
    SemaphoreHandle_t *serial_mutex;
    
    bool is_running_drill = false, is_running_vacuum = false, is_running_vibro = false;
    
    ServoLidState lid_state = ServoLidState::CLOSED;

    bool was_collecting_run = false, was_dropping_run = false, was_cleaning_run = false;
}

static void vacuum_task(void *parameters);
static void handle_state_transition(VacuumState state);

static inline void drill_off();
static inline void drill_on();
static inline void vibro_off();
static inline void vibro_on();

void Vacuum::init(struct States::AppState &a_s, SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m)
{
    if(xSemaphoreTake(s_m, 50))
    {
        Serial.println("[VACUUM] Initializing...");
        xSemaphoreGive(s_m);
    }

    serial_mutex = &s_m;
    app_state_mutex = &a_s_m;

    if(xSemaphoreTake(a_s_m, 50))
    {
        app_state = &a_s;
        xSemaphoreGive(a_s_m);
    }
    
    // --- SERVO SETUP ---
    ledcSetup(Vacuum::PWM_CHANNEL_SERVO, Vacuum::PWM_FREQ_SERVO, Vacuum::PWM_RES_SERVO);
    ledcAttachPin(Pins::PWM_SERVO_PIN, Vacuum::PWM_CHANNEL_SERVO);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // --- BLDC SETUP ---
    ledcSetup(Vacuum::PWM_CHANNEL_BLDC, Vacuum::PWM_FREQ_BLDC, Vacuum::PWM_RES_BLDC);
    ledcAttachPin(Pins::PWM_BLDC_PIN, Vacuum::PWM_CHANNEL_BLDC);

    digitalWrite(Pins::DIR_PIN, LOW);
    digitalWrite(Pins::BRAKE_PIN, LOW);

    vTaskDelay(pdMS_TO_TICKS(500));

    // --- DC MOTORS SETUP ---
    drill_off();
    vibro_off();

    vTaskDelay(pdMS_TO_TICKS(500));
    
    bool is_task_created = xTaskCreate(
        vacuum_task,
        "VACUUM",
        4096,
        NULL,
        3,
        NULL
    ) == pdPASS;

    if(xSemaphoreTake(s_m, 50))
    {
        if(is_task_created)
        {
            Serial.println("[VACUUM] Initialization finished.\n");
        }
        else
        {
            Serial.println("[VACUUM] Failed to create VACUUM task!");
        }
        xSemaphoreGive(s_m);
    }

}

void vacuum_task(void *parameters)
{
    //TickType_t last_wake_time = xTaskGetTickCount();
    for(;;)
    {
        VacuumCommand command = VacuumCommand::TO_IDLE;
        VacuumState state = VacuumState::IDLE;
        uint32_t collecting_time_ms = Vacuum::CTIME_DEFAULT, dropping_time_ms = Vacuum::DTIME_DEFAULT;

        if(xSemaphoreTake(*app_state_mutex, 50))
        {
            command = app_state->vacuum_command;
            collecting_time_ms = app_state->collecting_time_ms;
            dropping_time_ms = app_state->dropping_time_ms;

            // --- Writes ---

            app_state->lid_state = lid_state;
            app_state->is_running_drill = is_running_drill;
            app_state->is_running_vacuum = is_running_vacuum;
            app_state->is_running_vibro = is_running_vibro;

            switch (command)
            {
                case VacuumCommand::TO_IDLE:
                case VacuumCommand::TO_COLLECTING:
                case VacuumCommand::TO_DROPPING:
                    if(is_running_drill || is_running_vacuum || is_running_vibro || lid_state == ServoLidState::OPEN )
                    {
                        break;
                    }
                case VacuumCommand::TO_STOP:
                case VacuumCommand::TO_CLEANING:
                    handle_state_transition(static_cast<VacuumState>(command));
                    break;
                case VacuumCommand::CLEAR_ERRORS:
                    app_state->error_state = AppErrors::NO_ERROR;

                    if(can_transition(app_state->vacuum_state, VacuumState::IDLE))
                    {
                        app_state->vacuum_state = VacuumState::IDLE;
                    }
                    else
                    {
                        app_state->error_state = AppErrors::STATE_TRANSITION_ERROR;
                    }
                    break;
            }

            state = app_state->vacuum_state;

            xSemaphoreGive(*app_state_mutex);
        }

        static uint32_t cstart_time = 0, dstart_time = 0;

        switch (state)
        {
            case VacuumState::COLLECTING:
                if(!was_collecting_run && !is_running_vacuum && !is_running_drill)
                {
                    ledcWrite(Vacuum::PWM_CHANNEL_BLDC, Vacuum::PWM_BLDC_DUTY);
                    vTaskDelay(pdMS_TO_TICKS(500));
                    is_running_vacuum = true;
                    drill_on();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    is_running_drill = true;

                    cstart_time = millis();
                    was_collecting_run = true;
                }
                
                if(millis() - cstart_time < collecting_time_ms)
                    break;

                ledcWrite(Vacuum::PWM_CHANNEL_BLDC, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                drill_off();

                is_running_drill = is_running_vacuum = false;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case VacuumState::DROPPING:
                if(!was_dropping_run && lid_state == ServoLidState::CLOSED)
                {
                    ledcWrite(Vacuum::PWM_CHANNEL_SERVO, ServoLidState::OPEN);
                    lid_state = ServoLidState::OPEN;
                    vTaskDelay(pdMS_TO_TICKS(500));
                    vibro_on();
                    is_running_vibro = true;

                    dstart_time = millis();
                    was_dropping_run = true;
                }
                
                if(millis() - dstart_time < dropping_time_ms)
                    break;

                ledcWrite(Vacuum::PWM_CHANNEL_SERVO, ServoLidState::CLOSED);
                vTaskDelay(pdMS_TO_TICKS(500));
                vibro_off();
                
                lid_state = ServoLidState::CLOSED;
                is_running_vibro = false;
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case VacuumState::IDLE:
            case VacuumState::STOP:
            case VacuumState::ERROR:
                drill_off();
                vibro_off();
                
                ledcWrite(Vacuum::PWM_CHANNEL_BLDC, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                ledcWrite(Vacuum::PWM_CHANNEL_SERVO, ServoLidState::CLOSED);
                vTaskDelay(pdMS_TO_TICKS(100));

                lid_state = ServoLidState::CLOSED;
                is_running_drill = is_running_vacuum = is_running_vibro = false;
                was_collecting_run = was_cleaning_run = was_dropping_run = false;
                break;
            case VacuumState::CLEANING:
                was_collecting_run = was_dropping_run = false;
                if(!was_cleaning_run)
                {
                    digitalWrite(Pins::GPIO_ON_OFF1_PIN, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    digitalWrite(Pins::GPIO_ON_OFF2_PIN, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    digitalWrite(Pins::GPIO_ON_OFF1_PIN, LOW);
                    digitalWrite(Pins::GPIO_ON_OFF2_PIN, LOW);
                    was_cleaning_run = true;
                }
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(Vacuum::VACUUM_TASK_TIMEOUT));
    }

}

// !!! Call ONLY AFTER the requisition of app_state_mutex !!! 
static void handle_state_transition(VacuumState state)
{
    if(can_transition(app_state->vacuum_state, state))
    {
        app_state->vacuum_state = state;
        app_state->error_state = AppErrors::NO_ERROR;
    }
    else
    {
        app_state->error_state = AppErrors::STATE_TRANSITION_ERROR;
    }
}

static inline void drill_off()
{
    digitalWrite(Pins::INMOTOR3_PIN, LOW);
    digitalWrite(Pins::INMOTOR4_PIN, LOW);
}

static inline void drill_on()
{
    digitalWrite(Pins::INMOTOR3_PIN, HIGH);
    digitalWrite(Pins::INMOTOR4_PIN, LOW);
}

static inline void vibro_off()
{
    digitalWrite(Pins::INMOTOR1_PIN, LOW);
    digitalWrite(Pins::INMOTOR2_PIN, LOW);
}

static inline void vibro_on()
{
    digitalWrite(Pins::INMOTOR1_PIN, HIGH);
    digitalWrite(Pins::INMOTOR2_PIN, LOW);
}