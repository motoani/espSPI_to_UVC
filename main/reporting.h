#pragma once

#include "stdint.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

void fps_report_cb( TimerHandle_t);