// Hardware.cpp
#include <DallasTemperature.h>
#include <OneWire.h>
#include "Hardware.h"
#include "Enums.h"
#include "Configs/HardwareConfig.h"
#include "Configs/NetworkConfig.h"
#include "States.h"

namespace
{
    QueueHandle_t *cmd_queue_{};
    QueueHandle_t *feedback_queue_{};
    SemaphoreHandle_t *serial_mtx_{};
    OneWire oneWire(Pins::TEMP_PIN);
    DallasTemperature sensors(&oneWire);

    float readBatteryVoltage(uint8_t pin, float multiplier)
    {
        using namespace HardwareConfig;

        return (analogRead(pin) * ADC_VREF * multiplier) / static_cast<float>(ADC_RES);
    }

    void hardwareTask(void *parameters)
    {
        using namespace HardwareConfig;
        using namespace Pins;

        uint8_t cmd{0};
        char feedback_buf[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD] {0};

        for(;;)
        {
            if(xQueueReceive(*cmd_queue_, &cmd, 10))
            {
                for(uint8_t i = 0; i < 7; i++)
                    digitalWrite(pin_by_offset[i], (pin_by_offset[i] & (1U << i)) ? HIGH : LOW);
            }

            sensors.requestTemperatures();
            float temp_c        = sensors.getTempCByIndex(0);
            if(temp_c == DEVICE_DISCONNECTED_C)
                temp_c  = -127.0;
            float bat_1_v       = readBatteryVoltage(Pins::BAT_1_ADC_PIN, bat_multipliers[0]);
            float bat_2_v       = readBatteryVoltage(Pins::BAT_2_ADC_PIN, bat_multipliers[1]);
            float bat_3_v       = readBatteryVoltage(Pins::BAT_3_ADC_PIN, bat_multipliers[2]);
            float bat_4_v       = readBatteryVoltage(Pins::BAT_4_ADC_PIN, bat_multipliers[3]);
            uint16_t adc_wl     = analogRead(Pins::ADC_WL_PIN);
            uint16_t adc_wr     = analogRead(Pins::ADC_WR_PIN);
            uint16_t adc_ram    = analogRead(Pins::ADC_RAM_PIN);
            uint16_t adc_elek   = analogRead(Pins::ADC_ELEK_PIN);
            uint16_t adc_sci    = analogRead(Pins::ADC_SCI_PIN);
            uint16_t adc_inne   = analogRead(Pins::ADC_INNE_PIN);

            size_t fbuf_len = snprintf(
                feedback_buf,
                NetworkConfig::MAX_COMMAND_JSON_PAYLOAD - 1,
                "[%f,%f,%f,%f,%f,%u,%u,%u,%u,%u,%u]",
                temp_c, bat_1_v, bat_2_v, bat_3_v, bat_4_v,
                adc_wl, adc_wr, adc_ram, adc_elek, adc_sci, adc_inne
            );

            if(!xQueueSend(*feedback_queue_, feedback_buf, 50) == pdTRUE)
            {
                if(xSemaphoreTake(*serial_mtx_, HardwareConfig::MTX_TAKE_SHORT))
                {
                    Serial.println("[HARDWARE] feedback_queue is full! Dropping the update.");
                    xSemaphoreGive(*serial_mtx_);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(HardwareConfig::TIMEOUT_SHORT));
        }
    }
}

bool Hardware::init(QueueHandle_t *cmd_queue, QueueHandle_t *feedback_queue, SemaphoreHandle_t *serial_mtx)
{
    if(cmd_queue == nullptr || feedback_queue == nullptr || serial_mtx == nullptr) return false;
    cmd_queue_ = cmd_queue;
    feedback_queue_ = feedback_queue;
    serial_mtx_ = serial_mtx;

    return xTaskCreate(
        hardwareTask,
        "HARDWARE",
        2048,
        NULL,
        1,
        NULL
    ) == pdTRUE;
}