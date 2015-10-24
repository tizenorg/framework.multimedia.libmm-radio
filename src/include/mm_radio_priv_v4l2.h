/*
 * libmm-radio
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __MM_Radio_INTERNAL_V4L2_H__
#define __MM_Radio_INTERNAL_V4L2_H__

/*===========================================================================================
  INCLUDE FILES
========================================================================================== */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================================
  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE
========================================================================================== */

/*---------------------------------------------------------------------------
    GLOBAL #defines:
---------------------------------------------------------------------------*/
#define MM_RADIO_NAME "radio"

#define SAMPLEDELAY     	15000

#if 0 /* si470x dependent define */
#define SYSCONFIG1					4		/* System Configuration 1 */
#define SYSCONFIG1_RDS				0x1000	/* bits 12..12: RDS Enable */
#define SYSCONFIG1_RDS_OFFSET		12		/* bits 12..12: RDS Enable Offset */

#define SYSCONFIG2					5		/* System Configuration 2 */
#define SYSCONFIG2_SEEKTH			0xff00	/* bits 15..08: RSSI Seek Threshold */
#define SYSCONFIG2_SEEKTH_OFFSET	8		/* bits 15..08: RSSI Seek Threshold Offset */

#define SYSCONFIG3					6		/* System Configuration 3 */
#define SYSCONFIG3_SKSNR			0x00f0	/* bits 07..04: Seek SNR Threshold */
#define SYSCONFIG3_SKCNT			0x000f	/* bits 03..00: Seek FM Impulse Detection Threshold */
#define SYSCONFIG3_SKSNR_OFFSET	4		/* bits 07..04: Seek SNR Threshold Offset */
#define SYSCONFIG3_SKCNT_OFFSET	0		/* bits 03..00: Seek FM Impulse Detection Threshold Offset */

#define DEFAULT_CHIP_MODEL			"radio-si470x"
#endif

/* COMMON */

#define FMRX_DEVICE_STATUS "/sys/devices/virtual/video4linux/radio0/fmrx_status"
#define FMRX_RSSI_LEVEL_THRESHOLD "/sys/devices/virtual/video4linux/radio0/fmrx_rssi_lvl"

/* for BRCM */
#define FMRX_CURR_SNR "/sys/devices/virtual/video4linux/radio0/fmrx_curr_snr"
#define FMRX_CNT_THRESHOLD "/sys/devices/virtual/video4linux/radio0/fmrx_cos_th"
#define FMRX_SNR_LEVEL_THRESHOLD "/sys/devices/virtual/video4linux/radio0/fmrx_snr_lvl"

/* for BRCM softmute*/
#define FMRX_SOFT_MUTE_START_SNR "/sys/devices/virtual/video4linux/radio0/fmrx_start_snr"
#define FMRX_SOFT_MUTE_STOP_SNR "/sys/devices/virtual/video4linux/radio0/fmrx_stop_snr"
#define FMRX_SOFT_MUTE_START_RSSI "/sys/devices/virtual/video4linux/radio0/fmrx_start_rssi"
#define FMRX_SOFT_MUTE_STOP_RSSI "/sys/devices/virtual/video4linux/radio0/fmrx_stop_rssi"
#define FMRX_SOFT_MUTE_START_MUTE "/sys/devices/virtual/video4linux/radio0/fmrx_start_mute"
#define FMRX_SOFT_MUTE_STOP_ATTEN "/sys/devices/virtual/video4linux/radio0/fmrx_stop_atten"
#define FMRX_SOFT_MUTE_MUTE_RATE "/sys/devices/virtual/video4linux/radio0/fmrx_mute_rate"
#define FMRX_SOFT_MUTE_SNR_40 "/sys/devices/virtual/video4linux/radio0/fmrx_snr40"
#define FMRX_SOFT_MUTE_UPDATE "/sys/devices/virtual/video4linux/radio0/fmrx_set_blndmute"
#define FMRX_PROPERTY_SEARCH_ABORT "/sys/devices/virtual/video4linux/radio0/fmrx_search_abort"

/* for SPRD */
#define FMRX_TUNING_FREQ_OFFSET "/sys/class/video4linux/radio0/fmrx_freq_offset"
#define FMRX_TUNING_NOISE_POWER "/sys/class/video4linux/radio0/fmrx_noise_power"
#define FMRX_TUNING_PILOT_POWER "/sys/class/video4linux/radio0/fmrx_pilot_power"
#define FMRX_TUNING_ENABLE_DISABLE_SOFTMUTE "/sys/class/video4linux/radio0/fmrx_start_mute"

#define DEV_OPEN_RETRY_COUNT 3

typedef struct {
	pthread_t fade_volume_thread;
	int initial_volume;
	int final_volume;
	int time_in_ms;
	int number_of_steps;
	int fade_finished;
} mm_radio_volume_fade_args;

typedef struct {
	/* device control */
	struct v4l2_capability vc;
	struct v4l2_tuner vt;
	struct v4l2_control vctrl;
	struct v4l2_frequency vf;

	/* hw debug */
	struct v4l2_dbg_register reg;

	/*chipset specific flags*/
	int prev_seek_freq;

	/*seeek cancel atomic*/
	pthread_mutex_t seek_cancel_mutex;

	int device_ready;

	/*fadeup volume*/
	mm_radio_volume_fade_args volume_fade;
} mm_radio_priv_v4l2_t;

#ifdef __cplusplus
}
#endif

#endif
