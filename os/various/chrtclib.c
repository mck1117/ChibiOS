/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
                 2011,2012 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
   Concepts and parts of this file have been contributed by Uladzimir Pylinsky
   aka barthess.
 */

#include <time.h>

#include "ch.h"
#include "hal.h"

#include "chrtclib.h"

#if (defined(STM32F4XX) || defined(STM32F2XX) || defined(STM32L1XX) || \
     defined(STM32F1XX) || defined(STM32F10X_MD) || defined(STM32F10X_LD) || \
     defined(STM32F10X_HD))
#if STM32_RTC_IS_CALENDAR
/**
 * @brief   Converts from STM32 BCD to canonicalized time format.
 *
 * @param[out] timp     pointer to a @p tm structure as defined in time.h
 * @param[in] timespec  pointer to a @p RTCTime structure
 *
 * @notapi
 */
static void stm32_rtc_bcd2tm(struct tm *timp, RTCTime *timespec) {
  uint32_t tv_time = timespec->tv_time;
  uint32_t tv_date = timespec->tv_date;

#if CH_DBG_ENABLE_CHECKS
  timp->tm_isdst = 0;
  timp->tm_wday  = 0;
  timp->tm_mday  = 0;
  timp->tm_yday  = 0;
  timp->tm_mon   = 0;
  timp->tm_year  = 0;
  timp->tm_sec   = 0;
  timp->tm_min   = 0;
  timp->tm_hour  = 0;
#endif

  timp->tm_isdst = -1;

  timp->tm_wday = (tv_date & RTC_DR_WDU) >> RTC_DR_WDU_OFFSET;
  if (timp->tm_wday == 7)
    timp->tm_wday = 0;

  timp->tm_mday =  (tv_date & RTC_DR_DU) >> RTC_DR_DU_OFFSET;
  timp->tm_mday += ((tv_date & RTC_DR_DT) >> RTC_DR_DT_OFFSET) * 10;

  timp->tm_mon  =  (tv_date & RTC_DR_MU) >> RTC_DR_MU_OFFSET;
  timp->tm_mon  += ((tv_date & RTC_DR_MT) >> RTC_DR_MT_OFFSET) * 10;
  timp->tm_mon  -= 1;

  timp->tm_year =  (tv_date & RTC_DR_YU) >> RTC_DR_YU_OFFSET;
  timp->tm_year += ((tv_date & RTC_DR_YT) >> RTC_DR_YT_OFFSET) * 10;
  timp->tm_year += 2000 - 1900;

  timp->tm_sec  =  (tv_time & RTC_TR_SU) >> RTC_TR_SU_OFFSET;
  timp->tm_sec  += ((tv_time & RTC_TR_ST) >> RTC_TR_ST_OFFSET) * 10;

  timp->tm_min  =  (tv_time & RTC_TR_MNU) >> RTC_TR_MNU_OFFSET;
  timp->tm_min  += ((tv_time & RTC_TR_MNT) >> RTC_TR_MNT_OFFSET) * 10;

  timp->tm_hour =  (tv_time & RTC_TR_HU) >> RTC_TR_HU_OFFSET;
  timp->tm_hour += ((tv_time & RTC_TR_HT) >> RTC_TR_HT_OFFSET) * 10;
  timp->tm_hour += 12 * ((tv_time & RTC_TR_PM) >> RTC_TR_PM_OFFSET);
}

/**
 * @brief   Converts from canonicalized to STM32 BCD time format.
 *
 * @param[in] timp      pointer to a @p tm structure as defined in time.h
 * @param[out] timespec pointer to a @p RTCTime structure
 *
 * @notapi
 */
static void stm32_rtc_tm2bcd(struct tm *timp, RTCTime *timespec) {
  uint32_t v = 0;

  timespec->tv_date = 0;
  timespec->tv_time = 0;

  v = timp->tm_year - 100;
  timespec->tv_date |= ((v / 10) << RTC_DR_YT_OFFSET) & RTC_DR_YT;
  timespec->tv_date |= (v % 10) << RTC_DR_YU_OFFSET;

  if (timp->tm_wday == 0)
    v = 7;
  else
    v = timp->tm_wday;
  timespec->tv_date |= (v << RTC_DR_WDU_OFFSET) & RTC_DR_WDU;

  v = timp->tm_mon + 1;
  timespec->tv_date |= ((v / 10) << RTC_DR_MT_OFFSET) & RTC_DR_MT;
  timespec->tv_date |= (v % 10) << RTC_DR_MU_OFFSET;

  v = timp->tm_mday;
  timespec->tv_date |= ((v / 10) << RTC_DR_DT_OFFSET) & RTC_DR_DT;
  timespec->tv_date |= (v % 10) << RTC_DR_DU_OFFSET;

  v = timp->tm_hour;
  timespec->tv_time |= ((v / 10) << RTC_TR_HT_OFFSET) & RTC_TR_HT;
  timespec->tv_time |= (v % 10) << RTC_TR_HU_OFFSET;

  v = timp->tm_min;
  timespec->tv_time |= ((v / 10) << RTC_TR_MNT_OFFSET) & RTC_TR_MNT;
  timespec->tv_time |= (v % 10) << RTC_TR_MNU_OFFSET;

  v = timp->tm_sec;
  timespec->tv_time |= ((v / 10) << RTC_TR_ST_OFFSET) & RTC_TR_ST;
  timespec->tv_time |= (v % 10) << RTC_TR_SU_OFFSET;
}

/**
 * @brief   Gets raw time from RTC and converts it to canonicalized format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @param[out] timp     pointer to a @p tm structure as defined in time.h
 *
 * @api
 */
void rtcGetTimeTm(RTCDriver *rtcp, struct tm *timp) {
  RTCTime timespec = {0,0,FALSE,0};

  rtcGetTime(rtcp, &timespec);
  stm32_rtc_bcd2tm(timp, &timespec);
}

/**
 * @brief   Sets RTC time.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @param[out] timp     pointer to a @p tm structure as defined in time.h
 *
 * @api
 */
void rtcSetTimeTm(RTCDriver *rtcp, struct tm *timp) {
  RTCTime timespec = {0,0,FALSE,0};

  stm32_rtc_tm2bcd(timp, &timespec);
  rtcSetTime(rtcp, &timespec);
}

/**
 * @brief   Gets raw time from RTC and converts it to unix format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in seconds.
 *
 * @api
 */
time_t rtcGetTimeUnixSec(RTCDriver *rtcp) {
  RTCTime timespec = {0,0,FALSE,0};
  struct tm timp;

  rtcGetTime(rtcp, &timespec);
  stm32_rtc_bcd2tm(&timp, &timespec);

  return mktime(&timp);
}

/**
 * @brief   Sets RTC time.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in seconds.
 *
 * @api
 */
void rtcSetTimeUnixSec(RTCDriver *rtcp, time_t tv_sec) {
  RTCTime timespec = {0,0,FALSE,0};
  struct tm *timp;

  timp = localtime(&tv_sec);
  stm32_rtc_tm2bcd(timp, &timespec);
  rtcSetTime(rtcp, &timespec);
}

/**
 * @brief   Gets raw time from RTC and converts it to unix format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in microseconds.
 *
 * @api
 */
uint64_t rtcGetTimeUnixUsec(RTCDriver *rtcp) {
  uint64_t result = 0;

  RTCTime timespec = {0,0,FALSE,0};
  struct tm timp;

  rtcGetTime(rtcp, &timespec);
  stm32_rtc_bcd2tm(&timp, &timespec);

  result = mktime(&timp) * 1000000;

#if STM32_RTC_HAS_SUBSECONDS
  return result + timespec.tv_msec * 1000;
#else
  return result;
#endif
}

#else /* STM32_RTC_IS_CALENDAR */
/**
 * @brief   Gets raw time from RTC and converts it to canonicalized format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @param[out] timp     pointer to a @p tm structure as defined in time.h
 *
 * @api
 */
void rtcGetTimeTm(RTCDriver *rtcp, struct tm *timp) {
  RTCTime timespec = {0,0,FALSE,0};

  rtcGetTime(rtcp, &timespec);
  if (timp != NULL) /* this comparison needed to avoid compiler warning */
    timp = localtime((time_t *)&(timespec.tv_sec));
}

/**
 * @brief   Sets RTC time.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @param[out] timp     pointer to a @p tm structure as defined in time.h
 *
 * @api
 */
void rtcSetTimeTm(RTCDriver *rtcp, struct tm *timp) {
  RTCTime timespec = {0,0,FALSE,0};

  timespec.tv_sec = mktime(timp);
  timespec.tv_msec = 0;
  rtcSetTime(rtcp, &timespec);
}

/**
 * @brief   Gets raw time from RTC and converts it to unix format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in seconds.
 *
 * @api
 */
time_t rtcGetTimeUnixSec(RTCDriver *rtcp) {
  RTCTime timespec = {0,0,FALSE,0};

  rtcGetTime(rtcp, &timespec);
  return timespec.tv_sec;
}

/**
 * @brief   Sets RTC time.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in seconds.
 *
 * @api
 */
void rtcSetTimeUnixSec(RTCDriver *rtcp, time_t tv_sec) {
  RTCTime timespec = {0,0,FALSE,0};

  timespec.tv_sec = tv_sec;
  timespec.tv_msec = 0;
  rtcSetTime(rtcp, &timespec);
}

/**
 * @brief   Gets raw time from RTC and converts it to unix format.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              Unix time value in microseconds.
 *
 * @api
 */
uint64_t rtcGetTimeUnixUsec(RTCDriver *rtcp) {
  uint64_t result = 0;
  RTCTime timespec = {0,0,FALSE,0};

  rtcGetTime(rtcp, &timespec);
  result = timespec.tv_sec * 1000000;

#if STM32_RTC_HAS_SUBSECONDS
  return result + timespec.tv_msec * 1000;
#else
  return result;
#endif
}
#endif /* STM32_RTC_IS_CALENDAR */
#endif /* (defined(STM32F4XX) || defined(STM32F2XX) || defined(STM32L1XX) || defined(STM32F1XX)) */

/**
 * @brief   Get current time in format suitable for usage in FatFS.
 *
 * @param[in] rtcp      pointer to RTC driver structure
 * @return              FAT time value.
 *
 * @api
 */
uint32_t rtcGetTimeFat(RTCDriver *rtcp) {
  uint32_t fattime = 0;
  struct tm *timp = NULL;

  rtcGetTimeTm(rtcp, timp);

  fattime |= (timp->tm_sec / 2);
  fattime |= (timp->tm_min) << 5;
  fattime |= (timp->tm_hour) << 11;
  fattime |= (timp->tm_mday) << 16;
  fattime |= (timp->tm_mon + 1) << 21;
  fattime |= (timp->tm_year - 80) << 25;
  return fattime;
}
