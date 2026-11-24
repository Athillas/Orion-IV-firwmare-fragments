// Network.h
#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

#include "Configs/NetworkConfig.h"

#include "States.h"

using namespace States;

namespace Network
{
    void init(char (&f_m)[NetworkConfig::MAX_FEEDBACK_JSON_PAYLOAD], struct States::AppState &a_s,  SemaphoreHandle_t &f_m_m, SemaphoreHandle_t &a_s_m, SemaphoreHandle_t &s_m);
    constexpr uint32_t HANDLE_NETWORK_TASK_TIMEOUT = 100;
}

#endif