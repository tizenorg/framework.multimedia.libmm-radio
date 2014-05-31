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
#include <mm_ta.h>

#include <mm_error.h>
#include <mm_debug.h>
#include <mm_message.h>

#include "mm_radio_priv.h"

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

#define FREQ_FRAC				16
#define RADIO_FREQ_FORMAT_SET(x_freq)		((x_freq) * FREQ_FRAC)
#define RADIO_FREQ_FORMAT_GET(x_freq)		((x_freq) / FREQ_FRAC)
#define DEFAULT_WRAP_AROUND 			1 //If non-zero, wrap around when at the end of the frequency range, else stop seeking

#define RADIO_DEFAULT_REGION			MM_RADIO_REGION_GROUP_USA

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
static const MMRadioRegion_t region_table[] =
{
		{	/* Notrh America, South America, South Korea, Taiwan, Australia */
			MM_RADIO_REGION_GROUP_USA,	// region type
			MM_RADIO_DEEMPHASIS_75_US,	// de-emphasis
			MM_RADIO_FREQ_MIN_87500_KHZ, 	// min freq.
			MM_RADIO_FREQ_MAX_108000_KHZ,	// max freq.
		},
		{	/* China, Europe, Africa, Middle East, Hong Kong, India, Indonesia, Russia, Singapore */
			MM_RADIO_REGION_GROUP_EUROPE,
			MM_RADIO_DEEMPHASIS_50_US,
			MM_RADIO_FREQ_MIN_87500_KHZ,
			MM_RADIO_FREQ_MAX_108000_KHZ,
		},
		{
			MM_RADIO_REGION_GROUP_JAPAN,
			MM_RADIO_DEEMPHASIS_50_US,
			MM_RADIO_FREQ_MIN_76100_KHZ,
			MM_RADIO_FREQ_MAX_89900_KHZ,
		},
};
/*---------------------------------------------------------------------------
    LOCAL FUNCTION PROTOTYPES:
---------------------------------------------------------------------------*/
static bool	__mmradio_post_message(mm_radio_t* radio, enum MMMessageType msgtype, MMMessageParamType* param);
static int  		__mmradio_check_state(mm_radio_t* radio, MMRadioCommand command);
static int		__mmradio_get_state(mm_radio_t* radio);
static bool	__mmradio_set_state(mm_radio_t* radio, int new_state);
static void 	__mmradio_seek_thread(mm_radio_t* radio);
static void	__mmradio_scan_thread(mm_radio_t* radio);
ASM_cb_result_t	__mmradio_asm_callback(int handle, ASM_event_sources_t sound_event, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data);
static bool 	__is_tunable_frequency(mm_radio_t* radio, int freq);
static int 		__mmradio_set_deemphasis(mm_radio_t* radio);
static int 		__mmradio_set_band_range(mm_radio_t* radio);

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
int
_mmradio_apply_region(mm_radio_t* radio, MMRadioRegionType region, bool update)
{
	int ret = MM_ERROR_NONE;
	int count = 0;
	int index = 0;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_SET_REGION );

	/* if needed, radio region must be updated.
	  * Otherwise, just applying settings to device without it.
	  */
	if ( update )
	{
		count = ARRAY_SIZE(region_table);

		//TODO: if auto is supported...get the region info. here

		/* update radio region settings */
		for ( index = 0; index < count; index++ )
		{
			/* find the region from pre-defined table*/
			if (region_table[index].country == region)
			{
				radio->region_setting.country = region_table[index].country;
				radio->region_setting.deemphasis = region_table[index].deemphasis;
				radio->region_setting.band_min = region_table[index].band_min;
				radio->region_setting.band_max = region_table[index].band_max;
			}
		}
	}

	/* chech device is opened or not. if it's not ready, skip to apply region to device now*/
	if (radio->radio_fd < 0)
	{
		MMRADIO_LOG_DEBUG("not opened device. just updating region info. \n");
		return MM_ERROR_NONE;
	}

	MMRADIO_LOG_DEBUG("setting region - country: %d, de-emphasis: %d, band range: %d ~ %d KHz\n",
		radio->region_setting.country, radio->region_setting.deemphasis, radio->region_setting.band_min, radio->region_setting.band_max);

	/* set de-emphsasis to device */
	ret = __mmradio_set_deemphasis(radio);

	MMRADIO_CHECK_RETURN_IF_FAIL(ret, "set de-emphasis");

	/* set band range to device */
	ret  = __mmradio_set_band_range(radio);

	MMRADIO_CHECK_RETURN_IF_FAIL(ret, "set band range");

	MMRADIO_LOG_FLEAVE();

	return ret;
}

int
_mmradio_create_radio(mm_radio_t* radio)
{
	int ret  = 0;

	MMRADIO_LOG_FENTER();
		
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_CREATE );

	/* set default value */
	radio->radio_fd = -1;
	radio->freq = DEFAULT_FREQ;
	memset(&radio->region_setting, 0, sizeof(MMRadioRegion_t));

	/* create command lock */
	ret = pthread_mutex_init( &radio->cmd_lock, NULL );
	if ( ret )
	{
		MMRADIO_LOG_ERROR("mutex creation failed\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_NULL );

	/* register to ASM */
	ret = mmradio_asm_register(&radio->sm, __mmradio_asm_callback, (void*)radio);
	if ( ret )
	{
		/* NOTE : we are dealing it as an error since we cannot expect it's behavior */
		MMRADIO_LOG_ERROR("failed to register asm server\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_realize(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_REALIZE );

	/* open radio device */
	if(radio->radio_fd == -1)
	{
		MMRadioRegionType region = MM_RADIO_REGION_GROUP_NONE;
		bool update = false;

		/* open device */
		radio->radio_fd = open(DEFAULT_DEVICE, O_RDONLY);
		if (radio->radio_fd < 0)
		{
			MMRADIO_LOG_ERROR("failed to open radio device[%s] because of %s(%d)\n",
						DEFAULT_DEVICE, strerror(errno), errno);
			
			/* check error */
			switch (errno)
			{
				case ENOENT:
					return MM_ERROR_RADIO_DEVICE_NOT_FOUND;
				case EACCES:					
					return MM_ERROR_RADIO_PERMISSION_DENIED;
				default:
					return MM_ERROR_RADIO_DEVICE_NOT_OPENED;
			}
		}
		MMRADIO_LOG_DEBUG("radio device fd : %d\n", radio->radio_fd);

		/* query radio device capabilities. */
		if (ioctl(radio->radio_fd, VIDIOC_QUERYCAP, &(radio->vc)) < 0)
		{
			MMRADIO_LOG_ERROR("VIDIOC_QUERYCAP failed!\n");
			goto error;
		}

		if ( ! ( radio->vc.capabilities & V4L2_CAP_TUNER) )
		{
			MMRADIO_LOG_ERROR("this system can't support fm-radio!\n");
			goto error;
		}

		/* set tuner audio mode */
		ioctl(radio->radio_fd, VIDIOC_G_TUNER, &(radio->vt));

		if ( ! ( (radio->vt).capability & V4L2_TUNER_CAP_STEREO) )
		{
			MMRADIO_LOG_ERROR("this system can support mono!\n");
			(radio->vt).audmode = V4L2_TUNER_MODE_MONO;
		}
		else
		{
			(radio->vt).audmode = V4L2_TUNER_MODE_STEREO;
		}

		/* set tuner index. Must be 0. */
		(radio->vt).index = TUNER_INDEX;
		ioctl(radio->radio_fd, VIDIOC_S_TUNER, &(radio->vt));

		/* check region country type if it's updated or not */
		if ( radio->region_setting.country == MM_RADIO_REGION_GROUP_NONE)
		{
			/* not initialized  yet. set it with default region */
			region = RADIO_DEFAULT_REGION;
			update = true;
		}
		else // already initialized by application
		{
			region = radio->region_setting.country;
		}

		ret = _mmradio_apply_region(radio, region, update);

		MMRADIO_CHECK_RETURN_IF_FAIL(ret, "update region info");
	}

	/* ready but nosound */
	if( _mmradio_mute(radio) != MM_ERROR_NONE)
		goto error;

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_READY );
#ifdef USE_GST_PIPELINE
	ret = _mmradio_realize_pipeline(radio);
	if ( ret ) {
		debug_error("_mmradio_realize_pipeline is failed\n");
		return ret;
	}
#endif
	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;

error:
	if (radio->radio_fd >= 0)
	{
		close(radio->radio_fd);
		radio->radio_fd = -1;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_RADIO_INTERNAL;
}

int
_mmradio_unrealize(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;
	
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_UNREALIZE );

	if( _mmradio_mute(radio) != MM_ERROR_NONE)
		return MM_ERROR_RADIO_NOT_INITIALIZED;

	/* close radio device here !!!! */
	if (radio->radio_fd >= 0)
	{
		close(radio->radio_fd);
		radio->radio_fd = -1;
	}

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_NULL );
#ifdef USE_GST_PIPELINE
	ret= _mmradio_destroy_pipeline(radio);
	if ( ret ) {
		debug_error("_mmradio_destroy_pipeline is failed\n");
		return ret;
	}
#endif

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_destroy(mm_radio_t* radio)
{
	int ret = 0;
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_DESTROY );
	
	ret = mmradio_asm_deregister(&radio->sm);
	if ( ret )
	{
		MMRADIO_LOG_ERROR("failed to deregister asm server\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	_mmradio_unrealize( radio );

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}


int
_mmradio_set_frequency(mm_radio_t* radio, int freq) // unit should be KHz
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_SET_FREQ );

	MMRADIO_LOG_DEBUG("Setting %d frequency\n", freq);

	radio->freq = freq;

	if (radio->radio_fd < 0)
	{
		MMRADIO_LOG_DEBUG("radio device is not opened yet. so, it will be applied\n", freq);
		return MM_ERROR_NONE;
	}

	/* check frequency range */
	if ( freq < radio->region_setting.band_min
		|| freq > radio->region_setting.band_max )
	{
		MMRADIO_LOG_ERROR("out of frequency range\n", freq);
		return MM_ERROR_INVALID_ARGUMENT;
	}

	/* set it */
	(radio->vf).tuner = 0;
	(radio->vf).frequency = RADIO_FREQ_FORMAT_SET(freq);

	if(ioctl(radio->radio_fd, VIDIOC_S_FREQUENCY, &(radio->vf))< 0)
	{
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	MMRADIO_LOG_FLEAVE();
	
	return MM_ERROR_NONE;
	
}

int
_mmradio_get_frequency(mm_radio_t* radio, int* pFreq)
{
	int freq = 0;
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_GET_FREQ );

	return_val_if_fail( pFreq, MM_ERROR_INVALID_ARGUMENT );

	/* just return stored frequency if radio device is not ready */
	if ( radio->radio_fd < 0 )
	{
		MMRADIO_LOG_DEBUG("freq : %d\n", radio->freq);
		*pFreq = radio->freq;
		return MM_ERROR_NONE;
	}

	if (ioctl(radio->radio_fd, VIDIOC_G_FREQUENCY, &(radio->vf)) < 0)
	{
		MMRADIO_LOG_ERROR("failed to do VIDIOC_G_FREQUENCY\n");
		return MM_ERROR_RADIO_INTERNAL;
	}	

	freq = RADIO_FREQ_FORMAT_GET((radio->vf).frequency);

	/* update freq in handle */
	radio->freq = freq;

	*pFreq = radio->freq;

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_mute(mm_radio_t* radio)
{
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_MUTE );

	if (radio->radio_fd < 0)
	{
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}	

	(radio->vctrl).id = V4L2_CID_AUDIO_MUTE;
	(radio->vctrl).value = 1; //mute

	if (ioctl(radio->radio_fd, VIDIOC_S_CTRL, &(radio->vctrl)) < 0)
	{
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	MMRADIO_LOG_FLEAVE();
	
	return MM_ERROR_NONE;
	
}

int
_mmradio_unmute(mm_radio_t* radio)
{
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_UNMUTE );
	MMRADIO_CHECK_DEVICE_STATE( radio );

	(radio->vctrl).id = V4L2_CID_AUDIO_MUTE;
	(radio->vctrl).value = 0; //unmute

	if (ioctl(radio->radio_fd, VIDIOC_S_CTRL, &(radio->vctrl)) < 0)
	{
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

/* --------------------------------------------------------------------------
 * Name   : __mmradio_set_deemphasis
 * Desc   : apply de-emphasis value to device
 * Param  :
 *	    [in] radio : radio handle
 * Return : zero on success, or negative value with error code
 *---------------------------------------------------------------------------*/
int
__mmradio_set_deemphasis(mm_radio_t* radio)
{
	int value = 0;
	
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );

	/* get de-emphasis */
	switch (radio->region_setting.deemphasis)
	{
		case MM_RADIO_DEEMPHASIS_50_US:
			value = 1;//V4L2_DEEMPHASIS_50_uS;
			break;

		case MM_RADIO_DEEMPHASIS_75_US:
			value = 2;//V4L2_DEEMPHASIS_75_uS;
			break;

		default:
			MMRADIO_LOG_ERROR("not availabe de-emphasis value\n");
			return MM_ERROR_COMMON_INVALID_ARGUMENT;
	}

	/* set it to device */
	(radio->vctrl).id = (0x009d0000 | 0x900) +1;//V4L2_CID_TUNE_DEEMPHASIS;
	(radio->vctrl).value = value;

	if (ioctl(radio->radio_fd, VIDIOC_S_CTRL, &(radio->vctrl)) < 0)
	{
		MMRADIO_LOG_ERROR("failed to set de-emphasis\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

/* --------------------------------------------------------------------------
 * Name   : __mmradio_set_band_range
 * Desc   : apply max and min frequency to device
 * Param  :
 *	    [in] radio : radio handle
 * Return : zero on success, or negative value with error code
 *---------------------------------------------------------------------------*/
int
__mmradio_set_band_range(mm_radio_t* radio)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );

	/* get min and max freq. */
	(radio->vt).rangelow = RADIO_FREQ_FORMAT_SET(radio->region_setting.band_min);
	(radio->vt).rangehigh = RADIO_FREQ_FORMAT_SET(radio->region_setting.band_max);

	/* set it to device */
	if (ioctl(radio->radio_fd, VIDIOC_S_TUNER, &(radio->vt)) < 0 )
	{
		MMRADIO_LOG_ERROR("failed to set band range\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_set_message_callback(mm_radio_t* radio, MMMessageCallback callback, void *user_param)
{
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );

	radio->msg_cb = callback;
	radio->msg_cb_param = user_param;

	MMRADIO_LOG_DEBUG("msg_cb : 0x%p msg_cb_param : 0x%p\n", callback, user_param);

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}
		
int
_mmradio_get_state(mm_radio_t* radio, int* pState)
{
	int state = 0;

	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	return_val_if_fail( pState, MM_ERROR_INVALID_ARGUMENT );
		
	state = __mmradio_get_state( radio );

	*pState = state;

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_start(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;
	
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_START );

	MMRADIO_LOG_DEBUG("now tune to frequency : %d\n", radio->freq);

	ret = mmradio_asm_set_state(&radio->sm, ASM_STATE_PLAYING, ASM_RESOURCE_RADIO_TUNNER);
	if ( ret )
	{
		MMRADIO_LOG_ERROR("failed to set asm state to PLAYING\n");
		return ret;
	}

	/* set stored frequency */
	_mmradio_set_frequency( radio, radio->freq );

	/* unmute */
	if( _mmradio_unmute(radio) != MM_ERROR_NONE)
		return MM_ERROR_RADIO_NOT_INITIALIZED;

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_PLAYING );
#ifdef USE_GST_PIPELINE
	ret = _mmradio_start_pipeline( radio );
	if ( ret ) {
		debug_error("_mmradio_start_pipeline is failed\n");
		return ret;
	}
#endif

	MMRADIO_LOG_FLEAVE();

	return ret;
}

int
_mmradio_stop(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;

	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_STOP );

	if( _mmradio_mute(radio) != MM_ERROR_NONE)
		return MM_ERROR_RADIO_NOT_INITIALIZED;

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_READY );

	ret = mmradio_asm_set_state(&radio->sm, ASM_STATE_STOP, ASM_RESOURCE_NONE);
	if ( ret )
	{
		MMRADIO_LOG_ERROR("failed to set asm state to PLAYING\n");
		return ret;
	}
#ifdef USE_GST_PIPELINE
	ret= _mmradio_stop_pipeline( radio );
	if ( ret ) {
		debug_error("_mmradio_stop_pipeline is failed\n");
		return ret;
	}
#endif

	MMRADIO_LOG_FLEAVE();

	return ret;
}

#ifdef USE_GST_PIPELINE
int
_mmradio_realize_pipeline(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;

	gst_init (NULL, NULL);
	radio->pGstreamer_s = g_new0 (mm_radio_gstreamer_s, 1);

	radio->pGstreamer_s->pipeline= gst_pipeline_new ("avsysaudio");

	radio->pGstreamer_s->avsysaudiosrc= gst_element_factory_make("avsysaudiosrc","fm audio src");
	radio->pGstreamer_s->queue2= gst_element_factory_make("queue2","queue2");
	radio->pGstreamer_s->avsysaudiosink= gst_element_factory_make("avsysaudiosink","audio sink");

	g_object_set(radio->pGstreamer_s->avsysaudiosrc, "latency", 2, NULL);
	g_object_set(radio->pGstreamer_s->avsysaudiosink, "sync", false, NULL);

	if (!radio->pGstreamer_s->pipeline || !radio->pGstreamer_s->avsysaudiosrc || !radio->pGstreamer_s->queue2 || !radio->pGstreamer_s->avsysaudiosink) {
		mmf_debug(MMF_DEBUG_ERROR,"[%s][%05d] One element could not be created. Exiting.\n", __func__, __LINE__);
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	gst_bin_add_many(GST_BIN(radio->pGstreamer_s->pipeline),
			radio->pGstreamer_s->avsysaudiosrc,
			radio->pGstreamer_s->queue2,
			radio->pGstreamer_s->avsysaudiosink,
			NULL);
	if(!gst_element_link_many(
			radio->pGstreamer_s->avsysaudiosrc,
			radio->pGstreamer_s->queue2,
			radio->pGstreamer_s->avsysaudiosink,
			NULL)) {
		mmf_debug(MMF_DEBUG_ERROR,"[%s][%05d] Fail to link b/w appsrc and ffmpeg in rotate\n", __func__, __LINE__);
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}
	return ret;
}

int
_mmradio_start_pipeline(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;
	GstStateChangeReturn ret_state;
	debug_log("\n");

	if(gst_element_set_state (radio->pGstreamer_s->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state (radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug (MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	return ret;
}

int
_mmradio_stop_pipeline(mm_radio_t* radio)
{
	int ret = MM_ERROR_NONE;
	GstStateChangeReturn ret_state;

	debug_log("\n");
	if(gst_element_set_state (radio->pGstreamer_s->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state (radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug (MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	return ret;
}

int
_mmradio_destroy_pipeline(mm_radio_t * radio)
{
	int ret = 0;
	GstStateChangeReturn ret_state;
	debug_log("\n");

	if(gst_element_set_state (radio->pGstreamer_s->pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "Fail to change pipeline state");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	}

	ret_state = gst_element_get_state (radio->pGstreamer_s->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
	if (ret_state == GST_STATE_CHANGE_FAILURE) {
		mmf_debug(MMF_DEBUG_ERROR, "GST_STATE_CHANGE_FAILURE");
		gst_object_unref (radio->pGstreamer_s->pipeline);
		g_free (radio->pGstreamer_s);
		return MM_ERROR_RADIO_INVALID_STATE;
	} else {
		mmf_debug (MMF_DEBUG_LOG, "[%s][%05d] GST_STATE_NULL ret_state = %d (GST_STATE_CHANGE_SUCCESS)\n", __func__, __LINE__, ret_state);
	}
	gst_object_unref (radio->pGstreamer_s->pipeline);
	g_free (radio->pGstreamer_s);
	return ret;
}
#endif

int
_mmradio_seek(mm_radio_t* radio, MMRadioSeekDirectionType direction)
{
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_SEEK );

 	int ret = 0;
 	
	if( _mmradio_mute(radio) != MM_ERROR_NONE)
		return MM_ERROR_RADIO_NOT_INITIALIZED;
		
	MMRADIO_LOG_DEBUG("trying to seek. direction[0:UP/1:DOWN) %d\n", direction);
	radio->seek_direction = direction;

	ret = pthread_create(&radio->seek_thread, NULL,
		(void *)__mmradio_seek_thread, (void *)radio);

	if ( ret )
	{
		MMRADIO_LOG_DEBUG("failed create thread\n");
		return MM_ERROR_RADIO_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_start_scan(mm_radio_t* radio)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_START_SCAN );

	int scan_tr_id = 0;
 	
	radio->stop_scan = false;

	scan_tr_id = pthread_create(&radio->scan_thread, NULL,
		(void *)__mmradio_scan_thread, (void *)radio);

	if (scan_tr_id != 0) 
	{
		MMRADIO_LOG_DEBUG("failed to create thread : scan\n");
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_SCANNING );

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int
_mmradio_stop_scan(mm_radio_t* radio)
{
	MMRADIO_LOG_FENTER();
	
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_STOP_SCAN );
		
	radio->stop_scan = true;

 	if( radio->scan_thread > 0 )
	{	
		pthread_cancel(radio->scan_thread);
		pthread_join(radio->scan_thread, NULL);
	 	radio->scan_thread = 0;
	}

 	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_READY );
	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_STOP, NULL);

	MMRADIO_LOG_FLEAVE();
	
	return MM_ERROR_NONE;
}

void
__mmradio_scan_thread(mm_radio_t* radio)
{
	int ret = 0;
	int prev_freq = 0;
	struct v4l2_hw_freq_seek vs = {0,};
	vs.tuner = TUNER_INDEX;
	vs.type = V4L2_TUNER_RADIO;
	vs.wrap_around = 0; /* around:1 not around:0 */
	vs.seek_upward = 1; /* up : 1	------- down : 0 */

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );

	if( _mmradio_mute(radio) != MM_ERROR_NONE)
		goto FINISHED;

	if( _mmradio_set_frequency(radio, radio->region_setting.band_min) != MM_ERROR_NONE)
		goto FINISHED;

	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_START, NULL);
	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_SCANNING );

	while( ! radio->stop_scan )
	{
		int freq = 0;
		MMMessageParamType param = {0,};

		MMRADIO_LOG_DEBUG("scanning....\n");
		ret = ioctl(radio->radio_fd, VIDIOC_S_HW_FREQ_SEEK, &vs);

		if( ret == -1 )
		{
			if ( errno == EAGAIN )
			{
				MMRADIO_LOG_ERROR("scanning timeout\n");
				continue;
			}
			else if ( errno == EINVAL )
			{
				MMRADIO_LOG_ERROR("The tuner index is out of bounds or the value in the type field is wrong.");
				break;
			}
			else
			{
				MMRADIO_LOG_ERROR("Error: %s, %d\n", strerror(errno), errno);
				break;
			}
		}

		/* now we can get new frequency from radio device */

		if ( radio->stop_scan ) break;

		ret = _mmradio_get_frequency(radio, &freq);
		if ( ret )
		{
			MMRADIO_LOG_ERROR("failed to get current frequency\n");
		}
		else
		{
			if ( freq < prev_freq )
			{
				MMRADIO_LOG_DEBUG("scanning wrapped around. stopping scan\n");
				break;
			}

			if ( freq == prev_freq)
				continue;

			prev_freq = param.radio_scan.frequency = freq;
			MMRADIO_LOG_DEBUG("scanning : new frequency : [%d]\n", param.radio_scan.frequency);
			
			/* drop if max freq is scanned */
			if (param.radio_scan.frequency == radio->region_setting.band_max )
			{
				MMRADIO_LOG_DEBUG("%d freq is dropping...and stopping scan\n", param.radio_scan.frequency);
				break;
			}

			if ( radio->stop_scan ) break; // doesn't need to post

			MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_INFO, &param);
		}
	}
FINISHED:
	radio->scan_thread = 0;
	
	MMRADIO_SET_STATE( radio, MM_RADIO_STATE_READY );

	if ( ! radio->stop_scan )
	{
		MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SCAN_FINISH, NULL);
	}

	MMRADIO_LOG_FLEAVE();

	pthread_exit(NULL);
}

bool 
__is_tunable_frequency(mm_radio_t* radio, int freq)
{
	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );

	if ( freq == radio->region_setting.band_max|| freq == radio->region_setting.band_min )
		return false;

	MMRADIO_LOG_FLEAVE();

	return true;
}

void
__mmradio_seek_thread(mm_radio_t* radio)
{
	int ret = 0;
	int freq = 0;
	bool seek_stop = false;
	MMMessageParamType param = {0,};
	struct v4l2_hw_freq_seek vs = {0,};

	vs.tuner = TUNER_INDEX;
	vs.type = V4L2_TUNER_RADIO;
	vs.wrap_around = DEFAULT_WRAP_AROUND;

	MMRADIO_LOG_FENTER();

	MMRADIO_CHECK_INSTANCE( radio );

	/* check direction */
	switch( radio->seek_direction )
	{
		case MM_RADIO_SEEK_UP:
			vs.seek_upward = 1;
			break;
		default:
			vs.seek_upward = 0;
			break;
	}

	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_START, NULL);

	MMRADIO_LOG_DEBUG("seeking....\n");
	
	while (  ! seek_stop )
	{	
		ret = ioctl( radio->radio_fd, VIDIOC_S_HW_FREQ_SEEK, &vs );

		if( ret == -1 )
		{
			if ( errno == EAGAIN )
			{
				/* FIXIT : we need retrying code here */
				MMRADIO_LOG_ERROR("scanning timeout\n");
				goto SEEK_FAILED;
			}
			else if ( errno == EINVAL )
			{
				MMRADIO_LOG_ERROR("The tuner index is out of bounds or the value in the type field is wrong.");
				goto SEEK_FAILED;
			}
			else
			{
				MMRADIO_LOG_ERROR("Error: %s, %d\n", strerror(errno), errno);
				goto SEEK_FAILED;
			}
		}

		/* now we can get new frequency from radio device */
		ret = _mmradio_get_frequency(radio, &freq);
		if ( ret )
		{
			MMRADIO_LOG_ERROR("failed to get current frequency\n");
			goto SEEK_FAILED;
		}

		MMRADIO_LOG_DEBUG("found frequency = %d\n", freq);

		/* if same freq is found, ignore it and search next one. */
		if ( freq == radio->prev_seek_freq )
		{
			MMRADIO_LOG_DEBUG("It's same with previous one(%d). So, trying next one. \n", freq);
			continue;
		}

		if ( __is_tunable_frequency(radio, freq) ) // check if it's limit freq or not
		{
			/* now tune to new frequency */
			ret = _mmradio_set_frequency(radio, freq);
			if ( ret )
			{
				MMRADIO_LOG_ERROR("failed to tune to new frequency\n");
				goto SEEK_FAILED;
			}
		}

		/* now turn on radio 
		  * In the case of limit freq, tuner should be unmuted. 
		  * Otherwise, sound can't output even though application set new frequency. 
		  */
		ret = _mmradio_unmute(radio);
		if ( ret )
		{
			MMRADIO_LOG_ERROR("failed to tune to new frequency\n");
			goto SEEK_FAILED;
		}

		param.radio_scan.frequency = radio->prev_seek_freq = freq;
		MMRADIO_LOG_DEBUG("seeking : new frequency : [%d]\n", param.radio_scan.frequency);
		MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_FINISH, &param);
		seek_stop = true;
	}

	radio->seek_thread = 0;

	MMRADIO_LOG_FLEAVE();

	pthread_exit(NULL);
	return;

SEEK_FAILED:
	/* freq -1 means it's failed to seek */
	param.radio_scan.frequency = -1;
	MMRADIO_POST_MSG(radio, MM_MESSAGE_RADIO_SEEK_FINISH, &param);
	pthread_exit(NULL);
}

static bool
__mmradio_post_message(mm_radio_t* radio, enum MMMessageType msgtype, MMMessageParamType* param)
{
	MMRADIO_CHECK_INSTANCE( radio );

	MMRADIO_LOG_FENTER();

	if ( !radio->msg_cb )
	{
		debug_warning("failed to post a message\n");
		return false;
	}

	MMRADIO_LOG_DEBUG("address of msg_cb : %d\n", radio->msg_cb);

	radio->msg_cb(msgtype, param, radio->msg_cb_param);

	MMRADIO_LOG_FLEAVE();

	return true;
}

static int
 __mmradio_check_state(mm_radio_t* radio, MMRadioCommand command)
 {
	 MMRadioStateType radio_state = MM_RADIO_STATE_NUM;

	MMRADIO_LOG_FENTER();

 	MMRADIO_CHECK_INSTANCE( radio );

 	radio_state = __mmradio_get_state( radio );

 	MMRADIO_LOG_DEBUG("incomming command : %d  current state : %d\n", command, radio_state);

 	switch( command )
 	{
 		case MMRADIO_COMMAND_CREATE:
 		{
 			if ( radio_state != 0 )
 				goto NO_OP;
 		}
 		break;

 		case MMRADIO_COMMAND_REALIZE:
 		{
			if ( radio_state == MM_RADIO_STATE_READY ||
					radio_state == MM_RADIO_STATE_PLAYING ||
					radio_state == MM_RADIO_STATE_SCANNING )
				goto NO_OP;

			if ( radio_state == 0 )
				goto INVALID_STATE;
 		}
 		break;

 		case MMRADIO_COMMAND_UNREALIZE:
 		{
			if ( radio_state == MM_RADIO_STATE_NULL )
				goto NO_OP;

			/* we can call unrealize at any higher state */
 		}
 		break;

 		case MMRADIO_COMMAND_START:
 		{
 			if ( radio_state == MM_RADIO_STATE_PLAYING )
 				goto NO_OP;

 			if ( radio_state != MM_RADIO_STATE_READY )
 				goto INVALID_STATE;
 		}
 		break;

 		case MMRADIO_COMMAND_STOP:
 		{
 			if ( radio_state == MM_RADIO_STATE_READY )
 				goto NO_OP;

 			if ( radio_state != MM_RADIO_STATE_PLAYING )
 				goto INVALID_STATE;
 		}
 		break;

 		case MMRADIO_COMMAND_START_SCAN:
 		{
 			if ( radio_state == MM_RADIO_STATE_SCANNING )
 				goto NO_OP;

 			if ( radio_state != MM_RADIO_STATE_READY )
 				goto INVALID_STATE;
 		}
 		break;

 		case MMRADIO_COMMAND_STOP_SCAN:
 		{
 			if ( radio_state == MM_RADIO_STATE_READY )
 				goto NO_OP;

 			if ( radio_state != MM_RADIO_STATE_SCANNING )
 				goto INVALID_STATE;
 		}
 		break;

 		case MMRADIO_COMMAND_DESTROY:
 		case MMRADIO_COMMAND_MUTE:
 		case MMRADIO_COMMAND_UNMUTE:
 		case MMRADIO_COMMAND_SET_FREQ:
 		case MMRADIO_COMMAND_GET_FREQ:
		case MMRADIO_COMMAND_SET_REGION:
 		{
			/* we can do it at any state */
 		}
 		break;

 		case MMRADIO_COMMAND_SEEK:
 		{
			if ( radio_state != MM_RADIO_STATE_PLAYING )
				goto INVALID_STATE;
 		}
 		break;

		case MMRADIO_COMMAND_GET_REGION:
		{
			if ( radio_state == MM_RADIO_STATE_NULL )
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
 	debug_warning("invalid state. current : %d  command : %d\n",
 			radio_state, command);
	MMRADIO_LOG_FLEAVE();
 	return MM_ERROR_RADIO_INVALID_STATE;


 NO_OP:
 	debug_warning("mm-radio is in the desired state(%d). doing noting\n", radio_state);
	MMRADIO_LOG_FLEAVE();
 	return MM_ERROR_RADIO_NO_OP;

 }

static bool
__mmradio_set_state(mm_radio_t* radio, int new_state)
{
	MMMessageParamType msg = {0, };
	int msg_type = MM_MESSAGE_UNKNOWN;

	MMRADIO_LOG_FENTER();
 	MMRADIO_CHECK_INSTANCE( radio );

	if ( ! radio )
	{
		debug_warning("calling set_state with invalid radio handle\n");
		return false;
	}

	if ( radio->current_state == new_state && radio->pending_state == 0 )
	{
		debug_warning("we are in same state\n");
		return true;
	}

	/* set state */
	radio->old_state = radio->current_state;
	radio->current_state = new_state;

	/* fill message param */
	msg.state.previous = radio->old_state;
	msg.state.current = radio->current_state;

	/* post message to application */
	switch( radio->sm.by_asm_cb )
	{
		case MMRADIO_ASM_CB_NONE:
		{
			msg_type = MM_MESSAGE_STATE_CHANGED;
			MMRADIO_POST_MSG( radio, msg_type, &msg );
		}
		break;

		case MMRADIO_ASM_CB_POSTMSG:
		{
			msg_type = MM_MESSAGE_STATE_INTERRUPTED;
			msg.union_type = MM_MSG_UNION_CODE;
			msg.code = radio->sm.event_src;
			MMRADIO_POST_MSG( radio, msg_type, &msg );
		}
		break;

		case MMRADIO_ASM_CB_SKIP_POSTMSG:
		default:
		break;
	}

	MMRADIO_LOG_FLEAVE();

	return true;
}

static int
__mmradio_get_state(mm_radio_t* radio)
{
 	MMRADIO_CHECK_INSTANCE( radio );

	MMRADIO_LOG_DEBUG("radio state : current : [%d]   old : [%d]   pending : [%d]\n",
			radio->current_state, radio->old_state, radio->pending_state );

	return radio->current_state;
}

ASM_cb_result_t
__mmradio_asm_callback(int handle, ASM_event_sources_t event_source, ASM_sound_commands_t command, unsigned int sound_status, void* cb_data)
{
	mm_radio_t* radio = (mm_radio_t*) cb_data;
	int result = MM_ERROR_NONE;
	ASM_cb_result_t	cb_res = ASM_CB_RES_NONE;

	MMRADIO_LOG_FENTER();

	radio->sm.event_src = event_source;

	switch(command)
	{
		case ASM_COMMAND_STOP:
		case ASM_COMMAND_PAUSE:
		{
			MMRADIO_LOG_DEBUG("ASM asked me to stop. cmd : %d\n", command);
			switch(event_source)
			{
				case ASM_EVENT_SOURCE_CALL_START:
				case ASM_EVENT_SOURCE_ALARM_START:
				case ASM_EVENT_SOURCE_EARJACK_UNPLUG:
				case ASM_EVENT_SOURCE_MEDIA:
				{
					radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;					
					result = _mmradio_stop(radio);
					if( result ) 
					{
						MMRADIO_LOG_ERROR("failed to stop radio\n");
					}

					MMRADIO_LOG_DEBUG("skip unrealize in asm callback")
				}
				break;

				case ASM_EVENT_SOURCE_RESOURCE_CONFLICT:
				default:
				{
					radio->sm.by_asm_cb = MMRADIO_ASM_CB_SKIP_POSTMSG;					
					result = _mmradio_stop(radio);
					if( result ) 
					{
						MMRADIO_LOG_ERROR("failed to stop radio\n");
					}

					radio->sm.by_asm_cb = MMRADIO_ASM_CB_POSTMSG;
					result = _mmradio_unrealize(radio);
					if ( result ) 
					{
						MMRADIO_LOG_ERROR("failed to unrealize radio\n");
					}
				}
				break;
			}
			cb_res = ASM_CB_RES_STOP;
		}
		break;

		case ASM_COMMAND_PLAY:
		case ASM_COMMAND_RESUME:
		{
			MMMessageParamType msg = {0,};
			msg.union_type = MM_MSG_UNION_CODE;
			msg.code = event_source;

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

int _mmradio_get_region_type(mm_radio_t*radio, MMRadioRegionType *type)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_GET_REGION );

	return_val_if_fail( type, MM_ERROR_INVALID_ARGUMENT );

	*type = radio->region_setting.country;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}

int _mmradio_get_region_frequency_range(mm_radio_t* radio, unsigned int *min_freq, unsigned int *max_freq)
{
	MMRADIO_LOG_FENTER();
	MMRADIO_CHECK_INSTANCE( radio );
	MMRADIO_CHECK_STATE_RETURN_IF_FAIL( radio, MMRADIO_COMMAND_GET_REGION );

	return_val_if_fail( min_freq && max_freq, MM_ERROR_INVALID_ARGUMENT );

	*min_freq = radio->region_setting.band_min;
	*max_freq = radio->region_setting.band_max;

	MMRADIO_LOG_FLEAVE();
	return MM_ERROR_NONE;
}
