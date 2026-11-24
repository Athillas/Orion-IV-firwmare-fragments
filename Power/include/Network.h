// Network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

#include "Configs/NetworkConfig.h"

#include "States.h"

using namespace States;

namespace Network
{
    bool init(struct States::AppState &a_s,
        SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m,
        QueueHandle_t &cmd_queue, QueueHandle_t &feedback_queue,
        TaskHandle_t &hardware_task
    );
    constexpr uint32_t HANDLE_NETWORK_TASK_TIMEOUT = 100;
}

#endif