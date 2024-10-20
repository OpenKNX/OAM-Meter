#pragma once

#define OKNXHW_REG1_APP_SEN_MULTI

#include "OpenKNXHardware.h"
#include "SmartMFHardware.h"

#ifdef OKNXHW_REG1_BASE_V1
    #define OPENKNX_BI_GPIO_PINS OKNXHW_REG1_APP_SEN_MULTI_BINARY_INPUT1_PIN, OKNXHW_REG1_APP_SEN_MULTI_BINARY_INPUT2_PIN, OKNXHW_REG1_APP_SEN_MULTI_BINARY_INPUT3_PIN
    #define OPENKNX_BI_GPIO_COUNT 3
    #define OPENKNX_BI_PULSE -1
    #define OPENKNX_BI_PULSE_PAUSE_TIME 0
    #define OPENKNX_BI_PULSE_WAIT_TIME 0
    #define OPENKNX_BI_ONLEVEL HIGH
#endif

#ifdef SMARTMF_1TE_RP2040_BE3
#endif

#ifdef SMARTMF_2TE_RP2040_BE2_SML2
#endif

