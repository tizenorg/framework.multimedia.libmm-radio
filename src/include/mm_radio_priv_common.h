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

#ifndef __MM_Radio_INTERNAL_COMMON_H__
#define __MM_Radio_INTERNAL_COMMON_H__

/*===========================================================================================
  INCLUDE FILES
========================================================================================== */
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <iniparser.h>

#include <mm_types.h>
#include <mm_message.h>
#include <glib.h>

#include "mm_radio_asm.h"
#include "mm_radio.h"
#include "mm_radio_utils.h"


#include <gst/gst.h>
#include <gst/gstbuffer.h>

/*compile #include <avsys-audio.h> */

#ifdef __cplusplus
extern "C" {
#endif

#define MM_RADIO_TRUE 1
#define MM_RADIO_FALSE 0
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
	MMRADIO_COMMAND_VOLUME,
	MMRADIO_COMMAND_MUTE,
	MMRADIO_COMMAND_UNMUTE,
	MMRADIO_COMMAND_SEEK,
	MMRADIO_COMMAND_SET_REGION,
	MMRADIO_COMMAND_GET_REGION,
	MMRADIO_COMMAND_TUNE,
	MMRADIO_COMMAND_RDS_START,
	MMRADIO_COMMAND_RDS_STOP,
	MMRADIO_COMMAND_NUM
}MMRadioCommand;


#define MMFW_RADIO_TUNING_DEFUALT_FILE						"/usr/etc/mmfw_fmradio.ini"
#define MMFW_RADIO_TUNING_TEMP_FILE							"/opt/usr/media/.mmfw_fmradio.ini"
#define MMFW_RADIO_TUNING_ENABLE							"tuning:enable"

#define MMFW_RADIO_TUNING_SNR_THR							"fmradio:mmfw_fmradio_snr_threshold"
#define MMFW_RADIO_TUNING_RSSI_THR							"fmradio:mmfw_fmradio_rssi_threshold"
#define MMFW_RADIO_TUNING_CNT_THR							"fmradio:mmfw_fmradio_cnt_threshold"

/*Soft Mute*/
#define MMFW_RADIO_TUNING_SOFT_MUTE_START_SNR				"fmradio:mmfw_fmradio_softmute_start_snr"
#define MMFW_RADIO_TUNING_SOFT_MUTE_STOP_SNR				"fmradio:mmfw_fmradio_softmute_stop_snr"
#define MMFW_RADIO_TUNING_SOFT_MUTE_START_RSSI			"fmradio:mmfw_fmradio_softmute_start_rssi"
#define MMFW_RADIO_TUNING_SOFT_MUTE_STOP_RSSI				"fmradio:mmfw_fmradio_softmute_stop_rssi"
#define MMFW_RADIO_TUNING_SOFT_MUTE_START_MUTE			"fmradio:mmfw_fmradio_softmute_start_mute"
#define MMFW_RADIO_TUNING_SOFT_MUTE_STOP_ATTEN			"fmradio:mmfw_fmradio_softmute_stop_atten"
#define MMFW_RADIO_TUNING_SOFT_MUTE_MUTE_RATE				"fmradio:mmfw_fmradio_softmute_mute_rate"
#define MMFW_RADIO_TUNING_SOFT_MUTE_SNR40					"fmradio:mmfw_fmradio_softmute_snr40"

/* sprd sc2331 */
#define MMFW_RADIO_TUNING_FREQUENCY_OFFSET					"fmradio:mmfw_fmradio_frequency_offset"
#define MMFW_RADIO_TUNING_NOISE_POWER					"fmradio:mmfw_fmradio_noise_power"
#define MMFW_RADIO_TUNING_PILOT_POWER					"fmradio:mmfw_fmradio_pilot_power"
#define MMFW_RADIO_TUNING_SOFTMUTE_ENABLE				"fmradio:mmfw_fmradio_softmute_enable"

#define MMFW_RADIO_TUNING_VOLUME_LEVELS		"fmradio:volume_levels"
#define MMFW_RADIO_TUNING_VOLUME_TABLE			"fmradio:volume_table"
#define MMFW_RADIO_TUNING_VOLUME_STEPS		16

#define RECORDING_VOLUME_LOWER_THRESHOLD 5
#define RECORDING_VOLUME_HEIGHER_THRESHOLD 11

/* max and mix frequency types, KHz */
typedef enum {
	MM_RADIO_FREQ_NONE				= 0,
	/* min band types */
	MM_RADIO_FREQ_MIN_76100_KHZ 		= 76100,
	MM_RADIO_FREQ_MIN_87500_KHZ 		= 87500,
	MM_RADIO_FREQ_MIN_88100_KHZ 		= 88100,
	/* max band types */
	MM_RADIO_FREQ_MAX_89900_KHZ		= 89900,
	MM_RADIO_FREQ_MAX_108000_KHZ	= 108000,
} MMRadioFreqTypes;

/* de-emphasis types  */
typedef enum {
	MM_RADIO_DEEMPHASIS_NONE = 0,
	MM_RADIO_DEEMPHASIS_50_US,
	MM_RADIO_DEEMPHASIS_75_US,
} MMRadioDeemphasis;

/* radio region settings */
typedef struct {
	MMRadioRegionType country;
	MMRadioDeemphasis deemphasis;	/* unit :  us */
	MMRadioFreqTypes band_min;		/* <- freq. range, unit : KHz */
	MMRadioFreqTypes band_max;		/* -> */
	int channel_spacing;				/* TBD */
} MMRadioRegion_t;

typedef enum {
	MMRADIO_EVENT_DESTROY = 0,
	MMRADIO_EVENT_STOP = 1,
	MMRADIO_EVENT_SET_FREQ_ASYNC = 2,
	MMRADIO_EVENT_NUM
} MMRadioEvent;

/*---------------------------------------------------------------------------
    GLOBAL DATA TYPE DEFINITIONS:
---------------------------------------------------------------------------*/
/*#define USE_GST_PIPELINE */

#ifdef USE_GST_PIPELINE
typedef struct _mm_radio_gstreamer_s {
	GMainLoop *loop;
	GstElement *pipeline;
	GstElement *avsysaudiosrc;
	GstElement *queue2;
	GstElement *avsysaudiosink;
	GstBuffer *output_buffer;
} mm_radio_gstreamer_s;
#endif

typedef struct _mm_radio_event_s {
	MMRadioEvent event_type;
	int event_data;
} _mm_radio_event_s;

typedef struct _mm_radio_tuning_s {
	/* to check default or temp value*/
	int enable;

	int cnt_th;
	int rssi_th;
	int snr_th;

	/*soft mute*/
	int start_snr;
	int stop_snr;
	int start_rssi;
	int stop_rssi;
	int start_mute;
	int stop_atten;
	int mute_rate;
	int snr40;

	/*recording volume*/
	int recording_volume_lower_thres;
	int recording_volume_heigher_thres;

	/* sprd */
	int freq_offset;
	int noise_power;
	int pilot_power;
	int softmute_enable;

} mm_radio_tuning_t;

typedef struct {
	/* radio state */
	int current_state;
	int old_state;
	int pending_state;

	/*commad to be performed*/
	int cmd;

	/* command lock */
	pthread_mutex_t cmd_lock;

	/* volume set lock */
	pthread_mutex_t volume_lock;


	/* radio attributes */
	MMHandleType *attrs;

	/* message callback */
	MMMessageCallback msg_cb;
	void *msg_cb_param;

	/* radio device fd */
	int radio_fd;

	/* scan */
	pthread_t scan_thread;
	int stop_scan;

	/* seek */
	pthread_t seek_thread;
	MMRadioSeekDirectionType seek_direction;
	int prev_seek_freq;
	/*These parameters are inserted for better seek handling*/
	int is_seeking;
	int seek_unmute;
	bool seek_cancel;

	/* ASM */
	MMRadioASM sm;

	/*frequency*/
	int freq;

	/*mute*/
	int is_muted;

	/*Handle to vender specific attributes*/
	MMRadioVenderHandle vender_specific_handle;

	/*radio tuning structure*/
	mm_radio_tuning_t radio_tuning;

	/*to keep state changes atomic*/
	pthread_mutex_t state_mutex;
#ifdef USE_GST_PIPELINE
	mm_radio_gstreamer_s *pGstreamer_s;
#endif

	/* region settings */
	MMRadioRegion_t region_setting;

	/*frequency changes*/
	pthread_t event_thread;
	GAsyncQueue *event_queue;
} mm_radio_t;

/*===========================================================================================
  GLOBAL FUNCTION PROTOTYPES
========================================================================================== */
int _mmradio_create_radio(mm_radio_t *radio);
int _mmradio_destroy(mm_radio_t *radio);
int _mmradio_realize(mm_radio_t *radio);
int _mmradio_unrealize(mm_radio_t *radio);
int _mmradio_set_message_callback(mm_radio_t *radio, MMMessageCallback callback, void *user_param);
int _mmradio_get_state(mm_radio_t *radio, int *pState);
int _mmradio_set_frequency(mm_radio_t *radio, int freq);
int _mmradio_get_frequency(mm_radio_t *radio, int *pFreq);
int _mmradio_mute(mm_radio_t *radio);
int _mmradio_unmute(mm_radio_t *radio);
int _mmradio_start(mm_radio_t *radio);
int _mmradio_stop(mm_radio_t *radio);
int _mmradio_seek(mm_radio_t *radio, MMRadioSeekDirectionType direction);
void _mmradio_seek_cancel(mm_radio_t *radio);
int _mmradio_start_scan(mm_radio_t *radio);
int _mmradio_stop_scan(mm_radio_t *radio);
int _mm_radio_get_signal_strength(mm_radio_t *radio, int *value);
int _mmradio_apply_region(mm_radio_t *radio, MMRadioRegionType region, bool update);
int _mmradio_get_region_type(mm_radio_t *radio, MMRadioRegionType *type);
int _mmradio_get_region_frequency_range(mm_radio_t *radio, uint *min_freq, uint *max_freq);
#if 0 /* compile */
int _mm_radio_enable_rds(mm_radio_t *radio);
int _mm_radio_disable_rds(mm_radio_t *radio);
int _mm_radio_is_rds_enabled(mm_radio_t *radio, bool *rds_enbled);

int _mm_radio_enable_af(mm_radio_t *radio);
int _mm_radio_disable_af(mm_radio_t *radio);
int _mm_radio_is_af_enabled(mm_radio_t *radio, bool *rds_enbled);
#endif
int _mmradio_prepare_device(mm_radio_t *radio);
void _mmradio_unprepare_device(mm_radio_t *radio);

#ifdef USE_GST_PIPELINE
int _mmradio_realize_pipeline(mm_radio_t *radio);
int _mmradio_start_pipeline(mm_radio_t *radio);
int _mmradio_stop_pipeline(mm_radio_t *radio);
int _mmradio_destroy_pipeline(mm_radio_t *radio);
#endif

int _mm_radio_load_volume_table(int **volume_table, int *number_of_elements);
void _mm_radio_write_tuning_parameter(char *tuning_parameter, int tuning_value);
int _mm_create_temp_tuning_file(void);

#if 0
int mmradio_set_attrs(mm_radio_t  *radio, MMRadioAttrsType type, MMHandleType attrs);
MMHandleType mmradio_get_attrs(mm_radio_t  *radio, MMRadioAttrsType type);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __MM_Radio_INTERNAL_COMMON_H__ */
