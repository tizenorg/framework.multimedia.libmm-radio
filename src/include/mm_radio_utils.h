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
 
#ifndef __MM_RADIO_UTILS_H__
#define __MM_RADIO_UTILS_H__

#include <assert.h>
#include <mm_types.h>
#include <mm_error.h>
#include <mm_message.h>

/* radio log */
#define MMRADIO_LOG_FENTER		debug_fenter
#define MMRADIO_LOG_FLEAVE		debug_fleave
#define MMRADIO_LOG_DEBUG		debug_log
#define MMRADIO_LOG_ERROR		debug_error
#define MMRADIO_LOG_WARNING	debug_warning
#define MMRADIO_LOG_CRITICAL	debug_critical

/* general */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))
#endif

#define MMRADIO_MAX_INT	(2147483647)

#define MMRADIO_FREEIF(x) \
if ( x ) \
	free( x ); \
x = NULL;

#define MMRADIO_CHECK_INSTANCE( x_radio ) \
if ( ! x_radio ) \
{ \
	debug_error("radio instance is NULL\n"); \
	return MM_ERROR_RADIO_NOT_INITIALIZED; \
}

#define MMRADIO_CHECK_DEVICE_STATE( x_radio ) \
if ( x_radio->radio_fd < 0 ) \
{ \
	debug_error("not available radio device\n"); \
	return MM_ERROR_RADIO_NOT_INITIALIZED;\
}

/* command locking for multithreading */
#define MMRADIO_CMD_LOCK(x_radio)		pthread_mutex_lock( &((mm_radio_t*)x_radio)->cmd_lock )
#define MMRADIO_CMD_UNLOCK(x_radio)		pthread_mutex_unlock( &((mm_radio_t*)x_radio)->cmd_lock )

/* message posting */
#define MMRADIO_POST_MSG( x_radio, x_msgtype, x_msg_param ) \
debug_log("posting %s to application\n", #x_msgtype); \
__mmradio_post_message(x_radio, x_msgtype, x_msg_param);

/* setting radio state */
#define MMRADIO_SET_STATE( x_radio, x_state ) \
debug_log("setting mm-radio state to %d\n", x_state); \
__mmradio_set_state(x_radio, x_state);

/* state */
#define MMRADIO_CHECK_STATE_RETURN_IF_FAIL( x_radio, x_command ) \
debug_log("checking radio state before doing %s\n", #x_command); \
switch ( __mmradio_check_state(x_radio, x_command) ) \
{ \
	case MM_ERROR_RADIO_INVALID_STATE: \
		return MM_ERROR_RADIO_INVALID_STATE; \
	break; \
	/* NOTE : for robustness of mmfw. we won't treat it as an error */ \
	case MM_ERROR_RADIO_NO_OP: \
		return MM_ERROR_NONE; \
	break; \
	default: \
	break; \
}

#define MMRADIO_CHECK_RETURN_IF_FAIL( x_ret, x_msg ) \
do \
{ \
	if( x_ret < 0 ) \
	{ \
		debug_error("%s error\n", x_msg);\
		return x_ret; \
	} \
} while( 0 ) ;

#endif

