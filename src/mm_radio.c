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

#include <string.h>
#include "mm_radio.h"
#include "mm_radio_priv.h"
#include "mm_radio_utils.h"
#include <mm_types.h>
#include <mm_message.h>
#include "mm_debug.h"

/*===========================================================================================
|																							|
|  LOCAL DEFINITIONS AND DECLARATIONS FOR MODULE											|
|  																							|
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED VARIABLE DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    IMPORTED FUNCTION DECLARATIONS:										|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL #defines:														|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL CONSTANT DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL DATA TYPE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    GLOBAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL VARIABLE DEFINITIONS:											|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|    LOCAL FUNCTION PROTOTYPES:												|
---------------------------------------------------------------------------*/

/*===========================================================================
|																			|
|  FUNCTION DEFINITIONS														|
|  																			|
========================================================================== */

int mm_radio_create(MMHandleType *hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* new_radio = NULL;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(hradio, MM_ERROR_RADIO_NOT_INITIALIZED);


	/* alloc radio structure */
	new_radio = (mm_radio_t*) malloc(sizeof(mm_radio_t));
	if ( ! new_radio )
	{
		debug_critical("cannot allocate memory for radio\n");
		goto ERROR;
	}
	memset(new_radio, 0, sizeof(mm_radio_t));

	/* internal creation stuffs */
	result = _mmradio_create_radio( new_radio );

	if(result != MM_ERROR_NONE)
		goto ERROR;

	*hradio = (MMHandleType)new_radio;

	MMRADIO_LOG_FLEAVE();

	return result;

ERROR:

	if ( new_radio )
	{
		MMRADIO_FREEIF( new_radio );
	}

	*hradio = (MMHandleType)0;

	MMRADIO_LOG_FLEAVE();

	/* FIXIT : need to specify more error case */
	return result;
}

int  mm_radio_destroy(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	result = _mmradio_destroy( radio );

	if ( result != MM_ERROR_NONE )
	{
		debug_error("failed to destroy radio\n");
	}

	/* free radio */
	MMRADIO_FREEIF( radio );



	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_realize(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_realize( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_unrealize(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_unrealize( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_set_message_callback(MMHandleType hradio, MMMessageCallback callback, void *user_param)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_set_message_callback( radio, callback, user_param );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_get_state(MMHandleType hradio, MMRadioStateType* pState)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;
	int state = 0;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(pState, MM_ERROR_COMMON_INVALID_ARGUMENT);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_get_state( radio, &state );

	*pState = state;

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_start(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_start( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int  mm_radio_stop(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_stop( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_seek(MMHandleType hradio, MMRadioSeekDirectionType direction)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(direction >= MM_RADIO_SEEK_UP && direction <= MM_RADIO_SEEK_DOWN, MM_ERROR_INVALID_ARGUMENT);

	MMRADIO_CMD_LOCK( radio );

	radio->seek_direction = direction;

	result = _mmradio_seek( radio, direction );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_set_frequency(MMHandleType hradio, int freq)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_set_frequency( radio, freq );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_get_frequency(MMHandleType hradio, int* pFreq)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;
	int freq = 0;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(pFreq, MM_ERROR_INVALID_ARGUMENT);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_get_frequency( radio, &freq );

	*pFreq = freq;

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_scan_start(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_start_scan( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_scan_stop(MMHandleType hradio)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK( radio );

	result = _mmradio_stop_scan( radio );

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_set_mute(MMHandleType hradio, bool muted)
{
	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_LOG_FENTER();

	return_val_if_fail(radio, MM_ERROR_RADIO_NOT_INITIALIZED);

	MMRADIO_CMD_LOCK(radio);

	if (muted)
	{
		result = _mmradio_mute(radio);
	}
	else
	{
		result = _mmradio_unmute(radio);
	}

	MMRADIO_CMD_UNLOCK(radio);

	MMRADIO_LOG_FLEAVE();

	return result;
}

int mm_radio_get_signal_strength(MMHandleType hradio, int *value)
{
	return_val_if_fail(hradio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(value, MM_ERROR_INVALID_ARGUMENT);

	MMRADIO_LOG_FENTER();

	int ret = MM_ERROR_NONE;

	mm_radio_t* radio = (mm_radio_t*)hradio;

	MMRADIO_CMD_LOCK( radio );

	ret = _mm_radio_get_signal_strength(radio, value);

	MMRADIO_CMD_UNLOCK( radio );

	MMRADIO_LOG_DEBUG("signal strength = %d\n", *value);
	MMRADIO_LOG_FLEAVE();

	return ret;
}

int mm_radio_get_region_type(MMHandleType hradio, MMRadioRegionType *type)
{
	MMRADIO_LOG_FENTER();
	return_val_if_fail(hradio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(type, MM_ERROR_INVALID_ARGUMENT);

	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;
	MMRadioRegionType cur_type = MM_RADIO_REGION_GROUP_NONE;

	result = _mmradio_get_region_type(radio, &cur_type);

	if (result == MM_ERROR_NONE)
		*type = cur_type;

	MMRADIO_LOG_FLEAVE();
	return result;
}

int mm_radio_get_region_frequency_range(MMHandleType hradio, unsigned int *min, unsigned int*max)
{
	MMRADIO_LOG_FENTER();

	return_val_if_fail(hradio, MM_ERROR_RADIO_NOT_INITIALIZED);
	return_val_if_fail(min && max, MM_ERROR_INVALID_ARGUMENT);

	int result = MM_ERROR_NONE;
	mm_radio_t* radio = (mm_radio_t*)hradio;
	unsigned int min_freq = 0;
	unsigned int max_freq = 0;

	result = _mmradio_get_region_frequency_range(radio, &min_freq, &max_freq);

	if (result == MM_ERROR_NONE)
	{
		*min = min_freq;
		*max = max_freq;
	}

	MMRADIO_LOG_FLEAVE();
	return result;
}

