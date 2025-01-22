#pragma once
#include <stdint.h>

/**
 * The precision of the RTC driver is milliseconds
 */
#define RTC_PRECISION 1000

void rtc_init(void);
uint64_t rtc_get_epoch(void);