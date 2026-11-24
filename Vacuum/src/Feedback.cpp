// Feedback.cpp

#include <ACS712.h>
#include <HX711.h>

#include "Feedback.h"
#include "Pins.h"
#include "States.h"

using namespace States;

namespace
{
    char *feedback_message = nullptr;
    struct States::AppState *app_state = nullptr;

    SemaphoreHandle_t *serial_mutex;
    SemaphoreHandle_t *feedback_message_mutex;
    SemaphoreHandle_t *app_state_mutex;

    ACS712 bldc_ammeter(Pins::VESP321_PIN, Feedback::AMMETER_VOLTS, Feedback::AMMETER_MAX_ADC, Feedback::AMMETER_MV_PER_AMPERE);
    ACS712 compressor_valve_ammeter(Pins::VESP322_PIN, Feedback::AMMETER_VOLTS, Feedback::AMMETER_MAX_ADC, Feedback::AMMETER_MV_PER_AMPERE);
    HX711 load_cell;

}

void feedback_task(void *parameters);

void Feedback::init(char (&f_m)[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD],
    struct States::AppState &a_s, 
    SemaphoreHandle_t &f_m_m,
    SemaphoreHandle_t &a_s_m,
    SemaphoreHandle_t &s_m
)
{
    feedback_message_mutex = &f_m_m;
    app_state_mutex = &a_s_m;
    serial_mutex = &s_m;

    if(xSemaphoreTake(s_m, 50))
    {
        Serial.println("[FEEDBACK] Initializing...");
        xSemaphoreGive(s_m);
    }

    if(xSemaphoreTake(f_m_m, 50))
    {
        feedback_message = f_m;
        feedback_message[0] = '\0';
        xSemaphoreGive(f_m_m);
    }
    
    if(xSemaphoreTake(a_s_m, 50))
    {
        app_state = &a_s;
        xSemaphoreGive(a_s_m);
    }

    if (load_cell.wait_ready_timeout(50))
    {
        load_cell.set_scale(Feedback::LOAD_CELL_CALIBRATION_FACTOR);
        load_cell.tare();
    }

    bool is_task_created = xTaskCreate(
        feedback_task,
        "FEEDBACK",
        4096,
        NULL,
        2,
        NULL) == pdPASS;

    if(xSemaphoreTake(s_m, 50))
    {
        if(is_task_created)
        {
            Serial.println("[FEEDBACK] Feedback task created successfully. Initialization complete.");
        }
        else
        {
            Serial.println("[FEEDBACK] Failed to create the feedback task!");
        }
        xSemaphoreGive(s_m);
    }
}

void feedback_task(void *parameters)
{
    States::AppState app_state_{};

    char feedback_message_[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD]{0};
    
    for(;;)
    {
        float bldc_ammeter_value_ = bldc_ammeter.mA_DC();
        float compressor_valve_ammeter_value_ = compressor_valve_ammeter.mA_DC();
        float load_cell_value_ = 0.0f;

        if (load_cell.wait_ready_timeout(50))
        {
            load_cell_value_ = load_cell.get_units(2); 
        }

        uint32_t bldc_temperature_value_ = analogRead(Pins::TEMP_BLDC_PIN);

        if(xSemaphoreTake(*app_state_mutex, 50))
        {
            app_state_ = *app_state;
            xSemaphoreGive(*app_state_mutex);
        }

        /*
            [
                0: bldc_ammeter_value_              (float)
                1: compressor_valve_ammeter_value_  (float)
                2: load_cell_value_                 (float)
                3: bldc_temperature_value_          (uint16_t)
                4: VacuumCommand                    (uint8_t)
                5: VacuumState                      (uint8_t)
                6: AppErrors                        (uint8_t)
                7: isReady                          (uint8_t)
            ]
        */

        // struct FeedbackState fs = {
        //     .bldc_ammeter_value = bldc_ammeter_value_,
        //     .compressor_valve_ammeter_value = compressor_valve_ammeter_value_,
        //     .load_cell_value = load_cell_value_,
        //     .bldc_temperature_value = bldc_temperature_value_,
        //     .vacuum_command = app_state_.vacuum_command,
        //     .vacuum_state = app_state_.vacuum_state,
        //     .error_state = app_state_.error_state,
        //     .is_ready = app_state_.is_ready
        // };
       
        // uint16_t len = JsonArrayParser::serialize_feedback_state(
        //     feedback_message_,
        //     sizeof(feedback_message_),
        //     &fs
        // );
       
        uint16_t len = snprintf(
            feedback_message_,
            sizeof(feedback_message_),
            "[%.2f,%.2f,%.2f,%u,%u,%u,%u,%u,%u,%u,%u]",
            bldc_ammeter_value_,
            compressor_valve_ammeter_value_,
            load_cell_value_,
            bldc_temperature_value_,
            app_state_.vacuum_command,
            app_state_.vacuum_state,
            app_state_.error_state,
            app_state_.lid_state,
            //app_state_.is_ready,
            app_state_.is_running_vacuum,
            app_state_.is_running_drill,
            app_state_.is_running_vibro
        );

        // if(xSemaphoreTake(*serial_mutex, 50))
        // {
        //     Serial.println(feedback_message_);
        //     xSemaphoreGive(*serial_mutex);
        // }

        if(len == 0)
        {
            if(xSemaphoreTake(*serial_mutex, 50))
            {
                Serial.println("[FEEDBACK] Failed to serialize the feedback_message_ buffer!");
                xSemaphoreGive(*serial_mutex);
            }
        }
        else
        {
            if(xSemaphoreTake(*feedback_message_mutex, 50))
            {
                memcpy(feedback_message, feedback_message_, len + 1);
                xSemaphoreGive(*feedback_message_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(Feedback::FEEDBACK_TASK_TIMEOUT));
    }
}