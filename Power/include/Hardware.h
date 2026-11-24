// Hardware.h
#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <cstdint>

#include "Pins.h"
#include "Enums.h"

namespace Hardware
{
    bool Hardware::init(QueueHandle_t *cmd_queue, QueueHandle_t *feedback_queue, SemaphoreHandle_t *serial_mtx);
}


#endif