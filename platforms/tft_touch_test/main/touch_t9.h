/**
 * @file touch_t9.h
 * @brief ESP32 built-in capacitive touch on pad T9 (== GPIO32).
 *        Uses the legacy touch_pad v1 API (first-class on IDF v4.4 for esp32).
 *        Reading DROPS below the idle baseline when the pad is touched.
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t touch_t9_baseline(void);   /* inits the peripheral, returns averaged idle value */
uint16_t touch_t9_read(void);       /* current filtered reading                          */

#ifdef __cplusplus
}
#endif
