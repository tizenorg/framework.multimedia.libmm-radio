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

/*===========================================================================================
|																							|
|  INCLUDE FILES																			|
|  																							|
========================================================================================== */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <mm_sound.h>

#include <mm_error.h>
#include <mm_debug.h>
#include <mm_message.h>

#include <dbus/dbus.h>
#include <vconf.h>


#include "mm_radio_priv_common.h"
#include "mm_radio_priv_v4l2.h"

#define MM_RADIO_EAR_PHONE_POLICY
#define ENABLE_FM_TUNING

//#ifdef USE_FM_RADIO_V4L2_VOLUME /* in sprd sc2331, volume set is handled by mixer to separate rec volume */

/*===========================================================================================
note: This File is specific to fm devices based on v4l2
============================================================================================*/

/*===========================================================================================
  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE
========================================================================================== */
/*---------------------------------------------------------------------------
    GLOBAL CONSTANT DEFINITIONS:
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    IMPORTED VARIABLE DECLARATIONS:
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    IMPORTED FUNCTION DECLARATIONS:
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    LOCAL #defines:
---------------------------------------------------------------------------*/
#define DEFAULT_DEVICE				"/dev/radio0"
#define TUNER_INDEX				0

#define DEFAULT_FREQ				107700
#define SPRD_DEFAULT_FREQ			99900

#define FREQ_FRAC				16
#define RADIO_FREQ_FORMAT_SET(x_freq)		((x_freq) * FREQ_FRAC)
#define RADIO_FREQ_FORMAT_GET(x_freq)		((x_freq) / FREQ_FRAC)
#define DEFAULT_WRAP_AROUND 			1 /*If non-zero, wrap around when at the end of the frequency range, else stop seeking */

#define RADIO_DEFAULT_REGION			MM_RADIO_REGION_GROUP_EUROPE

#ifdef USE_FM_RADIO_V4L2_VOLUME
#define FM_VOLUME_BASE 30
#define FM_VOLUME_MAX 16
static int fm_volume_tbl[FM_VOLUME_MAX] = {
	0, 2, 4, 8, 15, 30, 45, 70, 80, 100, 125, 150, 180, 210, 230, 255
};
#endif

#define BT_ENABLE "/var/run/bluetooth/bt"
#define DEVICE_CLOSE_RETRY_CNT 10
#define HCI_DISABLE_RETRY_CNT 100

/*---------------------------------------------------------------------------
    LOCAL CONSTANT DEFINITIONS:
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    LOCAL DATA TYPE DEFINITIONS:
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
    GLOBAL VARIABLE DEFINITIONS:
---------------------------------------------------------------------------*/
extern int errno;

/*---------------------------------------------------------------------------
    LOCAL VARIABLE DEFINITIONS:
---------------------------------------------------------------------------*/
/* radio region configuration table */
static const MMRadioRegion_t region_table[] = {
	{	/* Notrh America, South America, South Korea, Taiwan, Australia */
		MM_RADIO_REGION_GROUP_USA,	/* region type */
		MM_RADIO_DEEMPHASIS_75_US,	/* de-emphasis */
		MM_RADIO_FREQ_MIN_87500_KHZ, 	/* min freq. */
		MM_RADIO_FREQ_MAX_108000_KHZ,	/* max freq. */
		50,
	},
	{	/* China, Europe, Africa, Middle East, Hong Kong, India, Indonesia, Russia, Singapore */
		MM_RADIO_REGION_GROUP_EUROPE,
		MM_RADIO_DEEMPHASIS_50_US,
		MM_RADIO_FREQ_MIN_87500_KHZ,
		MM_RADIO_FREQ_MAX_108000_KHZ,
		50,
	},
	{
		MM_RADIO_REGION_GROUP_JAPAN,
		MM_RADIO_DEEMPHASIS_50_US,
		MM_RADIO_FREQ_MIN_76100_KHZ,
		MM_RADIO_FREQ_MAX_89900_KHZ,
		50,
	},
};
/*---------------------------------------------------------------------------
    LOCAL FUNCTION PROTOTYPES:
---------------------------------------------------------------------------*/
static bool __mmradio_post_message(mm_radio_t *radio, enum MMMessageType msgtype, MMMessageParamType *param);
static int __mmradio_check_state(mm_radio_t *radio, MMRadioCommand command);
static int __mmradio_get_state(mm_radio_t *radio);
static bool __mmradio_set_state(mm_radio_t *radio, int new_state);
static void __mmradio_seek_thread(mm_radio_t *radio);
static void __mmradio_scan_thread(mm_radio_t *radio);
#ifdef FEATURE_ASM_SUPPORT
static ASM_cb_result_t __mmradio_asm_callback(int handle, ASM_event_sources_t sound_event, ASM_sound_commands_t command, unsigned int sound_status, void *cb_data);
#else
void _mmradio_sound_focus_cb(int id, mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state, const char *reason_for_change, const char *additional_info, void *user_data);
void _mmradio_sound_focus_watch_cb(int id, mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state, const char *reason_for_change, const char *additional_info, void *user_data);
#endif
static bool __is_tunable_frequency(mm_radio_t *radio, int freq);
static int __mmradio_set_band_range(mm_radio_t *radio);
#ifdef USE_FM_RADIO_V4L2_VOLUME
static void __mmradio_volume_change_cb(volume_type_t type, unsigned int volume, void *user_data);
static int __mmradio_set_volume(mm_radio_t *radio, unsigned int value);
static int __mmradio_get_volume(mm_radio_t *radio, unsigned int *value);
static void __mmradio_active_device_changed_cb (mm_sound_device_in device_in, mm_sound_device_out device_out, void *user_data);
#endif
static int __mmradio_enable_bluetooth(mm_radio_t  *radio, int enable);
static int __mmradio_wait_bluetooth_ready(mm_radio_t  *radio, int wait_active, int retry_count);

void __mm_radio_init_tuning_params(mm_radio_t *radio);
void __mm_radio_apply_tuning_params(mm_radio_t *radio);

static int __mmradio_init_v4l2_device(mm_radio_t *radio);
static void __mmradio_deinit_v4l2_device(mm_radio_t *radio);
static int __mm_radio_rds_callback(int type, void *data, void *user_data);
int _mmradio_pause(mm_radio_t *radio);
#ifdef USE_FM_RADIO_V4L2_VOLUME
static void _mmradio_fadevolume_cancel(mm_radio_t *radio);
static int _mm_radio_recording_volume_change_policy(mm_radio_t *radio, int present_volume);
#endif
static void __mmradio_event_thread(mm_radio_t *radio);

/*===========================================================================
  FUNCTION DEFINITIONS
========================================================================== */
/* --------------------------------------------------------------------------
 * Name   : _mmradio_apply_region()
 * Desc   : update radio region information and set values to device
 * Param  :
 *	    [in] radio : radio handle
 * 	    [in] region : region type
 *          [in] update : update region values or not
 * Return : zero on success, or negative value with error code
 *---------------------------------------------------------------------------*/
int _mmradio_apply_region(mm_radio_t *radio, MMRadioRegionType region, bool update)
{
	int count = 0;
	int index = 0;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_SET_REGION);

	/* if needed, radio region must be updated.
	  * Otherwise, just applying settings to device without it.
	  */
	if (update) {
		count = ARRAY_SIZE(region_table);

		/*TODO: if auto is supported...get the region info. here */

		/* update radio region settings */
		for (index = 0; index < count; index++) {
			/* find the region from pre-defined table*/
			if (region_table[index].country == region) {
				radio->region_setting.country = region_table[index].country;
				radio->region_setting.deemphasis = region_table[index].deemphasis;
				radio->region_setting.band_min = region_table[index].band_min;
				radio->region_setting.band_max = region_table[index].band_max;
				radio->region_setting.channel_spacing = region_table[index].channel_spacing;
			}
		}
	}
	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmradio_prepare_device(mm_radio_t *radio)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	if (!priv_sprd_v4l2->device_ready) {
		/*Only check blue-tooth - As this is not a reference count we need not care about the number of calls to enable */
		if (MM_ERROR_NONE != __mmradio_enable_bluetooth(radio, MM_RADIO_TRUE)) {
			MMRADIO_LOG_ERROR("__mmradio_enable_bluetooth is failed\n");
			return MM_ERROR_RADIO_DEVICE_NOT_OPENED;
		}

		if (MM_ERROR_NONE != __mmradio_wait_bluetooth_ready(radio, MM_RADIO_TRUE, 50)) {
			MMRADIO_LOG_ERROR("__mmradio_wait_bluetooth_ready is failed\n");
			return MM_ERROR_RADIO_DEVICE_NOT_OPENED;
		}
	} else {
		MMRADIO_LOG_INFO("HCI attach is already started");
	}
	priv_sprd_v4l2->device_ready = MM_RADIO_TRUE;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

void _mmradio_unprepare_device(mm_radio_t *radio)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	/*This is done locked here becuase pause and stop doesn't contain unprepare and is handeled seperatly*/
	pthread_mutex_lock(&radio->state_mutex);

	if (radio->radio_fd < 0) {
		if (priv_sprd_v4l2->device_ready) {
			MMRADIO_LOG_INFO("try to __mmradio_enable_bluetooth (disable)\n");
			if (MM_ERROR_NONE != __mmradio_enable_bluetooth(radio, FALSE)) {
				MMRADIO_LOG_ERROR("__mmradio_enable_bluetooth (disable) is failed\n");
			} else {
				/* FIXME: if there is /var/run/bluetooth/bt, BT is enabled. */
				int bt_disabled = 0;
				int try_cnt = 0;
bt_check_again:
				bt_disabled = access(BT_ENABLE, F_OK);
				if (bt_disabled == 0) {
					MMRADIO_LOG_INFO("BT is enabled. we will not wait hci disable.");
					/* FIXME: we should add some logic for waiting FM close itself later */
				} else {
					int ret = MM_ERROR_NONE;
					/* fopen BT failed. it means that we should wait hci attach disabled completely */
					MMRADIO_LOG_DEBUG("BT disabled. (%s:%d). Try to disable hci attach", strerror(errno), errno);
					ret = __mmradio_wait_bluetooth_ready(radio, MM_RADIO_FALSE, 1);
					if (ret == MM_ERROR_RADIO_INVALID_STATE) {
						try_cnt++;
						if (try_cnt <= HCI_DISABLE_RETRY_CNT) {
							MMRADIO_LOG_INFO("still hci attach is Active. Try again (%d/%d)", try_cnt, HCI_DISABLE_RETRY_CNT);
							goto bt_check_again;
						}
					}
				}
				usleep(200 * 1000); /* FIXME: sleep 200 ms for waiting hci disable in broadcom */
				MMRADIO_LOG_INFO("__mmradio_enable_bluetooth (disable) is success\n");
			}
			priv_sprd_v4l2->device_ready = MM_RADIO_FALSE;
		} else {
			MMRADIO_LOG_INFO("HCI attach is already stopped");
		}
	} else {
		MMRADIO_LOG_WARNING("radio_fd(%d) still opened. can not disable bluetooth now", radio->radio_fd);
	}

	pthread_mutex_unlock(&radio->state_mutex);
}

int _mmradio_create_radio(mm_radio_t *radio)
{
	int ret  = 0;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_CREATE);

	/* set default value */
	radio->radio_fd = -1;
	radio->freq = DEFAULT_FREQ;

	memset(&radio->region_setting, 0, sizeof(MMRadioRegion_t));

	/*allocate memory for chipset specific attributes*/
	radio->vender_specific_handle = (MMRadioVenderHandle) malloc(sizeof(mm_radio_priv_v4l2_t));
	if (!radio->vender_specific_handle) {
		MMRADIO_LOG_CRITICAL("cannot allocate memory for mm_radio_priv_sprd_t\n");
		return MM_ERROR_RADIO_NO_FREE_SPACE;
	}
	memset(radio->vender_specific_handle, 0, sizeof(mm_radio_priv_v4l2_t));

	/* create command lock */
	ret = pthread_mutex_init(&radio->cmd_lock, NULL);
	if (ret) {
		MMRADIO_LOG_ERROR("mutex creation failed\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_NULL);

#ifdef FEATURE_ASM_SUPPORT
	/* register to ASM */
	ret = mmradio_asm_register(&radio->sm, __mmradio_asm_callback, (void *)radio);
#else
	ret = mmradio_audio_focus_register(&radio->sm, _mmradio_sound_focus_cb, (void *)radio);
#endif
	if (ret) {
		/* NOTE : we are dealing it as an error since we cannot expect it's behavior */
		MMRADIO_LOG_ERROR("failed to register asm server\n");
		return MM_ERROR_RADIO_INTERNAL;
	}
	radio->event_queue = g_async_queue_new();
	if (!radio->event_queue) {
		MMRADIO_LOG_ERROR("failed to get g_async_queue_new \n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	ret = pthread_create(&radio->event_thread, NULL, (void *)__mmradio_event_thread, (void *)radio);
	if (ret) {
		/* NOTE : we are dealing it as an error since we cannot expect it's behavior */
		MMRADIO_LOG_ERROR("failed to register asm server\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int __mmradio_init_v4l2_device(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	int try_count = DEV_OPEN_RETRY_COUNT;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);
	/* open radio device */
	if (radio->radio_fd < 0) {
		MMRadioRegionType region = MM_RADIO_REGION_GROUP_NONE;
		bool update = MM_RADIO_FALSE;
try_again:
		/* open device */
		radio->radio_fd = open(DEFAULT_DEVICE, O_RDONLY | O_CLOEXEC);
		if (radio->radio_fd < 0) {
			MMRADIO_LOG_ERROR("failed to open radio device[%s] because of %s(%d)\n",
			                  DEFAULT_DEVICE, strerror(errno), errno);
			/* check error */
			switch (errno) {
				case ENOENT:
					ret = MM_ERROR_RADIO_DEVICE_NOT_FOUND;
					goto error;
				case EACCES:
					ret = MM_ERROR_RADIO_PERMISSION_DENIED;
					goto error;
				case EBUSY:
					ret = MM_ERROR_RADIO_DEVICE_NOT_OPENED;
					goto error;
				case EAGAIN:
					if (try_count > 0) {
						try_count--;
						MMRADIO_LOG_ERROR("Kernel asked me to try again!!! lets try %d more time(s)", try_count);
						/*we make device as not ready because if it was we would not get here*/
						priv_sprd_v4l2->device_ready = MM_RADIO_FALSE;
						ret = _mmradio_prepare_device(radio);
						if (ret) {
							MMRADIO_LOG_ERROR("Coundn't prepare the device - 0x%x\n", ret);
							goto error;
						}
						goto try_again;
					} else {
						MMRADIO_LOG_ERROR("Out of luck!! we return MM_ERROR_RADIO_DEVICE_NOT_OPENED error");
						ret = MM_ERROR_RADIO_DEVICE_NOT_OPENED;
						goto error;
					}
				default:
					ret = MM_ERROR_RADIO_DEVICE_NOT_OPENED;
					goto error;
			}
		}
		MMRADIO_LOG_DEBUG("radio device fd = %d\n", radio->radio_fd);

		/* query radio device capabilities. */
		if (ioctl(radio->radio_fd, VIDIOC_QUERYCAP, &(priv_sprd_v4l2->vc)) < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_QUERYCAP failed!%s\n", strerror(errno));
			goto error;
		}

		if (!(priv_sprd_v4l2->vc.capabilities & V4L2_CAP_TUNER)) {
			MMRADIO_LOG_ERROR("this system can't support fm-radio!\n");
			goto error;
		}

		/* set tuner audio mode */
		ret = ioctl(radio->radio_fd, VIDIOC_G_TUNER, &(priv_sprd_v4l2->vt));
		if (ret < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_G_TUNER failed with %s", strerror(errno));
			goto error;
		}

		if (!((priv_sprd_v4l2->vt).capability & V4L2_TUNER_CAP_STEREO)) {
			MMRADIO_LOG_ERROR("this system can support mono!\n");
			(priv_sprd_v4l2->vt).audmode = V4L2_TUNER_MODE_MONO;
		} else {
			(priv_sprd_v4l2->vt).audmode = V4L2_TUNER_MODE_STEREO;
		}

		/* set tuner index. Must be 0. */
		(priv_sprd_v4l2->vt).index = TUNER_INDEX;
		ret = ioctl(radio->radio_fd, VIDIOC_S_TUNER, &(priv_sprd_v4l2->vt));
		if (ret < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_S_TUNER failed with %s", strerror(errno));
			goto error;
		}


		MMRADIO_LOG_DEBUG("setting region - country= %d, de-emphasis= %d, band range= %d ~ %d KHz\n",
		                  radio->region_setting.country, radio->region_setting.deemphasis, radio->region_setting.band_min, radio->region_setting.band_max);

		/* set band range to device */
		ret = __mmradio_set_band_range(radio);
		MMRADIO_CHECK_GOTO_IF_FAIL(ret, "set band range", error);
	}

	return ret;
error:
	__mmradio_deinit_v4l2_device(radio);
	return ret;
}

void __mmradio_deinit_v4l2_device(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	/*don't let any other ioctl in other threads*/
	pthread_mutex_lock(&radio->volume_lock);

	/* close radio device here !!!! */
	if (radio->radio_fd >= 0) {
		MMRADIO_LOG_INFO("try to close Device");
		ret = close(radio->radio_fd);
		if (ret < 0) {
			MMRADIO_LOG_ERROR("failed to close radio device[%s] because of %s(%d)\n",
			                  DEFAULT_DEVICE, strerror(errno), errno);
		} else {
			/* FIXME: if there is sys fs, the device is NOT really closed. */

			FILE *close_check_file = 0;
			int try_cnt = 0;
			int status = -1;

close_check_again:
			close_check_file = fopen(FMRX_DEVICE_STATUS, "r");
			if (close_check_file) {
				try_cnt++;
				fscanf(close_check_file, "%d", &status);
				fclose(close_check_file);
				close_check_file = NULL;
				if (status == 1) { /* device is still opened */
					MMRADIO_LOG_INFO("fmrx_status: %d. device is not closed completely. try again (%d/%d)", status, try_cnt, DEVICE_CLOSE_RETRY_CNT);
					if (try_cnt <= DEVICE_CLOSE_RETRY_CNT) {
						usleep(100 * 1000); /* FIXME: sleep 100 ms for waiting status changing by FM Radio drv */
						goto close_check_again;
					} else {
						MMRADIO_LOG_ERROR("timeout: fail to close FM Radio device");
					}
				} else if (status == 0) { /* device is closed */
					MMRADIO_LOG_INFO("Device closed successfully");
				} else { /* error case with wrong value */
					MMRADIO_LOG_ERROR("Wrong fmrx_status : %d", status);
				}
			} else {
				MMRADIO_LOG_ERROR("can not open %s (%s:%d)", FMRX_DEVICE_STATUS, strerror(errno), errno);
			}
		}
		radio->radio_fd = -1;
	}

	/*remove don't let any other ioctl in other threads*/
	pthread_mutex_unlock(&radio->volume_lock);

	MMRADIO_LOG_FLEAVE();
}

int _mmradio_realize(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRadioRegionType region = MM_RADIO_REGION_GROUP_EUROPE;
	bool update = MM_RADIO_TRUE;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_REALIZE);

#ifdef USE_FM_RADIO_V4L2_VOLUME
	/* we will set recording 5-11 */
	radio->radio_tuning.recording_volume_lower_thres = RECORDING_VOLUME_LOWER_THRESHOLD;
	radio->radio_tuning.recording_volume_heigher_thres = RECORDING_VOLUME_HEIGHER_THRESHOLD;
#endif

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	ret = pthread_mutex_init(&radio->volume_lock, NULL);

#ifdef USE_FM_RADIO_V4L2_VOLUME
	/* add volume changed call back */
	MMRADIO_LOG_DEBUG("add mm_sound_add_volume_changed_callback");

	mm_sound_add_volume_changed_callback(__mmradio_volume_change_cb, (void *)radio);
	/* FIXME: for checking device is changed. This can be removed with another solution from sound-server */
	/* add active deviced changed callback */
	MMRADIO_LOG_DEBUG("add mm_sound_add_active_device_changed_callback");
	mm_sound_add_active_device_changed_callback(MM_RADIO_NAME, __mmradio_active_device_changed_cb, (void *)radio);
#endif

	/*we have to load the volume tables in libmmradio only for broadcom??
	*/
	int number_of_steps = 0;
	int index = 0;
	int *table = NULL;

#ifdef USE_FM_RADIO_V4L2_VOLUME
	_mm_radio_load_volume_table(&table, &number_of_steps);
	if (table) {
		MMRADIO_LOG_DEBUG("number of steps -> %d", number_of_steps);
		/*copy from temp structure to main strcture*/
		for (index = 0; index < number_of_steps; index++) {
			fm_volume_tbl[index] = table[index];
		}
		free(table);
		table = NULL;
	}
#endif

	/*TODO: double check*/
	priv_sprd_v4l2->device_ready = MM_RADIO_FALSE;


	/*seek cancel*/
	ret = pthread_mutex_init(&radio->state_mutex, NULL);
	if (ret < 0) {
		MMRADIO_LOG_DEBUG("Mutex creation failed %d", ret);
	}

	/*seek cancel*/
	ret = pthread_mutex_init(&priv_sprd_v4l2->seek_cancel_mutex, NULL);
	if (ret < 0) {
		MMRADIO_LOG_DEBUG("Mutex creation failed %d", ret);
	}

	MMRADIO_LOG_ERROR("_mmradio_realize_pipeline radio->region_setting.country : %d\n", radio->region_setting.country);
	/*Init Region settings*/
	/* check region country type if it's updated or not */
	if (radio->region_setting.country == MM_RADIO_REGION_GROUP_NONE) {
		/* not initialized  yet. set it with default region */
		region = RADIO_DEFAULT_REGION;
		update = MM_RADIO_TRUE;
	} else {/* already initialized by application */
		region = radio->region_setting.country;
	}
	ret = _mmradio_apply_region(radio, region, update);
	MMRADIO_CHECK_RETURN_IF_FAIL(ret, "set apply region");
#ifdef USE_GST_PIPELINE
	ret = _mmradio_realize_pipeline(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("_mmradio_realize_pipeline is failed\n");
		return ret;
	}
#endif
	radio->is_muted = MM_RADIO_FALSE;

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_READY);
	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_unrealize(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_UNREALIZE);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	/*Finish if there are scans*/
	_mmradio_stop_scan(radio);
	/*Stop radio if started*/
	_mmradio_stop(radio);


	/*If applicaiton hasn't called unpreapre let us prepare*/
	_mmradio_unprepare_device(radio);
	MMRADIO_LOG_INFO("device Unprep done");

#ifdef USE_FM_RADIO_V4L2_VOLUME
	mm_sound_remove_volume_changed_callback();
	mm_sound_remove_active_device_changed_callback(MM_RADIO_NAME);
#endif

	pthread_mutex_destroy(&radio->volume_lock);
	pthread_mutex_destroy(&priv_sprd_v4l2->seek_cancel_mutex);
	pthread_mutex_destroy(&radio->state_mutex);

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_NULL);
#ifndef FEATURE_ASM_SUPPORT
	ret = mmradio_set_audio_focus(&radio->sm, _mmradio_sound_focus_watch_cb, FALSE, (void *)radio);
#endif
#ifdef USE_GST_PIPELINE
	ret = _mmradio_destroy_pipeline(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("_mmradio_destroy_pipeline is failed\n");
		return ret;
	}
#endif
	MMRADIO_LOG_FLEAVE();
	return ret;
}

int _mmradio_destroy(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	mm_radio_priv_v4l2_t *priv_v4l2_handle = NULL;

	MMRADIO_LOG_INFO("<ENTER>");

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_DESTROY);

	_mmradio_unrealize(radio);

#ifdef FEATURE_ASM_SUPPORT
	ret = mmradio_asm_deregister(&radio->sm);
#else
	ret = mmradio_audio_focus_deregister(&radio->sm);
#endif
	if (ret) {
		MMRADIO_LOG_ERROR("failed to deregister asm server\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	priv_v4l2_handle = (mm_radio_priv_v4l2_t *)radio->vender_specific_handle;
	if (priv_v4l2_handle) {
		free(priv_v4l2_handle);
		radio->vender_specific_handle = NULL;
	}
	_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);

	if (event == NULL)
		return MM_ERROR_RADIO_INTERNAL;

	event->event_type = MMRADIO_EVENT_DESTROY;

	g_async_queue_push_sorted(radio->event_queue, event, event_comparator, NULL);
	pthread_join(radio->event_thread, NULL);

	/*Clean up frequency related variables*/
	g_async_queue_unref(radio->event_queue);

	MMRADIO_LOG_INFO("<LEAVE>");

	return ret;
}

void __mmradio_event_thread(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	MMMessageParamType param = {0,};
	_mm_radio_event_s *event = NULL;
	int exit_event_thread = 0;

	/*we run a while one loop*/
	while (exit_event_thread == 0) {
		event = (_mm_radio_event_s *)g_async_queue_pop(radio->event_queue);
		if (event == NULL) {
			MMRADIO_LOG_ERROR("poped message is NULL!");
			goto exit;
		}

		/*Its okay to use the command because they are nicely sorted for our needs
		 * also frequency is way above the needs of our commands simple and less complicated*/
		switch (event->event_type) {
			case MMRADIO_EVENT_DESTROY:
				MMRADIO_LOG_INFO("get destroy event. pop all event to finish this thread");
				while ((event = (_mm_radio_event_s *)g_async_queue_try_pop(radio->event_queue))) {
					if (event != NULL) {
						MMRADIO_LOG_DEBUG("drop this msg type: %d", event->event_type);
						g_slice_free(_mm_radio_event_s, event);
					}
				}
				exit_event_thread = 1;
				break;

			case MMRADIO_EVENT_STOP:
				MMRADIO_LOG_INFO("async close device is requested");
				/*we don't clear the frequency here, this might create a problem
				 * setting frequency after stop??*/
				pthread_mutex_lock(&radio->state_mutex);
				__mmradio_deinit_v4l2_device(radio);
				pthread_mutex_unlock(&radio->state_mutex);
				_mmradio_unprepare_device(radio);
				break;

			case MMRADIO_EVENT_SET_FREQ_ASYNC:
				MMRADIO_LOG_INFO("processing frequency: %d", event->event_data);
				/*we try to call mm_radio api here this is done on purpose, to make sure that cmd lock is held*/
				ret = mm_radio_set_frequency(radio, event->event_data);
				if (ret == MM_ERROR_NONE) {
					param.radio_scan.frequency = event->event_data;
				} else {
					param.radio_scan.frequency = ret; /* FIMXE: we need this? */
				}
				MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SET_FREQUENCY, &param);
				break;

			default:
				MMRADIO_LOG_ERROR("wrong event_type: %d", event->event_data);
				break;
		}
	}

exit:
	if (event) {
		g_slice_free(_mm_radio_event_s, event);
	}
	MMRADIO_LOG_INFO("event thread is finished");
}

int _mmradio_set_frequency_async(mm_radio_t *radio, int freq)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_SET_FREQ);

	/* check frequency range */
	if (freq < radio->region_setting.band_min
	    || freq > radio->region_setting.band_max) {
		MMRADIO_LOG_ERROR("out of frequency range - %d\n", freq);
		return MM_ERROR_INVALID_ARGUMENT;
	}

	_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);

	if (event == NULL)
		return MM_ERROR_RADIO_INTERNAL;

	event->event_type = MMRADIO_EVENT_SET_FREQ_ASYNC;
	event->event_data = freq;

	g_async_queue_push(radio->event_queue, event);

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_set_frequency(mm_radio_t *radio, int freq) /* unit should be KHz */
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	int ret = 0;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_SET_FREQ);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	/* check frequency range */
	if (freq < radio->region_setting.band_min
	    || freq > radio->region_setting.band_max) {
		MMRADIO_LOG_ERROR("out of frequency range - %d\n", freq);
		return MM_ERROR_INVALID_ARGUMENT;
	}

	if (radio->radio_fd < 0) {
		radio->freq = freq;
        if (radio->freq == MM_RADIO_FREQ_MIN_87500_KHZ && radio->region_setting.band_min == MM_RADIO_FREQ_MIN_87500_KHZ)
            radio->freq = SPRD_DEFAULT_FREQ;
		MMRADIO_LOG_WARNING("radio device is not opened to set (%d)! retun ok.", freq);
		return MM_ERROR_NONE;
	} else {

		/* set it */
		(priv_sprd_v4l2->vf).tuner = 0;
		(priv_sprd_v4l2->vf).type = V4L2_TUNER_RADIO; /* if we do not set type, we will get EINVAL */
		(priv_sprd_v4l2->vf).frequency = RADIO_FREQ_FORMAT_SET(freq);

		MMRADIO_LOG_DEBUG("Setting %d frequency\n", freq);

		ret = ioctl(radio->radio_fd, VIDIOC_S_FREQUENCY, &(priv_sprd_v4l2->vf)) ;
		if (ret < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_S_FREQUENCY failed with %s", strerror(errno));
			return MM_ERROR_RADIO_INTERNAL;
		}
	}
	/*Only update if the ioctl was successful*/
	radio->freq = freq;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_get_frequency(mm_radio_t *radio, int *pFreq)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_GET_FREQ);

	return_val_if_fail(pFreq, MM_ERROR_INVALID_ARGUMENT);

	/* update freq from handle */
	*pFreq = radio->freq;

	MMRADIO_LOG_DEBUG("Getting %d frequency\n", radio->freq);

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

#ifdef USE_FM_RADIO_V4L2_VOLUME
static void _mmradio_fadevolume_cancel(mm_radio_t *radio)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);
	if (priv_sprd_v4l2->volume_fade.fade_volume_thread) {
		pthread_cancel(priv_sprd_v4l2->volume_fade.fade_volume_thread);
		pthread_join(priv_sprd_v4l2->volume_fade.fade_volume_thread, NULL);
		priv_sprd_v4l2->volume_fade.fade_volume_thread = 0;
	}
}


static void __mmradio_fade_volume_thread(mm_radio_t *radio)
{
	int ret = 0;
	int index = 0;

	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	if (priv_sprd_v4l2->volume_fade.initial_volume < priv_sprd_v4l2->volume_fade.final_volume) {
		/*increasing index*/
		do {
			usleep(100 * 1000);
			priv_sprd_v4l2->volume_fade.initial_volume++;
			ret = __mmradio_set_volume(radio, priv_sprd_v4l2->volume_fade.initial_volume);
			if (ret != 0) {
				/*When this happens usually state is changed*/
				priv_sprd_v4l2->volume_fade.fade_finished = MM_RADIO_TRUE;
				MMRADIO_LOG_ERROR("set volume failed so no more fading for this request");
				return;
			}
		} while (priv_sprd_v4l2->volume_fade.initial_volume != priv_sprd_v4l2->volume_fade.final_volume);
	} else {
		/*decreasing index*/
		do {
			usleep(100 * 1000);
			priv_sprd_v4l2->volume_fade.initial_volume--;
			ret = __mmradio_set_volume(radio, priv_sprd_v4l2->volume_fade.initial_volume);
			if (ret != 0) {
				/*When this happens usually state is changed*/
				priv_sprd_v4l2->volume_fade.fade_finished = MM_RADIO_TRUE;
				MMRADIO_LOG_ERROR("set volume failed so no more fading for this request");
				return;
			}
		} while (priv_sprd_v4l2->volume_fade.initial_volume != priv_sprd_v4l2->volume_fade.final_volume);
	}
	priv_sprd_v4l2->volume_fade.fade_finished = MM_RADIO_TRUE;
}

/*
 * Description: This function fades in and out fm volume,
 * pseudo: open a thread, change the volume in steps in time intervals
 * */
static void _mmradio_fadevolume(mm_radio_t *radio, int initial_volume, int final_volume)
{
	int ret = 0;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	if (initial_volume == final_volume) {
		MMRADIO_LOG_INFO("volume is same no need to fade volume");
		return;
	}

	/*cancel any outstanding request*/
	_mmradio_fadevolume_cancel(radio);

	if (priv_sprd_v4l2->volume_fade.fade_finished) {
		priv_sprd_v4l2->volume_fade.initial_volume = initial_volume;
	}
	if (!priv_sprd_v4l2->volume_fade.initial_volume) {
		priv_sprd_v4l2->volume_fade.initial_volume = initial_volume;
	}

	priv_sprd_v4l2->volume_fade.final_volume = final_volume;

	if (priv_sprd_v4l2->volume_fade.final_volume == priv_sprd_v4l2->volume_fade.initial_volume) {
		MMRADIO_LOG_INFO("volume is same no need to fade volume");
		return;
	}

	if (radio->current_state != MM_RADIO_STATE_PLAYING) {
		MMRADIO_LOG_ERROR("we are not in playing state we are in state %d don't fade volume!!", radio->current_state);
		return;
	}

	priv_sprd_v4l2->volume_fade.fade_finished = MM_RADIO_FALSE;
	MMRADIO_LOG_INFO("fade from %d to %d", priv_sprd_v4l2->volume_fade.initial_volume, priv_sprd_v4l2->volume_fade.final_volume);

	ret = pthread_create(&priv_sprd_v4l2->volume_fade.fade_volume_thread, NULL,
	                     (void *)__mmradio_fade_volume_thread, (void *)radio);
	if (ret < 0) {
		MMRADIO_LOG_ERROR("fade volume thread creation failed - %d", ret);
		return ;
	}
}

static int _mm_radio_recording_volume_change_policy(mm_radio_t *radio, int present_volume)
{
	/* If lower than lower threshold return lower threshold */
	if (present_volume < radio->radio_tuning.recording_volume_lower_thres) {
		return radio->radio_tuning.recording_volume_lower_thres;
	}
	if (present_volume > radio->radio_tuning.recording_volume_heigher_thres) {
		return radio->radio_tuning.recording_volume_heigher_thres;
	}
	return present_volume;
}

static void __mmradio_volume_change_cb(volume_type_t type, unsigned int volume, void *user_data)
{
	mm_radio_t *radio = (mm_radio_t *)user_data;

	if (type == VOLUME_TYPE_MEDIA) {
		MMRADIO_LOG_DEBUG("Change FM Radio volume to %d\n", volume);
		__mmradio_set_volume(radio, volume);
	}
}

/* FIXME: for checking device is changed. This can be removed with another solution from sound-server */
static void __mmradio_active_device_changed_cb(mm_sound_device_in device_in, mm_sound_device_out device_out, void *user_data)
{
	mm_radio_t *radio = (mm_radio_t *)user_data;
	unsigned int volume = 0;
	unsigned int recording_volume = 0;

	MMRADIO_LOG_DEBUG("active_device_changed: in[0x%08x], out[0x%08x]\n", device_in, device_out);

	/* normal case */
	mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);

	MMRADIO_LOG_DEBUG("Change FM Radio volume to %d\n", volume);
	__mmradio_set_volume(radio, volume);
}

static int __mmradio_set_volume(mm_radio_t *radio, unsigned int value)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_VOLUME);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	if (radio->radio_fd < 0) {
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	/*During scanning or seeing it is not advisable to control volume
	 * refer to PLM - P141022-00376*/
	if (radio->is_seeking || radio->current_state == MM_RADIO_STATE_SCANNING) {
		MMRADIO_LOG_INFO("We are either seeking or scanning best not to set volume");
		return MM_ERROR_NONE;
	}

	pthread_mutex_lock(&radio->volume_lock);

	if (radio->radio_fd > 0) {
		(priv_sprd_v4l2->vctrl).id = V4L2_CID_AUDIO_VOLUME;
		(priv_sprd_v4l2->vctrl).value = fm_volume_tbl[value];

		MMRADIO_LOG_INFO("set volume:%d", (priv_sprd_v4l2->vctrl).value);

		if (ioctl(radio->radio_fd, VIDIOC_S_CTRL, &(priv_sprd_v4l2->vctrl)) < 0) {
			MMRADIO_LOG_ERROR("failed to set volume %s\n", strerror(errno));
			pthread_mutex_unlock(&radio->volume_lock);
			return MM_ERROR_RADIO_INTERNAL;
		}
	} else {
		MMRADIO_LOG_WARNING("radio_fd (%d) is closed already.", radio->radio_fd);
	}

	pthread_mutex_unlock(&radio->volume_lock);

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

static int __mmradio_get_volume(mm_radio_t *radio, unsigned int *value)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_VOLUME);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	if (radio->radio_fd < 0) {
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	pthread_mutex_lock(&radio->volume_lock);

	if (radio->radio_fd > 0) {
		(priv_sprd_v4l2->vctrl).id = V4L2_CID_AUDIO_VOLUME;

		if (ioctl(radio->radio_fd, VIDIOC_G_CTRL, &(priv_sprd_v4l2->vctrl)) < 0) {
			*value = 0;
			MMRADIO_LOG_ERROR("failed to get volume - %s\n", strerror(errno));
			pthread_mutex_unlock(&radio->volume_lock);
			return MM_ERROR_RADIO_INTERNAL;
		}
		*value = (((priv_sprd_v4l2->vctrl).value - FM_VOLUME_BASE) / (FM_VOLUME_MAX - 1));
	} else {
		MMRADIO_LOG_WARNING("radio_fd (%d) is closed already.", radio->radio_fd);
	}

	pthread_mutex_unlock(&radio->volume_lock);

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}
#endif /* USE_FM_RADIO_V4L2_VOLUME */

int _mmradio_mute(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	ret = mm_sound_set_route_info("fm_mute", "0");
	if (ret == MM_ERROR_NONE) {
		MMRADIO_LOG_INFO("set mixer mute success");
		radio->is_muted = MM_RADIO_TRUE;
	} else {
		MMRADIO_LOG_ERROR("set mixer mute failed. ret =0x%x", ret);
	}
	return ret;
}

int _mmradio_unmute(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	ret = mm_sound_set_route_info("fm_mute", "1");
	if (ret == MM_ERROR_NONE) {
		MMRADIO_LOG_INFO("set mixer unmute success");
		radio->is_muted = MM_RADIO_FALSE;
	} else {
		MMRADIO_LOG_ERROR("set mixer unmute failed. ret =0x%x", ret);
	}
	return ret;
}

int __mmradio_mute_internal(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	ret = mm_sound_set_route_info("fm_mute", "0");
	if (ret == MM_ERROR_NONE) {
		MMRADIO_LOG_INFO("set mixer mute success");
	} else {
		MMRADIO_LOG_ERROR("set mixer mute failed. ret =0x%x", ret);
	}
	return ret;
}

int __mmradio_unmute_internal(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	ret = mm_sound_set_route_info("fm_mute", "1");
	if (ret == MM_ERROR_NONE) {
		MMRADIO_LOG_INFO("set mixer unmute success");
	} else {
		MMRADIO_LOG_ERROR("set mixer unmute failed. ret =0x%x", ret);
	}
	return ret;
}


/* --------------------------------------------------------------------------
 * Name   : __mmradio_set_band_range
 * Desc   : apply max and min frequency to device_mmradio_stop
 * Param  :
 *	    [in] radio : radio handle
 * Return : zero on success, or negative value with error code
 *---------------------------------------------------------------------------*/
static int __mmradio_set_band_range(mm_radio_t *radio)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	/* get min and max freq. */
	(priv_sprd_v4l2->vt).rangelow = RADIO_FREQ_FORMAT_SET(radio->region_setting.band_min);
	(priv_sprd_v4l2->vt).rangehigh = RADIO_FREQ_FORMAT_SET(radio->region_setting.band_max);


	if (radio->radio_fd < 0) {
		MMRADIO_LOG_DEBUG("Device not ready so sending 0\n");
		return MM_ERROR_RADIO_INTERNAL;
	} else {
		/* set it to device */
		if (ioctl(radio->radio_fd, VIDIOC_S_TUNER, &(priv_sprd_v4l2->vt)) < 0) {
			MMRADIO_LOG_ERROR("failed to set band range - %s\n", strerror(errno));
			return MM_ERROR_RADIO_INTERNAL;
		}
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmradio_set_message_callback(mm_radio_t *radio, MMMessageCallback callback, void *user_param)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	radio->msg_cb = callback;
	radio->msg_cb_param = user_param;

	MMRADIO_LOG_DEBUG("msg_cb : 0x%x msg_cb_param : 0x%x\n", (unsigned int)callback, (unsigned int)user_param);

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmradio_get_state(mm_radio_t *radio, int *pState)
{
	int state = 0;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	return_val_if_fail(pState, MM_ERROR_INVALID_ARGUMENT);

	state = __mmradio_get_state(radio);

	*pState = state;

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int _mmradio_start(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	bool is_connected = false;
	unsigned int volume = 0;

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_START);

	pthread_mutex_lock(&radio->state_mutex);
	MMRADIO_LOG_INFO("<ENTER>");
	ret = _mmradio_prepare_device(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("Coundn't prepare the device - 0x%x\n", ret);
		goto error;
	}

	/*Initalize tuning parameters*/
	__mm_radio_init_tuning_params(radio);

	ret = __mmradio_init_v4l2_device(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("Coundn't init the device - 0x%x\n", ret);
		goto error;
	}

#ifdef ENABLE_FM_TUNING
	/*apply Tuning Parameters*/
	__mm_radio_apply_tuning_params( radio);
#endif
#ifdef USE_FM_RADIO_V4L2_VOLUME
	mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);
	__mmradio_set_volume(radio, volume);
#endif

	/* set stored frequency */
	_mmradio_set_frequency(radio, radio->freq);
	/*apply Tuning Parameters*/

	usleep(100 * 1000); /* FIXME: reduce FM Radio turn on noise */

#ifdef MM_RADIO_EAR_PHONE_POLICY
	ret = _mmradio_get_device_available(radio, &is_connected);
	if (!is_connected) {
		MMRADIO_LOG_ERROR("SOUND_DEVICE_AUDIO_JACK is not connected, radio can't play without audio jack");
		ret = MM_ERROR_RADIO_NO_ANTENNA;
		goto error;
	}
#endif


#ifdef FEATURE_ASM_SUPPORT
	radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;		/*add this so asm state is always updated*/
	ret = mmradio_asm_set_state(&radio->sm, ASM_STATE_PLAYING, ASM_RESOURCE_RADIO_TUNNER);
#else
	ret = mmradio_set_audio_focus(&radio->sm, _mmradio_sound_focus_watch_cb, TRUE, (void *)radio);
#endif
	if (ret) {
		MMRADIO_LOG_ERROR("failed to set asm state to PLAYING\n");
		goto error;
	}

#ifdef MM_RADIO_EAR_PHONE_POLICY
	ret = _mmradio_get_device_available(radio, &is_connected);
	if (!is_connected) {
		MMRADIO_LOG_ERROR("SOUND_DEVICE_AUDIO_JACK is not connected, radio can't play without audio jack");
		ret = MM_ERROR_RADIO_NO_ANTENNA;
		goto error;
	}
#endif

	/* we have to set mute here when mute is set before radio_start */
	if (radio->is_muted == MM_RADIO_TRUE) {
		MMRADIO_LOG_INFO("mute in start sequence");
		if (__mmradio_mute_internal(radio) != MM_ERROR_NONE) {
			MMRADIO_LOG_ERROR("set mute_internal is failed\n");
		}
	}

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_PLAYING);

#ifdef USE_GST_PIPELINE
	ret = _mmradio_start_pipeline(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("_mmradio_start_pipeline is failed\n");
		goto error;
	}
#endif
	MMRADIO_LOG_INFO("<LEAVE>");
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;

error:
#ifdef FEATURE_ASM_SUPPORT
	if (radio->sm.state == ASM_STATE_PLAYING) {
		if (mmradio_asm_set_state(&radio->sm, ASM_STATE_STOP, ASM_RESOURCE_NONE)) {
			MMRADIO_LOG_ERROR("failed to set asm-state to stop");
		}
	}
#endif
	__mmradio_deinit_v4l2_device(radio);

	pthread_mutex_unlock(&radio->state_mutex);
	_mmradio_unprepare_device(radio);
	MMRADIO_LOG_ERROR("<LEAVE> with ret = 0x%x", ret);
	return ret;
}

int _mmradio_stop(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	MMRADIO_CHECK_INSTANCE(radio);
	/*MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_STOP); */
	pthread_mutex_lock(&radio->state_mutex);

	MMRADIO_CHECK_STATE_GOTO_IF_FAIL(ret, radio, MMRADIO_COMMAND_STOP, error);
	MMRADIO_LOG_INFO("<ENTER>");


	radio->seek_unmute = MM_RADIO_FALSE;
	/*cancel if any seek*/
	_mmradio_seek_cancel(radio);

#ifdef USE_FM_RADIO_V4L2_VOLUME
	/*If there is volume fading going on we better stop it*/
	_mmradio_fadevolume_cancel(radio);
#endif

	/* mute for stop */
	MMRADIO_LOG_INFO("mute in stop sequence");
	if (__mmradio_mute_internal(radio) != MM_ERROR_NONE) {
		MMRADIO_LOG_ERROR("set mute_internal is failed\n");
	}

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_READY);
#ifdef FEATURE_ASM_SUPPORT
	ret = mmradio_asm_set_state(&radio->sm, ASM_STATE_STOP, ASM_RESOURCE_NONE);
	if (ret) {
		MMRADIO_LOG_ERROR("failed to set asm state to STOPEED\n");
		goto error;
	}
#endif
#ifdef USE_GST_PIPELINE
	ret = _mmradio_stop_pipeline(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("_mmradio_stop_pipeline is failed\n");
		goto error;
	}
#endif

	/*call by application*/
	if (radio->sm.by_asm_cb == MMRADIO_ASM_CB_NONE) {
		__mmradio_deinit_v4l2_device(radio);
	}
	MMRADIO_LOG_INFO("Radio Stopped");


	MMRADIO_LOG_INFO("<LEAVE>");
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;
error:
	MMRADIO_LOG_ERROR("<LEAVE> with ret = 0x%x", ret);
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;
}

/* There is no concept of pause in radio transmission.
 * This is essentially a stop call,
 * but this is added to keep sound-server's state in order */
int _mmradio_pause(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	MMRADIO_CHECK_INSTANCE(radio);

	pthread_mutex_lock(&radio->state_mutex);
	MMRADIO_CHECK_STATE_GOTO_IF_FAIL(ret, radio, MMRADIO_COMMAND_STOP, error);
	MMRADIO_LOG_INFO("<ENTER>");

	radio->seek_unmute = MM_RADIO_FALSE;
	/*cancel if any seek*/
	_mmradio_seek_cancel(radio);

#ifdef USE_FM_RADIO_V4L2_VOLUME
	/*If there is volume fading going on we better stop it*/
	_mmradio_fadevolume_cancel(radio);
#endif

	/* mute for pause */
	MMRADIO_LOG_INFO("mute in pause sequence");
	if (__mmradio_mute_internal(radio) != MM_ERROR_NONE) {
		MMRADIO_LOG_ERROR("set mute_internal is failed\n");
	}
	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_READY);
#ifdef FEATURE_ASM_SUPPORT
	ret = mmradio_asm_set_state(&radio->sm, ASM_STATE_PAUSE, ASM_RESOURCE_NONE);
	if (ret) {
		MMRADIO_LOG_ERROR("failed to set asm state to PLAYING\n");
		goto error;
	}
#endif
#ifdef USE_GST_PIPELINE
	ret = _mmradio_stop_pipeline(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("_mmradio_stop_pipeline is failed\n");
		goto error;
	}
#endif

	if (radio->sm.by_asm_cb == MMRADIO_ASM_CB_NONE) {
		__mmradio_deinit_v4l2_device(radio);
	}
	MMRADIO_LOG_INFO("Radio Stopped from pause");
	MMRADIO_LOG_INFO("<LEAVE>");
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;
error:
	MMRADIO_LOG_ERROR("<LEAVE> with ret = 0x%x", ret);
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;
}

int _mmradio_seek(mm_radio_t *radio, MMRadioSeekDirectionType direction)
{
	int ret = 0;

	MMRADIO_LOG_INFO("<ENTER>");

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_SEEK);

	if (radio->is_seeking) {
		MMRADIO_LOG_ERROR("[RADIO_ERROR_INVALID_OPERATION]radio is seeking, can't serve another request try again");
		return MM_ERROR_RADIO_INTERNAL;
	}

	radio->seek_unmute = MM_RADIO_FALSE;
	radio->is_seeking = MM_RADIO_TRUE;
	radio->seek_cancel = MM_RADIO_FALSE;
	if (!radio->is_muted) {
		if (_mmradio_mute(radio) != MM_ERROR_NONE) {
			return MM_ERROR_RADIO_NOT_INITIALIZED;
		}
		radio->seek_unmute = MM_RADIO_TRUE;
	}

	MMRADIO_LOG_DEBUG("trying to seek. direction[0:UP/1:DOWN) %d\n", direction);
	radio->seek_direction = direction;

	ret = pthread_create(&radio->seek_thread, NULL,
	                     (void *)__mmradio_seek_thread, (void *)radio);

	if (ret) {
		MMRADIO_LOG_DEBUG("failed create thread\n");
		/*reset parameters*/
		radio->is_seeking = MM_RADIO_FALSE;
		radio->seek_cancel = MM_RADIO_TRUE;
		if (radio->seek_unmute) {
			if (_mmradio_unmute(radio) != MM_ERROR_NONE) {
				radio->seek_unmute = MM_RADIO_FALSE;
			}
		}
		return MM_ERROR_RADIO_INTERNAL;
	}
	MMRADIO_LOG_INFO("<LEAVE>");

	return MM_ERROR_NONE;
}

void _mmradio_seek_cancel(mm_radio_t *radio)
{
	int ret = 0;
	MMRADIO_LOG_INFO("<ENTER>");

	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	/*cancel any outstanding seek request*/
	radio->seek_cancel = MM_RADIO_TRUE;
	if (radio->seek_thread) {
		ret = pthread_mutex_trylock(&priv_sprd_v4l2->seek_cancel_mutex);
		MMRADIO_LOG_DEBUG("try lock ret: %s (%d)", strerror(ret), ret);
		if (ret == EBUSY) { /* it was already locked by other */
			MMRADIO_LOG_DEBUG("send SEEK ABORT with FMRX_PROPERTY_SEARCH_ABORT");
		} else if (ret == 0) {
			MMRADIO_LOG_DEBUG("trylock is successful. unlock now");
			pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
		} else {
			MMRADIO_LOG_ERROR("trylock is failed but Not EBUSY. ret: %d", ret);
		}
		MMRADIO_LOG_DEBUG("pthread_join seek_thread");
		pthread_join(radio->seek_thread, NULL);
		MMRADIO_LOG_DEBUG("done");
		radio->is_seeking = MM_RADIO_FALSE;
		radio->seek_thread = 0;
	}
	MMRADIO_LOG_INFO("<LEAVE>");
}

int _mmradio_start_scan(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_START_SCAN);

	int scan_tr_id = 0;

	pthread_mutex_lock(&radio->state_mutex);
	MMRADIO_LOG_INFO("<ENTER>");

	/*Initalize tuning parameters*/
	__mm_radio_init_tuning_params(radio);

	/*Lets hope that device is prepared already if not no matter we'll try to prepare*/
	MMRADIO_LOG_INFO("Starting Radio in Scan");
	ret = _mmradio_prepare_device(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("Coundn't prepare the device - 0x%x\n", ret);
		goto error;
	}
	ret = __mmradio_init_v4l2_device(radio);
	if (ret) {
		MMRADIO_LOG_ERROR("Coundn't init the device - 0x%x\n", ret);
		goto error;
	}
#ifdef ENABLE_FM_TUNING
	/*apply Tuning Parameters*/
	__mm_radio_apply_tuning_params( radio);
#endif

	radio->stop_scan = MM_RADIO_FALSE;

	scan_tr_id = pthread_create(&radio->scan_thread, NULL,
	                            (void *)__mmradio_scan_thread, (void *)radio);

	if (scan_tr_id != 0) {
		MMRADIO_LOG_DEBUG("failed to create thread : scan\n");
		ret = MM_ERROR_RADIO_NOT_INITIALIZED;
		goto error;
	}

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_SCANNING);

	MMRADIO_LOG_INFO("<LEAVE>");
	pthread_mutex_unlock(&radio->state_mutex);
	return MM_ERROR_NONE;
error:
	MMRADIO_LOG_ERROR("<LEAVE> with ret = 0x%x", ret);
	pthread_mutex_unlock(&radio->state_mutex);
	return ret;
}

int _mmradio_stop_scan(mm_radio_t *radio)
{
	MMRADIO_LOG_INFO("<ENTER>");
	int ret = 0;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_STOP_SCAN);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	radio->stop_scan = MM_RADIO_TRUE;

	if (radio->scan_thread) {
		/* make sure all the search is stopped else we'll wait till search finish which is not ideal*/
		ret = pthread_mutex_trylock(&priv_sprd_v4l2->seek_cancel_mutex);
		MMRADIO_LOG_DEBUG("try lock ret: %s (%d)", strerror(ret), ret);
		if (ret == EBUSY) { /* it was already locked by other */
			MMRADIO_LOG_DEBUG("send SEEK ABORT with FMRX_PROPERTY_SEARCH_ABORT");
		} else if (ret == 0) {
			MMRADIO_LOG_DEBUG("trylock is successful. unlock now");
			pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
		} else {
			MMRADIO_LOG_ERROR("trylock is failed but Not EBUSY. ret: %d", ret);
		}
		MMRADIO_LOG_DEBUG("pthread_join scan_thread");
		pthread_join(radio->scan_thread, NULL);
		/*Clean up radio*/
		__mmradio_deinit_v4l2_device(radio);
		radio->scan_thread = 0;
	}

	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_READY);

#ifdef USE_FM_RADIO_V4L2_VOLUME
	/*if there were any volume changes, we better set them right here
	 * refer to PLM - P141022-00376*/
	{
		int volume = 0;
		ret = mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);
		if (ret) {
			MMRADIO_LOG_ERROR("mm_sound_volume_get_value failed with [0x%x]", ret);
		} else {
			/*we are good to set volume here*/
			ret = __mmradio_set_volume(radio, volume);
			if (ret) {
				MMRADIO_LOG_ERROR("__mmradio_set_volume failed with [0x%x]", ret);
			}
		}
	}
#endif

	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_STOP, NULL);

	MMRADIO_LOG_INFO("<LEAVE>");

	return MM_ERROR_NONE;
}

int _mm_radio_get_signal_strength(mm_radio_t *radio, int *value)
{
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	return_val_if_fail(value, MM_ERROR_INVALID_ARGUMENT);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *) radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE(priv_sprd_v4l2);

	/* just return stored frequency if radio device is not ready */
	if (radio->radio_fd < 0) {
		MMRADIO_LOG_DEBUG("Device not ready so sending 0\n");
		*value = 0;
		return MM_ERROR_NONE;
	} else {
		if (ioctl(radio->radio_fd, VIDIOC_G_TUNER, &(priv_sprd_v4l2->vt)) < 0) {
			MMRADIO_LOG_ERROR("ioctl VIDIOC_G_TUNER error - %s\n", strerror(errno));
			return MM_ERROR_RADIO_INTERNAL;
		}
	}

	/* RSSI from controller will be in range of -128 to +127.
	But V4L2 API defines the range of 0 to 65535. So convert this value
	FM rssi is cannot be 1~128 dbm normally, although range is -128 to +127 */
	/* (65535 / 128) = 511.9921875. kernel will also use same value */
	*value = priv_sprd_v4l2->vt.signal / 511 /*(65535/128)*/ - 128;
	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

static void __mmradio_scan_thread(mm_radio_t *radio)
{
	int ret = 0;
	int prev_freq = 0;
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;
	struct v4l2_hw_freq_seek vs = {0,};
	vs.tuner = TUNER_INDEX;
	vs.type = V4L2_TUNER_RADIO;
	vs.wrap_around = 0; /* around:1 not around:0 */
	vs.seek_upward = 1; /* up : 1	------- down : 0 */

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *)radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	MMRADIO_LOG_INFO("mute in scan thread");
	if (__mmradio_mute_internal(radio) != MM_ERROR_NONE) {
		MMRADIO_LOG_ERROR("set mute_internal is failed\n");
	};

	if (_mmradio_set_frequency(radio, radio->region_setting.band_min) != MM_ERROR_NONE) {
		MMRADIO_LOG_ERROR("set min freq failed during scanning. min freq: %d", radio->region_setting.band_min);
		goto FINISHED;
	} else {
		MMRADIO_LOG_DEBUG("scan start with min freq: %d", radio->region_setting.band_min);
	}

	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_START, NULL);
	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_SCANNING);

	while (!radio->stop_scan) {
		int freq = 0;
		MMMessageParamType param = {0,};

		MMRADIO_LOG_DEBUG("try to scan");
		pthread_mutex_lock(&priv_sprd_v4l2->seek_cancel_mutex);
		MMRADIO_LOG_DEBUG("search start during scanning\n");
		if (radio->stop_scan) {
			MMRADIO_LOG_DEBUG("scan was canceled why search so we return");
			pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
			goto FINISHED;
		}
		ret = ioctl(radio->radio_fd, VIDIOC_S_HW_FREQ_SEEK, &vs);
		MMRADIO_LOG_DEBUG("search end during scanning");
		pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
		if (ret == -1) {
			if (errno == EAGAIN) {
				MMRADIO_LOG_ERROR("scanning timeout\n");
			} else if (errno == EINVAL) {
				MMRADIO_LOG_ERROR("The tuner index is out of bounds or the value in the type field is wrong or we were asked to stop search");
				break;
			} else {
				MMRADIO_LOG_ERROR("Error: %s, %d\n", strerror(errno), errno);
				break;
			}
		}

		/* now we can get new frequency from radio device */

		if (radio->stop_scan)
			break;

		ret = ioctl(radio->radio_fd, VIDIOC_G_FREQUENCY, &(priv_sprd_v4l2->vf));
		if (ret < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_G_FREQUENCY failed with %s during SEEK", strerror(errno));
		} else {
			freq = RADIO_FREQ_FORMAT_GET((priv_sprd_v4l2->vf).frequency);
			radio->freq = freq; /* update freq in handle */
			if (freq < prev_freq) {
				MMRADIO_LOG_DEBUG("scanning wrapped around. stopping scan\n");
				break;
			}

			if (freq == prev_freq) {
				MMRADIO_LOG_ERROR("frequency is same we wrapped around, we are finished scanning");
				break;
			}

			prev_freq = param.radio_scan.frequency = freq;
			MMRADIO_LOG_DEBUG("scanning : new frequency : [%d]\n", param.radio_scan.frequency);

			/* drop if max freq is scanned */
			if (param.radio_scan.frequency == radio->region_setting.band_max) {
				MMRADIO_LOG_DEBUG("%d freq is dropping...and stopping scan\n", param.radio_scan.frequency);
				break;
			}
			if (radio->stop_scan)
				break; /* doesn't need to post */

			MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_INFO, &param);
		}
	}
FINISHED:
	radio->scan_thread = 0;

	if (!radio->stop_scan) {
		/*Clean up radio*/
		__mmradio_deinit_v4l2_device(radio);
	}
	MMRADIO_SET_STATE(radio, MM_RADIO_STATE_READY);

	if (!radio->stop_scan) {

#ifdef USE_FM_RADIO_V4L2_VOLUME
		/*if there were any volume changes, we better set them right here
		 * refer to PLM - P141022-00376*/
		{
			int volume = 0;
			ret = mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);
			if (ret) {
				MMRADIO_LOG_ERROR("mm_sound_volume_get_value failed with [0x%x]", ret);
			} else {
				/*we are good to set volume here*/
				ret = __mmradio_set_volume(radio, volume);
				if (ret) {
					MMRADIO_LOG_ERROR("__mmradio_set_volume failed with [0x%x]", ret);
				}
			}
		}
#endif

		MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_FINISH, NULL);
	}

	MMRADIO_LOG_FLEAVE();

	pthread_exit(NULL);

	return;
}

static bool __is_tunable_frequency(mm_radio_t *radio, int freq)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	if (freq == radio->region_setting.band_max || freq == radio->region_setting.band_min)
		return MM_RADIO_FALSE;

	MMRADIO_LOG_FLEAVE();

	return MM_RADIO_TRUE;
}

static void __mmradio_seek_thread(mm_radio_t *radio)
{
	int ret = 0;
	int freq = 0;
	int volume = 0;
	MMMessageParamType param = {0,};
	struct v4l2_hw_freq_seek vs = {0,};
	mm_radio_priv_v4l2_t *priv_sprd_v4l2 = NULL;

	vs.tuner = TUNER_INDEX;
	vs.type = V4L2_TUNER_RADIO;
	vs.wrap_around = DEFAULT_WRAP_AROUND;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE_RETURN_VOID(radio);

	priv_sprd_v4l2 = (mm_radio_priv_v4l2_t *)radio->vender_specific_handle;
	MMRADIO_CHECK_INSTANCE_RETURN_VOID(priv_sprd_v4l2);

	/* check direction */
	switch (radio->seek_direction) {
		case MM_RADIO_SEEK_UP:
			vs.seek_upward = 1;
			break;
		default:
			vs.seek_upward = 0;
			break;
	}

	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_START, NULL);

	if (!radio->seek_cancel) {
		MMRADIO_LOG_DEBUG("try to seek ");
		pthread_mutex_lock(&priv_sprd_v4l2->seek_cancel_mutex);
		MMRADIO_LOG_DEBUG("seek start\n");
		if (radio->seek_cancel) {
			MMRADIO_LOG_DEBUG("seek was canceled so we return failure to application");
			pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
			goto SEEK_FAILED;
		}
		ret = ioctl(radio->radio_fd, VIDIOC_S_HW_FREQ_SEEK, &vs);
		MMRADIO_LOG_DEBUG("seek end");
		pthread_mutex_unlock(&priv_sprd_v4l2->seek_cancel_mutex);
		if (ret == -1) {
			if (errno == EAGAIN) {
				/* FIXIT : we need retrying code here */
				MMRADIO_LOG_ERROR("seeking timeout\n");
				goto SEEK_FAILED;
			} else if (errno == EINVAL) {
				MMRADIO_LOG_ERROR("The tuner index is out of bounds or the value in the type field is wrong Or we were asked to stop search.");
				goto SEEK_FAILED;
			} else {
				MMRADIO_LOG_ERROR("Error: %s, %d\n", strerror(errno), errno);
				goto SEEK_FAILED;
			}
		}

		/*seek -> scan causes sound fix to that issue*/
		if (radio->seek_cancel) {
			goto SEEK_FAILED;
		}

		/* now we can get new frequency from radio device */
		ret = ioctl(radio->radio_fd, VIDIOC_G_FREQUENCY, &(priv_sprd_v4l2->vf));
		if (ret < 0) {
			MMRADIO_LOG_ERROR("VIDIOC_G_FREQUENCY failed with %s during SEEK", strerror(errno));
			goto SEEK_FAILED;
		} else {
			freq = RADIO_FREQ_FORMAT_GET((priv_sprd_v4l2->vf).frequency);
			radio->freq = freq; /* update freq in handle */
		}

		MMRADIO_LOG_INFO("found frequency %d during seek\n", radio->freq);

		/* if same freq is found, ignore it and search next one. */
		if (freq == radio->prev_seek_freq) {
			MMRADIO_LOG_DEBUG("It's same with previous found one. So, trying next one. \n");
			goto SEEK_FAILED;
		}

		if (__is_tunable_frequency(radio, freq)) {/* check if it's limit freq or not */
			/* now tune to new frequency */
			ret = _mmradio_set_frequency(radio, freq);
			if (ret) {
				MMRADIO_LOG_ERROR("failed to tune to new frequency\n");
				goto SEEK_FAILED;
			}
		}
		MMRADIO_LOG_DEBUG("seek_unmute : [%d] seek_canel - [%d]\n", radio->seek_unmute, radio->seek_cancel);
		if (radio->seek_unmute) {
			/* now turn on radio
			* In the case of limit freq, tuner should be unmuted.
			* Otherwise, sound can't output even though application set new frequency.
			*/
			ret = _mmradio_unmute(radio);
			if (ret) {
				MMRADIO_LOG_ERROR("failed to un_mute failed\n");
				goto SEEK_FAILED;
			}
			radio->seek_unmute = MM_RADIO_FALSE;
		}

		param.radio_scan.frequency = radio->prev_seek_freq = freq;
		MMRADIO_LOG_DEBUG("seeking : new frequency : [%d]\n", param.radio_scan.frequency);
		MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_FINISH, &param);
	}

	radio->seek_thread = 0;
	radio->is_seeking = MM_RADIO_FALSE;

	/*if there were any volume changes, we better set them right here
	 * refer to PLM - P141022-00376*/
	mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);
	mm_sound_volume_set_value(VOLUME_TYPE_MEDIA, volume);

	MMRADIO_LOG_FLEAVE();

	pthread_exit(NULL);
	return;

SEEK_FAILED:
	if (radio->seek_unmute) {
		/* now turn on radio
		* In the case of limit freq, tuner should be unmuted.
		* Otherwise, sound can't output even though application set new frequency.
		*/
		ret = _mmradio_unmute(radio);
		if (ret) {
			MMRADIO_LOG_ERROR("failed to un_mute failed\n");
		}
		radio->seek_unmute = MM_RADIO_FALSE;
	}
	/* freq -1 means it's failed to seek */
	param.radio_scan.frequency = -1;
	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_FINISH, &param);
	radio->is_seeking = MM_RADIO_FALSE;

	/*if there were any volume changes, we better set them right here
	 * refer to PLM - P141022-00376*/
	mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &volume);
	mm_sound_volume_set_value(VOLUME_TYPE_MEDIA, volume);

	pthread_exit(NULL);
	return;
}

static bool __mmradio_post_message(mm_radio_t *radio, enum MMMessageType msgtype, MMMessageParamType *param)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	if (!radio->msg_cb) {
		MMRADIO_LOG_WARNING("failed to post a message\n");
		return MM_RADIO_FALSE;
	}

	MMRADIO_LOG_DEBUG("address of msg_cb = %p\n", radio->msg_cb);

	radio->msg_cb(msgtype, param, radio->msg_cb_param);

	MMRADIO_LOG_FLEAVE();

	return MM_RADIO_TRUE;
}

static int __mmradio_check_state(mm_radio_t *radio, MMRadioCommand command)
{
	MMRadioStateType radio_state = MM_RADIO_STATE_NUM;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);

	radio_state = __mmradio_get_state(radio);

	MMRADIO_LOG_DEBUG("incomming command = %d  current state = %d\n", command, radio_state);

	switch (command) {
		case MMRADIO_COMMAND_CREATE: {
				if (radio_state != 0)
					goto NO_OP;
			}
			break;

		case MMRADIO_COMMAND_REALIZE: {
				if (radio_state == MM_RADIO_STATE_READY ||
				    radio_state == MM_RADIO_STATE_PLAYING ||
				    radio_state == MM_RADIO_STATE_SCANNING)
					goto NO_OP;

				if (radio_state == 0)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_UNREALIZE: {
				if (radio_state == MM_RADIO_STATE_NULL)
					goto NO_OP;

				/* we can call unrealize at any higher state */
			}
			break;

		case MMRADIO_COMMAND_START: {
				if (radio_state == MM_RADIO_STATE_PLAYING)
					goto NO_OP;

				if (radio_state != MM_RADIO_STATE_READY)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_STOP: {
				if (radio_state == MM_RADIO_STATE_READY)
					goto NO_OP;

				if (radio_state != MM_RADIO_STATE_PLAYING)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_START_SCAN: {
				if (radio_state == MM_RADIO_STATE_SCANNING)
					goto NO_OP;

				if (radio_state != MM_RADIO_STATE_READY)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_STOP_SCAN: {
				if (radio_state == MM_RADIO_STATE_READY)
					goto NO_OP;

				if (radio_state != MM_RADIO_STATE_SCANNING)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_DESTROY:
		case MMRADIO_COMMAND_VOLUME:
		case MMRADIO_COMMAND_MUTE:
		case MMRADIO_COMMAND_UNMUTE:
		case MMRADIO_COMMAND_SET_FREQ:
		case MMRADIO_COMMAND_GET_FREQ:
		case MMRADIO_COMMAND_SET_REGION: {
				/* we can do it at any state */
			}
			break;

		case MMRADIO_COMMAND_SEEK: {
				if (radio_state != MM_RADIO_STATE_PLAYING)
					goto INVALID_STATE;
			}
			break;

		case MMRADIO_COMMAND_GET_REGION: {
				if (radio_state == MM_RADIO_STATE_NULL)
					goto INVALID_STATE;
			}
			break;
		case MMRADIO_COMMAND_TUNE: {
				if (radio_state < MM_RADIO_STATE_READY || radio_state >= MM_RADIO_STATE_NUM)
					goto INVALID_STATE;
			}
			break;
		case MMRADIO_COMMAND_RDS_START: {
				if (radio_state != MM_RADIO_STATE_PLAYING)
					goto INVALID_STATE;
			}
			break;
		case MMRADIO_COMMAND_RDS_STOP: {
				if (radio_state != MM_RADIO_STATE_PLAYING)
					goto INVALID_STATE;
			}
			break;
		default:
			MMRADIO_LOG_DEBUG("not handled in FSM. don't care it\n");
			break;
	}

	MMRADIO_LOG_DEBUG("status OK\n");

	radio->cmd = command;

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;


INVALID_STATE:
	MMRADIO_LOG_WARNING("invalid state. current = %d  command = %d\n",
	                    radio_state, command);
	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_RADIO_INVALID_STATE;


NO_OP:
	MMRADIO_LOG_WARNING("mm-radio is in the desired state(%d). doing noting\n", radio_state);
	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_RADIO_NO_OP;

}

static bool __mmradio_set_state(mm_radio_t *radio, int new_state)
{
	MMMessageParamType msg = {0, };
	int msg_type = MM_MESSAGE_UNKNOWN;

	MMRADIO_LOG_FENTER();

	if (!radio) {
		MMRADIO_LOG_WARNING("calling set_state with invalid radio handle\n");
		return MM_RADIO_FALSE;
	}

	if (radio->current_state == new_state && radio->pending_state == 0) {
		MMRADIO_LOG_WARNING("we are in same state\n");
		return MM_RADIO_TRUE;
	}

	/* set state */
	radio->old_state = radio->current_state;
	radio->current_state = new_state;

	/* fill message param */
	msg.state.previous = radio->old_state;
	msg.state.current = radio->current_state;

	/* post message to application */
	switch (radio->sm.by_asm_cb) {
		case MMRADIO_ASM_CB_NONE: {
				msg_type = MM_MESSAGE_STATE_CHANGED;
				MMRADIO_POST_MSG(radio, MM_MESSAGE_STATE_CHANGED, &msg);
			}
			break;

		case MMRADIO_ASM_CB_POSTMSG: {
				msg_type = MM_MESSAGE_STATE_INTERRUPTED;
				msg.union_type = MM_MSG_UNION_CODE;
				msg.code = radio->sm.event_src;
				MMRADIO_POST_MSG(radio, MM_MESSAGE_STATE_INTERRUPTED, &msg);
			}
			break;

		case MMRADIO_ASM_CB_SKIP_POSTMSG:
		default:
			break;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_RADIO_TRUE;
}

static int __mmradio_get_state(mm_radio_t *radio)
{
	MMRADIO_CHECK_INSTANCE(radio);

	MMRADIO_LOG_DEBUG("radio state : current : [%d]   old : [%d]   pending : [%d]\n",
	                  radio->current_state, radio->old_state, radio->pending_state);

	return radio->current_state;
}
#ifdef FEATURE_ASM_SUPPORT
static ASM_cb_result_t __mmradio_asm_callback(int handle, ASM_event_sources_t event_source, ASM_sound_commands_t command, unsigned int sound_status, void *cb_data)
{
	mm_radio_t *radio = (mm_radio_t *) cb_data;
	int result = MM_ERROR_NONE;
	ASM_cb_result_t	cb_res = ASM_CB_RES_NONE;

	MMRADIO_LOG_FENTER();

	radio->sm.event_src = event_source;

	switch (command) {
		case ASM_COMMAND_STOP: {
				MMRADIO_LOG_INFO("got ASM_COMMAND_STOP cmd. (cmd: %d event_src: %d)\n", command, event_source);
				switch (event_source) {
					case ASM_EVENT_SOURCE_CALL_START:
					case ASM_EVENT_SOURCE_ALARM_START:
					case ASM_EVENT_SOURCE_EMERGENCY_START:
					case ASM_EVENT_SOURCE_EARJACK_UNPLUG:
					case ASM_EVENT_SOURCE_MEDIA:
					case ASM_EVENT_SOURCE_RESOURCE_CONFLICT:
					case ASM_EVENT_SOURCE_NOTIFY_START:
					case ASM_EVENT_SOURCE_OTHER_PLAYER_APP:
						/*				case ASM_EVENT_SOURCE_RESUMABLE_MEDIA: */
						{
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
							result = _mmradio_stop(radio);
							if (result) {
								MMRADIO_LOG_ERROR("failed to stop radio\n");
							}
							_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);
							if (event == NULL) {
								cb_res = ASM_CB_RES_IGNORE;
								return cb_res;
							}

							event->event_type = MMRADIO_EVENT_STOP;
							g_async_queue_push_sorted(radio->event_queue, event, event_comparator, NULL);
							MMRADIO_LOG_DEBUG("unprepare in asm callback\n");
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
						}
						break;
					default: {
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_SKIP_POSTMSG;
							result = _mmradio_stop(radio);
							if (result) {
								MMRADIO_LOG_ERROR("failed to stop radio\n");
							}
							_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);
							if (event == NULL) {
								cb_res = ASM_CB_RES_IGNORE;
								return cb_res;
							}

							event->event_type = MMRADIO_EVENT_STOP;
							g_async_queue_push_sorted(radio->event_queue, event, event_comparator, NULL);
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
						}
						break;
				}
				cb_res = ASM_CB_RES_STOP;
			}
			break;
		case ASM_COMMAND_PAUSE: {
				MMRADIO_LOG_INFO("got ASM_COMMAND_PAUSE cmd. (cmd: %d event_src: %d)\n", command, event_source);
				switch (event_source) {
					case ASM_EVENT_SOURCE_CALL_START:
					case ASM_EVENT_SOURCE_ALARM_START:
					case ASM_EVENT_SOURCE_EMERGENCY_START:
					case ASM_EVENT_SOURCE_EARJACK_UNPLUG:
					case ASM_EVENT_SOURCE_MEDIA:
					case ASM_EVENT_SOURCE_RESOURCE_CONFLICT:
					case ASM_EVENT_SOURCE_NOTIFY_START:
					case ASM_EVENT_SOURCE_OTHER_PLAYER_APP:
						/*				case ASM_EVENT_SOURCE_RESUMABLE_MEDIA: */
						{
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
							result = _mmradio_pause(radio);
							if (result) {
								MMRADIO_LOG_ERROR("failed to pause radio\n");
							}
							_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);
							if (event == NULL) {
								cb_res = ASM_CB_RES_IGNORE;
								return cb_res;
							}

							event->event_type = MMRADIO_EVENT_STOP;
							g_async_queue_push_sorted(radio->event_queue, event, event_comparator, NULL);
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
						}
						break;
#if 0
						/* to handle timer recording with RADIO_INTERRUPTED_BY_RESUME_CANCEL */
					case ASM_EVENT_SOURCE_RESUMABLE_CANCELED: {
							MMRadioStateType radio_cur_state = MM_RADIO_STATE_NUM;
							radio_cur_state = __mmradio_get_state(radio);
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
							if (radio_cur_state == MM_RADIO_STATE_READY) {
								MMMessageParamType msg = {0, };
								int msg_type = MM_MESSAGE_UNKNOWN;

								msg_type = MM_MESSAGE_STATE_INTERRUPTED;
								msg.union_type = MM_MSG_UNION_CODE;
								msg.code = radio->sm.event_src;
								MMRADIO_LOG_INFO("send RADIO_INTERRUPTED_BY_RESUME_CANCEL to clear timer. (cmd: %d event_src: %d)\n", command, event_source);
								MMRADIO_POST_MSG(radio, MM_MESSAGE_STATE_INTERRUPTED, &msg);
							}
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
						}
						break;
#endif
					default: {
							MMRADIO_LOG_WARNING("ASM_COMMAND_PAUSE but event_source is %d\n", event_source);
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_SKIP_POSTMSG;
							result = _mmradio_pause(radio);
							if (result) {
								MMRADIO_LOG_ERROR("failed to pause radio\n");
							}
							_mm_radio_event_s *event = g_slice_new0(_mm_radio_event_s);
							if (event == NULL) {
								cb_res = ASM_CB_RES_IGNORE;
								return cb_res;
							}

							event->event_type = MMRADIO_EVENT_STOP;
							g_async_queue_push_sorted(radio->event_queue, event, event_comparator, NULL);
							radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
						}
						break;
				}
				cb_res = ASM_CB_RES_PAUSE;
			}
			break;

		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_RESUME: {
				MMMessageParamType msg = {0,};
				msg.union_type = MM_MSG_UNION_CODE;
				msg.code = event_source;

				if (command == ASM_COMMAND_PLAY)
					MMRADIO_LOG_INFO("got ASM_COMMAND_PLAY  (cmd: %d event_src: %d)\n", command, event_source);
				else if (command == ASM_COMMAND_RESUME)
					MMRADIO_LOG_INFO("got ASM_COMMAND_RESUME (cmd: %d event_src: %d)\n", command, event_source);

				MMRADIO_LOG_DEBUG("Got ASM resume message by %d\n", msg.code);
				MMRADIO_POST_MSG(radio, MM_MESSAGE_READY_TO_RESUME, &msg);

				cb_res = ASM_CB_RES_IGNORE;
				radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;
			}
			break;

		default:
			break;
	}

	MMRADIO_LOG_FLEAVE();

	return cb_res;
}
#else
void _mmradio_sound_focus_cb(int id, mm_sound_focus_type_e focus_type,
                             mm_sound_focus_state_e focus_state, const char *reason_for_change,
                             const char *additional_info, void *user_data)
{
	mm_radio_t *radio = (mm_radio_t *) user_data;
	ASM_event_sources_t event_source;
	int result = MM_ERROR_NONE;
	int postMsg = false;

	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE(radio);

	mmradio_get_audio_focus_reason(focus_state, reason_for_change, &event_source, &postMsg);
	radio->sm.event_src = event_source;

	switch (focus_state) {
		case FOCUS_IS_RELEASED:
			radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
			result = _mmradio_stop(radio);
			if (result) {
				MMRADIO_LOG_ERROR("failed to stop radio\n");
			}
			MMRADIO_LOG_DEBUG("FOCUS_IS_RELEASED\n");
			break;

		case FOCUS_IS_ACQUIRED: {
				MMMessageParamType msg = {0,};
				msg.union_type = MM_MSG_UNION_CODE;
				msg.code = event_source;
				if (postMsg)
					MMRADIO_POST_MSG(radio, MM_MESSAGE_READY_TO_RESUME, &msg);

				radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;

				MMRADIO_LOG_DEBUG("FOCUS_IS_ACQUIRED\n");
			}
			break;

		default:
			MMRADIO_LOG_DEBUG("Unknown focus_state\n");
			break;
	}
	MMRADIO_LOG_FLEAVE();
}

void _mmradio_sound_focus_watch_cb(int id, mm_sound_focus_type_e focus_type, mm_sound_focus_state_e focus_state,
                                   const char *reason_for_change, const char *additional_info, void *user_data)
{
	mm_radio_t *radio = (mm_radio_t *) user_data;
	ASM_event_sources_t event_source;
	int result = MM_ERROR_NONE;
	int postMsg = false;

	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE(radio);

	mmradio_get_audio_focus_reason(!focus_state, reason_for_change, &event_source, &postMsg);
	radio->sm.event_src = event_source;

	switch (focus_state) {
		case FOCUS_IS_RELEASED: {
				MMMessageParamType msg = {0,};
				msg.union_type = MM_MSG_UNION_CODE;
				msg.code = event_source;
				if (postMsg)
					MMRADIO_POST_MSG(radio, MM_MESSAGE_READY_TO_RESUME, &msg);

				radio->sm.by_asm_cb = MMRADIO_ASM_CB_NONE;

				MMRADIO_LOG_DEBUG("FOCUS_IS_ACQUIRED postMsg: %d\n", postMsg);
			}
			break;

		case FOCUS_IS_ACQUIRED: {
				radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
				result = _mmradio_stop(radio);
				if (result) {
					MMRADIO_LOG_ERROR("failed to stop radio\n");
				}
				MMRADIO_LOG_DEBUG("FOCUS_IS_RELEASED\n");
				break;
			}
			break;

		default:
			MMRADIO_LOG_DEBUG("Unknown focus_state postMsg : %d\n", postMsg);
			break;
	}
	MMRADIO_LOG_FLEAVE();
}
#endif
int _mmradio_get_region_type(mm_radio_t *radio, MMRadioRegionType *type)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_GET_REGION);

	return_val_if_fail(type, MM_ERROR_INVALID_ARGUMENT);

	*type = radio->region_setting.country;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_get_region_frequency_range(mm_radio_t *radio, unsigned int *min_freq, unsigned int *max_freq)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_GET_REGION);

	return_val_if_fail(min_freq && max_freq, MM_ERROR_INVALID_ARGUMENT);

	*min_freq = radio->region_setting.band_min;
	*max_freq = radio->region_setting.band_max;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_get_channel_spacing(mm_radio_t *radio, unsigned int *ch_spacing)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE(radio);
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL(radio, MMRADIO_COMMAND_GET_REGION);

	return_val_if_fail(ch_spacing, MM_ERROR_INVALID_ARGUMENT);

	*ch_spacing = radio->region_setting.channel_spacing;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

#ifdef USE_GST_PIPELINE
int _mmradio_realize_pipeline(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;

	gst_init(NULL, NULL);
	radio->pGstreamer_s = g_new0(mm_radio_gstreamer_s, 1);

	radio->pGstreamer_s->pipeline = gst_pipeline_new("avsysaudio");

	radio->pGstreamer_s->avsysaudiosrc = gst_element_factory_make("avsysaudiosrc", "fm audio src");
	radio->pGstreamer_s->queue2 = gst_element_factory_make("queue2", "queue2");
	radio->pGstreamer_s->avsysaudiosink = gst_element_factory_make("avsysaudiosink", "audio sink");

	g_object_set(radio->pGstreamer_s->avsysaudiosrc, "latency", 2, NULL);
	g_object_set(radio->pGstreamer_s->avsysaudiosink, "sync", false, NULL);

	if (!radio->pGstreamer_s->pipeline || !radio->pGstreamer_s->avsysaudiosrc || !radio->pGstreamer_s->queue2 || !radio->pGstreamer_s->avsysaudiosink) {
		mmf_debug(MMF_DEBUG_ERROR, "[%s][%05d] One element could not be created. Exiting.\n", __func__, __LINE__);
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	gst_bin_add_many(GST_BIN(radio->pGstreamer_s->pipeline),
	                 radio->pGstreamer_s->avsysaudiosrc,
	                 radio->pGstreamer_s->queue2,
	                 radio->pGstreamer_s->avsysaudiosink,
	                 NULL);
	if (!gst_element_link_many(
	        radio->pGstreamer_s->avsysaudiosrc,
	        radio->pGstreamer_s->queue2,
	        radio->pGstreamer_s->avsysaudiosink,
	        NULL)) {
		mmf_debug(MMF_DEBUG_ERROR, "[%s][%05d] Fail to link b/w appsrc and ffmpeg in rotate\n", __func__, __LINE__);
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}
	return ret;
}

int _mmradio_start_pipeline(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	GstStateChangeReturn ret_state;
	debug_log("\n");

	if (gst_element_set_state(radio->pGstreamer_s->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state(radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug(MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	return ret;
}

int _mmradio_stop_pipeline(mm_radio_t *radio)
{
	int ret = MM_ERROR_NONE;
	GstStateChangeReturn ret_state;

	debug_log("\n");
	if (gst_element_set_state(radio->pGstreamer_s->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state(radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug(MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	return ret;
}

int _mmradio_destroy_pipeline(mm_radio_t  *radio)
{
	int ret = 0;
	GstStateChangeReturn ret_state;
	debug_log("\n");

	if (gst_element_set_state(radio->pGstreamer_s->pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state(radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref(radio->pGstreamer_s->pipeline);
		g_free(radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug(MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	gst_object_unref(radio->pGstreamer_s->pipeline);
	g_free(radio->pGstreamer_s);
	return ret;
}
#endif


static int append_variant(DBusMessageIter *iter, const char *sig, char *param[])
{
	char *ch;
	int i;
	int int_type;
	unsigned long long int64_type;

	if (!sig || !param)
		return 0;

	for (ch = (char *)sig, i = 0; *ch != '\0'; ++i, ++ch) {
		switch (*ch) {
			case 'i':
				int_type = atoi(param[i]);
				dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &int_type);
				break;
			case 'u':
				int_type = atoi(param[i]);
				dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &int_type);
				break;
			case 't':
				int64_type = atoi(param[i]);
				dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &int64_type);
				break;
			case 's':
				dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &param[i]);
				break;
			default:
				return -EINVAL;
		}
	}

	return 0;
}

#define DESTINATION "org.tizen.system.deviced"
#define PATH "/Org/Tizen/System/DeviceD/Bluetooth"
#define INTERFACE "org.tizen.system.deviced.bluetooth"
#define METHOD_TURN_ON "TurnOn"
#define METHOD_TURN_OFF "TurnOff"
#define METHOD_GET_STATE "GetState"
#define BLUETOOTH_STATE_ACTIVE "active"
#define BLUETOOTH_STATE_INACTIVE "inactive"

static int __mmradio_enable_bluetooth(mm_radio_t  *radio, int enable)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;

	int r;
	const char *dest = DESTINATION;
	const char *path = PATH;
	const char *interface = INTERFACE;
	const char *method = enable > 0 ? METHOD_TURN_ON : METHOD_TURN_OFF;
	char *param[1];

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		MMRADIO_LOG_ERROR("dbus_bus_get error");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_INFO("try to send (%s, %s, %s, %s)", dest, path, interface, method);

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		MMRADIO_LOG_ERROR("dbus_message_new_method_call(%s:%s-%s)", path, interface, method);
		return MM_ERROR_RADIO_INTERNAL;
	}
	param[0] = "fmradio";

	dbus_message_iter_init_append(msg, &iter);
	r = append_variant(&iter, "s", param);
	if (r < 0) {
		MMRADIO_LOG_ERROR("append_variant error(%d)", r);
		dbus_message_unref(msg);
		return MM_ERROR_RADIO_INTERNAL;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg,  DBUS_TIMEOUT_USE_DEFAULT, &err);
	if (!reply) {
		MMRADIO_LOG_ERROR("dbus_connection_send error(No reply)");
	}

	if (dbus_error_is_set(&err)) {
		MMRADIO_LOG_ERROR("dbus_connection_send error(%s:%s)", err.name, err.message);
		dbus_message_unref(msg);
		dbus_error_free(&err);
		return MM_ERROR_RADIO_INTERNAL;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	return MM_ERROR_NONE;
}

static int __mmradio_wait_bluetooth_ready(mm_radio_t  *radio, int wait_active, int retry_count)
{
	DBusConnection *conn;
	DBusMessage *msg = NULL;
	DBusMessage *reply = NULL;
	DBusError err;

	int ret = MM_ERROR_RADIO_INTERNAL;
	const char *dest = DESTINATION;
	const char *path = PATH;
	const char *interface = INTERFACE;
	const char *method = METHOD_GET_STATE;
	const int wait = 100 * 1000; /* 100 msec */

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		MMRADIO_LOG_ERROR("dbus_bus_get error");
		goto error;
	}

	MMRADIO_LOG_INFO("try to send (%s, %s, %s, %s)", dest, path, interface, method);

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		MMRADIO_LOG_ERROR("dbus_message_new_method_call(%s:%s-%s)", path, interface, method);
		goto error;
	}

	while (retry_count--) {
		dbus_error_init(&err);
		reply = dbus_connection_send_with_reply_and_block(conn, msg,  DBUS_TIMEOUT_USE_DEFAULT, &err);
		if (!reply) {
			MMRADIO_LOG_ERROR("dbus_connection_send error(No reply)");
			goto error;
		} else {
			const char *state;
			/* if bluetooth is turn on by bt framework, it would be returned "active" */
			if (MM_RADIO_FALSE == dbus_message_get_args(reply, &err, DBUS_TYPE_STRING, &state, DBUS_TYPE_INVALID)) {
				MMRADIO_LOG_ERROR("dbus_message_get_args error(%s:%s)", err.name, err.message);
				goto error;
			}

			if (wait_active == MM_RADIO_TRUE) {
				MMRADIO_LOG_INFO("wait for active state. current state (%s)", state);
				if (strncmp(BLUETOOTH_STATE_ACTIVE, state, strlen(BLUETOOTH_STATE_ACTIVE))) {
					usleep(wait);
					continue;
				} else {
					ret = MM_ERROR_NONE;
					break;
				}
			} else {
				MMRADIO_LOG_INFO("wait for inactive state. current state (%s)", state);
				if (strncmp(BLUETOOTH_STATE_INACTIVE, state, strlen(BLUETOOTH_STATE_INACTIVE))) {
					usleep(wait);
					ret = MM_ERROR_RADIO_INVALID_STATE;
					break;
				} else {
					ret = MM_ERROR_NONE;
					break;
				}
			}
		}
	}


error:
	if (dbus_error_is_set(&err)) {
		MMRADIO_LOG_ERROR("dbus_connection_send error(%s:%s)", err.name, err.message);
		reply = NULL;
	}

	if (msg != NULL)
		dbus_message_unref(msg);
	dbus_error_free(&err);

	return ret;

}


void __mm_radio_init_tuning_params(mm_radio_t *radio)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE_RETURN_VOID( radio );
	int not_found = -1;
	int value = 0;
	dictionary * dict = NULL;

#ifdef TUNING_ENABLE /* for eng binary only */
	dict = iniparser_load(MMFW_RADIO_TUNING_TEMP_FILE);
	if (!dict) {
		MMRADIO_LOG_WARNING("%s load failed. use default  file", MMFW_RADIO_TUNING_TEMP_FILE);
		dict = iniparser_load(MMFW_RADIO_TUNING_DEFUALT_FILE);
		if (!dict) {
			MMRADIO_LOG_ERROR("%s load failed", MMFW_RADIO_TUNING_DEFUALT_FILE);
			return;
		} else {
			MMRADIO_LOG_WARNING("%s is loaded.", MMFW_RADIO_TUNING_DEFUALT_FILE);
		}
	} else {
		MMRADIO_LOG_WARNING("we read temp tuning file. (%s) this means we are on TUNING!", MMFW_RADIO_TUNING_TEMP_FILE);
	}
#else
	dict = iniparser_load(MMFW_RADIO_TUNING_DEFUALT_FILE);
	if (!dict) {
		MMRADIO_LOG_ERROR("%s load failed", MMFW_RADIO_TUNING_DEFUALT_FILE);
		return;
	} else {
		MMRADIO_LOG_INFO("we read default tuning file. (%s)", MMFW_RADIO_TUNING_DEFUALT_FILE);
	}
#endif

#ifdef ENABLE_SPRD /* read sc2331 tuning param */

	value = iniparser_getint(dict, MMFW_RADIO_TUNING_RSSI_THR, not_found);
	if (value == not_found) {
		MMRADIO_LOG_ERROR("Can't get RSSI Threshold Value");
	}
	else {
		radio->radio_tuning.rssi_th = value;
	}
	value = 0;

	value = iniparser_getint(dict, MMFW_RADIO_TUNING_FREQUENCY_OFFSET, not_found);
	if (value == not_found) {
		MMRADIO_LOG_ERROR("Can't get FREQUENCY_OFFSET Value");
	}
	else {
		radio->radio_tuning.freq_offset = value;
	}
	value = 0;

	value = iniparser_getint(dict, MMFW_RADIO_TUNING_NOISE_POWER, not_found);
	if (value == not_found) {
		MMRADIO_LOG_ERROR("Can't get NOISE_POWER Value");
	}
	else {
		radio->radio_tuning.noise_power = value;
	}
	value = 0;

	value = iniparser_getint(dict, MMFW_RADIO_TUNING_PILOT_POWER, not_found);
	if (value == not_found) {
		MMRADIO_LOG_ERROR("Can't get PILOT_POWER Value");
	}
	else {
		radio->radio_tuning.pilot_power = value;
	}
	value = 0;


	/* this is not for tuning, for disable/enable softmute for sc2331 */
	value = iniparser_getint(dict, MMFW_RADIO_TUNING_SOFTMUTE_ENABLE, not_found);
	if (value == not_found) {
		MMRADIO_LOG_ERROR("Can't get SOFTMUTE_ENABLE Value. enable softmute by default.");
		radio->radio_tuning.softmute_enable = 1; /* if we can not find ini, we will just enable softmute by default */
	}
	else {
		radio->radio_tuning.softmute_enable = value;
		MMRADIO_LOG_INFO("SOFTMUTE_ENABLE : %d", radio->radio_tuning.softmute_enable);
	}
	value = 0;

#endif

	/*Cleanup*/
	iniparser_freedict(dict);
	MMRADIO_LOG_FLEAVE();
}

void __mm_radio_apply_tuning_params(mm_radio_t *radio)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE_RETURN_VOID( radio );
	int ret = 0;
	/*we only set if the values have been read correctly*/

#ifdef ENABLE_SPRD
	/* RSSI Threshold*/
	if (radio->radio_tuning.rssi_th) {
		ret = _mm_set_tuning_value(FMRX_RSSI_LEVEL_THRESHOLD, radio->radio_tuning.rssi_th);
		if (ret) {
			MMRADIO_LOG_ERROR("_mm_set_tuning_value failed with error = %d", ret);
		}
	}

	if (radio->radio_tuning.freq_offset) {
		ret = _mm_set_tuning_value(FMRX_TUNING_FREQ_OFFSET, radio->radio_tuning.freq_offset);
		if (ret) {
			MMRADIO_LOG_ERROR("_mm_set_tuning_value failed with error = %d", ret);
		}
	}

	if (radio->radio_tuning.noise_power) {
		ret = _mm_set_tuning_value(FMRX_TUNING_NOISE_POWER, radio->radio_tuning.noise_power);
		if (ret) {
			MMRADIO_LOG_ERROR("_mm_set_tuning_value failed with error = %d", ret);
		}
	}

	if (radio->radio_tuning.pilot_power) {
		ret = _mm_set_tuning_value(FMRX_TUNING_PILOT_POWER, radio->radio_tuning.pilot_power);
		if (ret) {
			MMRADIO_LOG_ERROR("_mm_set_tuning_value failed with error = %d", ret);
		}
	}

	MMRADIO_LOG_INFO("softmute is %s. set sysfs", radio->radio_tuning.softmute_enable ? "Enabled" : "Disabled");
	ret = _mm_set_tuning_value(FMRX_TUNING_ENABLE_DISABLE_SOFTMUTE, radio->radio_tuning.softmute_enable);
	if (ret) {
		MMRADIO_LOG_ERROR("_mm_set_tuning_value(softmute enable) failed with error = %d", ret);
	}
#endif

	MMRADIO_LOG_FLEAVE();
}

int _mm_set_tuning_value(char* file_name, int value)
{
#ifdef ENABLE_FM_TUNING
	FILE *tuning_file = NULL;
	tuning_file = fopen(file_name, "w");
	if (!tuning_file) {
		MMRADIO_LOG_ERROR("could not open file %s with error: %s (%d)", file_name, strerror(errno), errno);
		return errno;
	}
	fprintf(tuning_file, "%d", value);
	MMRADIO_LOG_INFO("value %s set to= %d", file_name, value);

	fclose(tuning_file);
	tuning_file = NULL;
#endif
	return 0;
}

int _mm_radio_load_volume_table(int **volume_table, int *number_of_elements)
{
	dictionary *dict = NULL;
	const char delimiter[] = ", ";
	char *ptr = NULL;
	char *token = NULL;
	char *list_str = NULL;
	int *temp_table = NULL;
	int index = 0;
	int ret = 0;

	bool tuning_enable = MM_RADIO_FALSE;
	int not_found = -1;
	int value = 0;

	dict = iniparser_load(MMFW_RADIO_TUNING_DEFUALT_FILE);
	if (dict == NULL) {
		MMRADIO_LOG_ERROR("%s load failed", MMFW_RADIO_TUNING_DEFUALT_FILE);
		return MM_ERROR_RADIO_INTERNAL;
	} else {
		/*tuning enable */
		value = iniparser_getboolean(dict, MMFW_RADIO_TUNING_ENABLE, not_found);
		if (value == not_found) {
			MMRADIO_LOG_ERROR("Can't get Tuning Enable value");
		} else {
			tuning_enable = value;
			MMRADIO_LOG_INFO("Tuning enabled.");
		}

		iniparser_freedict(dict); /*Cleanup*/
	}


	if (tuning_enable == MM_RADIO_TRUE) {
		dict = iniparser_load(MMFW_RADIO_TUNING_TEMP_FILE);
		if (!dict) {
			MMRADIO_LOG_WARNING("%s load failed. Use temporary file", MMFW_RADIO_TUNING_TEMP_FILE);
			dict = iniparser_load(MMFW_RADIO_TUNING_DEFUALT_FILE);
			if (!dict) {
				MMRADIO_LOG_ERROR("%s load failed", MMFW_RADIO_TUNING_DEFUALT_FILE);
				return MM_ERROR_RADIO_INTERNAL;
			}
		}
	} else {
		dict = iniparser_load(MMFW_RADIO_TUNING_DEFUALT_FILE);
		if (!dict) {
			MMRADIO_LOG_ERROR("%s load failed", MMFW_RADIO_TUNING_DEFUALT_FILE);
			return MM_ERROR_RADIO_INTERNAL;
		}
	}

	*number_of_elements = iniparser_getint(dict, MMFW_RADIO_TUNING_VOLUME_LEVELS, -1);
	if (*number_of_elements == -1) {
		ret = MM_ERROR_INVALID_ARGUMENT;
		goto error;
	}
	temp_table = (int *)malloc((*number_of_elements) * sizeof(int));
	if (!temp_table) {
		goto error;
	}
	*volume_table = temp_table;

	list_str = iniparser_getstr(dict, MMFW_RADIO_TUNING_VOLUME_TABLE);
	if (list_str) {
		token = strtok_r(list_str, delimiter, &ptr);
		while (token) {
			temp_table[index] = atoi(token);
			MMRADIO_LOG_INFO("fm volume index %d is %d", index, temp_table[index]);
			index++;
			token = strtok_r(NULL, delimiter, &ptr);
		}
	}
error:
	iniparser_freedict(dict);
	return ret;
}

int _mmradio_get_device_available(mm_radio_t *radio , bool *is_connected)
{
	mm_sound_device_flags_e flags = MM_SOUND_DEVICE_STATE_ACTIVATED_FLAG;
	MMSoundDeviceList_t device_list;
	MMSoundDevice_t device_h = NULL;
	mm_sound_device_type_e device_type;
	mm_sound_device_type_e current_device = MM_SOUND_DEVICE_TYPE_BUILTIN_SPEAKER;
	int ret = MM_ERROR_NONE;

	MMRADIO_LOG_FENTER();

	/* get status if speaker is activated */
	/* (1) get current device list */
	ret = mm_sound_get_current_device_list(flags, &device_list);
	if (ret) {
		MMRADIO_LOG_FLEAVE();
		MMRADIO_LOG_DEBUG("mm_sound_get_current_device_list() failed [%x]!!", ret);
		return MM_ERROR_RADIO_NO_ANTENNA;
	}

	while (current_device <= MM_SOUND_DEVICE_TYPE_USB_AUDIO) {
		/* (2) get device handle of device list */
		ret = mm_sound_get_next_device(device_list, &device_h);

		if (ret) {
			MMRADIO_LOG_DEBUG("mm_sound_get_next_device() failed [%x]!!", ret);
			MMRADIO_LOG_FLEAVE();
			return MM_ERROR_RADIO_NO_ANTENNA;
		}

		/* (3) get device type */
		ret = mm_sound_get_device_type(device_h, &device_type);

		if (ret) {
			MMRADIO_LOG_DEBUG("mm_sound_get_device_type() failed [%x]!!", ret);
			MMRADIO_LOG_FLEAVE();
			return MM_ERROR_RADIO_NO_ANTENNA;
		}

		MMRADIO_LOG_DEBUG("device_type [%d]!!", device_type);
		if (device_type == MM_SOUND_DEVICE_TYPE_AUDIOJACK) {
			*is_connected = TRUE;
			return MM_ERROR_NONE;
		}

		current_device++;
	}

	MMRADIO_LOG_DEBUG("ret [%d] is_connected : %d!!", ret, *is_connected);
	MMRADIO_LOG_FLEAVE();

	return ret;

}

