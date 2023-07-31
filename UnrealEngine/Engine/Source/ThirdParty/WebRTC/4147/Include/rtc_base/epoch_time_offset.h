#ifndef RTC_BASE_EPOCH_TIME_OFFSET_H_
#define RTC_BASE_EPOCH_TIME_OFFSET_H_

#include <time.h>
#include <atomic>

/** UE function to return the offset of time between actual unix epoch and the value returned by POSIX time() */
time_t UE_epoch_time_offset();

#endif