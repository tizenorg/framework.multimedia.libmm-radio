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

#ifndef __MM_RADIO_H__
#define	__MM_RADIO_H__

#include <mm_types.h>
#include <mm_message.h>
#include <mm_error.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@addtogroup RADIO
	@{

	@par
	This part describes the APIs with respect to play radio

 */


/**
 * Enumerations of radio state.
 */
typedef enum {
	MM_RADIO_STATE_NULL = 1,			/**< Radio is created, but not realized yet */
	MM_RADIO_STATE_READY,			/**< Radio is ready to play */
	MM_RADIO_STATE_PLAYING,			/**< Radio is now playing radio */
	MM_RADIO_STATE_SCANNING,			/**< Radio is now scanning frequency */
	MM_RADIO_STATE_NUM,				/**< Number of radio states */
}MMRadioStateType;

/**
 * Enumerations of seeking direction.
 */
typedef enum {
	MM_RADIO_SEEK_UP,					/**< Seek upward */
	MM_RADIO_SEEK_DOWN,				/**< Seek downward */
	MM_RADIO_SEEK_NUM				/**< Number of seeking direction */
} MMRadioSeekDirectionType;

/**
 * Enumerations of radio region country
 * Region settings are according to radio standards, not real geographic regions.
 * MM_RADIO_REGION_GROUP_1 : Notrh America, South America, South Korea, Taiwan, Australia
 * frequency details : 88.1 - 108MHz, 75uS de-emphasis
 *
 * MM_RADIO_REGION_GROUP_2 : China, Europe, Africa, Middle East, Hong Kong, India, Indonesia, Russia, Singapore
 * frequency details : 87.5 - 108MHz, 50uS de-emphasis
 *
 * MM_RADIO_REGION_GROUP_3 : Japan alone currenlty
 * frequency details : 76.1 - 89.9MHz, 50uS de-emphasis
 */
typedef enum {
	MM_RADIO_REGION_GROUP_NONE = 0,	/**< Region None */
	MM_RADIO_REGION_GROUP_USA,		/**< Region USA group */
	MM_RADIO_REGION_GROUP_EUROPE,	/**< Region EUROPE group */
	MM_RADIO_REGION_GROUP_JAPAN,		/**< Region Japan group */
} MMRadioRegionType;

#ifdef ENABLE_TTRACE
#include <ttrace.h>
#define TTRACE_BEGIN(NAME) traceBegin(TTRACE_TAG_VIDEO, NAME)
#define TTRACE_END() traceEnd(TTRACE_TAG_VIDEO)
#define TTRACE_ASYNCBEGIN(NAME, cookie) traceAsyncBegin (TTRACE_TAG_VIDEO, cookie, NAME);
#define TTRACE_ASYNCEND(NAME, cookie) traceAsyncEnd(TTRACE_TAG_VIDEO, cookie, NAME);
#else
#define TTRACE_BEGIN(NAME)
#define TTRACE_END()
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define FEATURE_ASM_SUPPORT

/**
 * This function creates a radio handle. \n
 * So, application can make radio instance and initializes it.
 *
 *
 * @param	hradio		[out]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	MM_RADIO_STATE_NULL
 * @remark	None
 * @see		mm_radio_destroy mm_radio_realize mm_radio_unrealize mm_radio_start mm_radio_stop
 * @par		Example
 */
int mm_radio_create(MMHandleType *hradio);

/**
 * This function deletes radio handle. \n
 * It closes radio device and releases all resources allocated.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		Application can use this API at any state.
 * @post 	None
 * @remark	None
 * @see		mm_radio_create mm_radio_realize mm_radio_unrealize mm_radio_start mm_radio_stop
 * @par		Example
 */
int mm_radio_destroy(MMHandleType hradio);

/**
 * This function opens radio device and ready to tune. \n
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_NULL
 * @post 	MM_RADIO_STATE_READY
 * @remark	None
 * @see		mm_radio_create mm_radio_destroy mm_radio_unrealize mm_radio_start mm_radio_stop
 * @par		Example
 */
int mm_radio_realize(MMHandleType hradio);

/**
 * This function mutes tuner and closes the radio device.
 * And, application can destroy radio directly without this API.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_READY
 * @post 	MM_RADIO_STATE_NULL
 * @remark	None
 * @see		mm_radio_create mm_radio_destroy mm_radio_realize mm_radio_start mm_radio_stop
 * @par		Example
 */
int mm_radio_unrealize(MMHandleType hradio);

/**
 * This function sets the callback function for receiving messages from radio.
 *
 * @param	hradio		[in]	Handle of radio.
 * @param	callback		[in]	Message callback function.
 * @param	user_param	[in]	User parameter which is passed to callback function.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	None
 * @see
 * @par		Example
 */
int mm_radio_set_message_callback(MMHandleType hradio, MMMessageCallback callback, void *user_param);

/**
 * This function gets the current state of radio.
 *
 * @param	hradio		[in]	Handle of radio.
 * @param	state		[out]	Current state of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	None
 * @see
 * @par		Example
 */
int mm_radio_get_state(MMHandleType hradio, MMRadioStateType *state);

/**
 * This function is to start playing radio.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_READY
 * @post 	MM_RADIO_STATE_PLAYING
 * @remark	None
 * @see		mm_radio_create mm_radio_destroy mm_radio_realize mm_radio_unrealize mm_radio_stop
 * @par	Example
 */
int mm_radio_start(MMHandleType hradio);

/**
 * This function is to stop playing radio.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_PLAYING
 * @post 	MM_RADIO_STATE_READY
 * @remark	None
 * @see		mm_radio_create mm_radio_destroy mm_radio_realize mm_radio_unrealize mm_radio_start
 * @par	Example
 */
int mm_radio_stop(MMHandleType hradio);

/**
 * This function seeks the effective frequency of radio. The application get a valid frequency from current value. \n
 * And, it can select direction to seek.
 * MM_MESSAGE_RADIO_SEEK_START will be sent when this function is called. \n
 * And if one valid frequency is found, MM_MESSAGE_RADIO_SEEK_FINISH will be sent. \n
 * It doesn't support wrap_around to prevent looping when there is no any valid frequency. \n
 * So, we will notice the limit of band as 87.5Mhz or 108Mhz.
 * In this case, applicaion can take two scenario.
 * One is to seek continually in the same direction. The other is to set previous frequency.
 *
 * @param	hradio		[in]	Handle of radio.
 * @param	direction		[in]	Seeking direction.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_PLAYING
 * @post 	MM_RADIO_STATE_PLAYING
 * @remark	None
 * @see		MM_MESSAGE_RADIO_SEEK_START MM_MESSAGE_RADIO_SEEK_FINISH
 * @par		Example
 */
int mm_radio_seek(MMHandleType hradio, MMRadioSeekDirectionType direction);

/**
 * This function sets the radio frequency with the desired one.
 *
 * @param	hradio	[in]	Handle of radio.
 * @param	freq		[in]	Frequency to set.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_READY, MM_RADIO_STATE_PLAYING
 * @post 	MM_RADIO_STATE_READY, MM_RADIO_STATE_PLAYING
 * @remark	None
 * @see		mm_radio_get_frequency
 * @par		Example
 */
int mm_radio_set_frequency(MMHandleType hradio, int freq);

/**
 * This function gets the current frequency of radio.
 *
 * @param	hradio		[in]	Handle of radio.
 * @param	pFreq		[out]	Current frequency.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_READY, MM_RADIO_STATE_PLAYING
 * @post 	MM_RADIO_STATE_READY, MM_RADIO_STATE_PLAYING
 * @remark	None
 * @see		mm_radio_set_frequency
 * @par		Example
 */
int mm_radio_get_frequency(MMHandleType hradio, int *pFreq);

/**
 * This function is to start for getting all effective frequencies. \n
 * So, if a frequency is found, MM_MESSAGE_RADIO_SCAN_INFO will be posted with frequency information.
 * If there is no frequency, we will post MM_MESSAGE_RADIO_SCAN_FINISH.
 * And then, the radio state will be changed as MM_RADIO_STATE_READY.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_READY
 * @post		MM_RADIO_STATE_SCANNING
 * @remark	None
 * @see		mm_radio_scan_stop
 * @par		Example
 */
int mm_radio_scan_start(MMHandleType hradio);

/**
 * This function is to stop for getting all the effective frequencies. \n
 * So, application can use this API to stop in the middle of scanning.
 *
 * @param	hradio		[in]	Handle of radio.
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		MM_RADIO_STATE_SCANNING
 * @post 	MM_RADIO_STATE_READY
 * @remark	None
 * @see		mm_radio_scan_start
 * @par		Example
 */
int mm_radio_scan_stop(MMHandleType hradio);

/**
 * This function is to mute/unmute radio output.
 *
 * @param	hradio		[in]		Handle of radio.
 * @param	muted		[in]		0: unmute 1: mute
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	None
 * @see
 * @par		Example
 */
int mm_radio_set_mute(MMHandleType hradio, bool muted);
/**
 * This function is get strength of radio signal.
 *
 * @param	hradio		[in]		Handle of radio.
 * @param	value		[in]		signal strength
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	None
 * @see
 * @par		Example
 */
int mm_radio_get_signal_strength(MMHandleType hradio, int *value);

/**
 * This function is to get type of radio region.
 *
 * @param	hradio		[in]		Handle of radio.
 * @param	type		[out]		type of current region
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	None
 * @see mm_radio_get_region_frequency_range()
 */
int mm_radio_get_region_type(MMHandleType hradio, MMRadioRegionType *type);

/**
 * This function is to get range of radio frequency.
 *
 * @param	hradio		[in]		Handle of radio.
 * @param	min		[out]		min frequency value
 * @param	max		[out]		max frequency value
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @remark	 The unit of frequency is KHz.
 * @see mm_radio_get_region_type()
 */
int mm_radio_get_region_frequency_range(MMHandleType hradio, unsigned int *min, unsigned int *max);

/**
 * This function is to get channel spacing.
 *
 * @param	hradio		[in]		Handle of radio.
 * @param	channel_spacing	[out]		channel spacing value
 *
 * @return	This function returns zero on success, or negative value with errors
 * @pre		None
 * @post 	None
 * @see mm_radio_get_region_type()
 */
int mm_radio_get_channel_spacing(MMHandleType hradio, int *channel_spacing);


/**
	@}
 */


#ifdef __cplusplus
}
#endif

#endif	/* __MM_RADIO_H__ */
