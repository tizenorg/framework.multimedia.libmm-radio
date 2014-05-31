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
 
#ifndef __MM_Radio_INTERNAL_H__
#define	__MM_Radio_INTERNAL_H__

/*===========================================================================================
  INCLUDE FILES
========================================================================================== */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>

#include <mm_types.h>
#include <mm_message.h>

#include "mm_radio_asm.h"
#include "mm_radio.h"
#include "mm_radio_utils.h"
#include <linux/videodev2.h>

#include <gst/gst.h>
#include <gst/gstbuffer.h>
 
#ifdef __cplusplus
	extern "C" {
#endif

/*===========================================================================================
  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE
========================================================================================== */

/*---------------------------------------------------------------------------
    GLOBAL #defines:
---------------------------------------------------------------------------*/
#define SAMPLEDELAY     	15000

/* si470x dependent define */
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

/*---------------------------------------------------------------------------
    GLOBAL CONSTANT DEFINITIONS:
---------------------------------------------------------------------------*/
typedef enum
{
	MMRADIO_COMMAND_CREATE = 0,
	MMRADIO_COMMAND_DESTROY,
	MMRADIO_COMMAND_REALIZE,
	MMRADIO_COMMAND_UNREALIZE,
	MMRADIO_COMMAND_START,
	MMRADIO_COMMAND_STOP,
	MMRADIO_COMMAND_START_SCAN,
	MMRADIO_COMMAND_STOP_SCAN,
	MMRADIO_COMMAND_SET_FREQ,
	MMRADIO_COMMAND_GET_FREQ,
	MMRADIO_COMMAND_MUTE,
	MMRADIO_COMMAND_UNMUTE,
	MMRADIO_COMMAND_SEEK,
	MMRADIO_COMMAND_SET_REGION,
	MMRADIO_COMMAND_GET_REGION,
	MMRADIO_COMMAND_NUM
} MMRadioCommand;

/* max and mix frequency types, KHz */
typedef enum
{
	MM_RADIO_FREQ_NONE				= 0,
	/* min band types */
	MM_RADIO_FREQ_MIN_76100_KHZ 		= 76100,
	MM_RADIO_FREQ_MIN_87500_KHZ 		= 87500,
	MM_RADIO_FREQ_MIN_88100_KHZ 		= 88100,
	/* max band types */
	MM_RADIO_FREQ_MAX_89900_KHZ		= 89900,
	MM_RADIO_FREQ_MAX_108000_KHZ	= 108000,
}MMRadioFreqTypes;

/* de-emphasis types  */
typedef enum
{
	MM_RADIO_DEEMPHASIS_NONE = 0,
	MM_RADIO_DEEMPHASIS_50_US,
	MM_RADIO_DEEMPHASIS_75_US,
}MMRadioDeemphasis;

/* radio region settings */
typedef struct
{
	MMRadioRegionType country;
	MMRadioDeemphasis deemphasis;	// unit :  us
	MMRadioFreqTypes band_min;		// <- freq. range, unit : KHz
	MMRadioFreqTypes band_max;		// ->
	//int channel_spacing;				// TBD
}MMRadioRegion_t;

/*---------------------------------------------------------------------------
    GLOBAL DATA TYPE DEFINITIONS:
---------------------------------------------------------------------------*/
#define USE_GST_PIPELINE

#ifdef USE_GST_PIPELINE
typedef struct _mm_radio_gstreamer_s
{
	GMainLoop *loop;
	GstElement *pipeline;
	GstElement *avsysaudiosrc;
	GstElement *queue2;
	GstElement *avsysaudiosink;
	GstBuffer *output_buffer;
} mm_radio_gstreamer_s;
#endif

typedef struct {
	/* radio state */
	int current_state;
	int old_state;
	int pending_state;

	int cmd;

	/* command lock */
	pthread_mutex_t cmd_lock;

	/* radio attributes */
	MMHandleType* attrs;

	/* message callback */
	MMMessageCallback msg_cb;
	void* msg_cb_param;

	/* radio device fd */
	int radio_fd;

	/* device control */
	struct v4l2_capability vc;
	struct v4l2_tuner vt;
	struct v4l2_control vctrl;
	struct v4l2_frequency vf;

	/* hw debug */
	struct v4l2_dbg_register reg;

	/* scan */
	pthread_t	scan_thread;
	bool	stop_scan;

	/* seek */
	pthread_t seek_thread;
	int prev_seek_freq;
	MMRadioSeekDirectionType seek_direction;

	/* ASM */
	MMRadioASM sm;

	int freq;
#ifdef USE_GST_PIPELINE
	mm_radio_gstreamer_s* pGstreamer_s;
#endif

	/* region settings */
	MMRadioRegion_t	region_setting;
} mm_radio_t;

/*===========================================================================================
  GLOBAL FUNCTION PROTOTYPES
========================================================================================== */
int _mmradio_create_radio(mm_radio_t* radio);
int _mmradio_destroy(mm_radio_t* radio);
int _mmradio_realize(mm_radio_t* radio);
int _mmradio_unrealize(mm_radio_t* radio);
int _mmradio_set_message_callback(mm_radio_t* radio, MMMessageCallback callback, void *user_param);
int _mmradio_get_state(mm_radio_t* radio, int* pState);
int _mmradio_set_frequency(mm_radio_t* radio, int freq);
int _mmradio_get_frequency(mm_radio_t* radio, int* pFreq);
int _mmradio_mute(mm_radio_t* radio);
int _mmradio_unmute(mm_radio_t* radio);
int _mmradio_start(mm_radio_t* radio);
int _mmradio_stop(mm_radio_t* radio);
int _mmradio_seek(mm_radio_t* radio, MMRadioSeekDirectionType direction);
int _mmradio_start_scan(mm_radio_t* radio);
int _mmradio_stop_scan(mm_radio_t* radio);
#ifdef USE_GST_PIPELINE
int _mmradio_realize_pipeline( mm_radio_t* radio);
int _mmradio_start_pipeline(mm_radio_t* radio);
int _mmradio_stop_pipeline( mm_radio_t* radio);
int _mmradio_destroy_pipeline(mm_radio_t* radio);
#endif
int _mmradio_apply_region(mm_radio_t*radio, MMRadioRegionType region, bool update);
int _mmradio_get_region_type(mm_radio_t*radio, MMRadioRegionType *type);
int _mmradio_get_region_frequency_range(mm_radio_t* radio, uint *min_freq, uint *max_freq);
#if 0
int mmradio_set_attrs(mm_radio_t*  radio, MMRadioAttrsType type, MMHandleType attrs);
MMHandleType mmradio_get_attrs(mm_radio_t*  radio, MMRadioAttrsType type);
#endif

#ifdef __cplusplus
	}
#endif

#endif	/* __MM_Radio_INTERNAL_H__ */
