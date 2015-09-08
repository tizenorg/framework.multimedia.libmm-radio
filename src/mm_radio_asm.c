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

#include <assert.h>
#include <mm_debug.h>
#include "mm_radio_asm.h"
#include "mm_radio_utils.h"

int mmradio_asm_register(MMRadioASM* sm, ASM_sound_cb_t callback, void* param)
{
	/* read mm-session information */
	int session_type = MM_SESSION_TYPE_MEDIA;
	int session_options = 0;
	int errorcode = MM_ERROR_NONE;
	int asm_handle = -1;
	int event_type = ASM_EVENT_MEDIA_FMRADIO;
	int pid = -1;

	MMRADIO_LOG_FENTER();

	if ( ! sm )
	{
		MMRADIO_LOG_ERROR("invalid session handle\n");
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	/* read session information */
	errorcode = _mm_session_util_read_information(pid, &session_type, &session_options);
		if ( errorcode )
		{
		debug_warning("Read Session Information failed. use default \"media\" type\n");
		session_type = MM_SESSION_TYPE_MEDIA;
	}

	/* check if it's MEDIA type */
	if ( session_type != MM_SESSION_TYPE_MEDIA)
	{
		MMRADIO_LOG_DEBUG("session type is not MEDIA (%d)\n", session_type);
		return MM_ERROR_RADIO_INTERNAL;
	}

	/* check if it's running on the media_server */
	if ( sm->pid > 0 )
	{
		pid = sm->pid;
		MMRADIO_LOG_DEBUG("mm-radio is running on different process. Just faking pid to [%d]. :-p\n", pid);
	}
	else
	{
		MMRADIO_LOG_DEBUG("no pid has assigned. using default(current) context\n");
	}

	/* register audio-session-manager callback */
	if( ! ASM_register_sound(pid, &asm_handle, event_type, ASM_STATE_NONE, callback, (void*)param, ASM_RESOURCE_NONE, &errorcode))
	{
		MMRADIO_LOG_CRITICAL("ASM_register_sound() failed\n");
		return errorcode;
	}
	/* set session options */
	if (session_options)
	{
		if( ! ASM_set_session_option(asm_handle, session_options, &errorcode))
		{
			debug_error("ASM_set_session_options() failed, error(%x)\n", errorcode);
			return errorcode;
		}
	}

	/* now succeded to register our callback. take result */
	sm->handle = asm_handle;
	sm->state = ASM_STATE_NONE;

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int mmradio_asm_deregister(MMRadioASM* sm)
{
	int event_type = ASM_EVENT_MEDIA_FMRADIO;
	int errorcode = 0;

	MMRADIO_LOG_FENTER();

	if ( ! sm )
	{
		MMRADIO_LOG_ERROR("invalid session handle\n");
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	if( ! ASM_unregister_sound( sm->handle, event_type, &errorcode) )
	{
		MMRADIO_LOG_ERROR("Unregister sound failed 0x%X\n", errorcode);
		return MM_ERROR_POLICY_INTERNAL;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

int mmradio_asm_set_state(MMRadioASM* sm, ASM_sound_states_t state, ASM_resource_t resource)
{
	int event_type = ASM_EVENT_MEDIA_FMRADIO;

	MMRADIO_LOG_FENTER();

	if ( ! sm )
	{
		MMRADIO_LOG_ERROR("invalid session handle\n");
		return MM_ERROR_RADIO_NOT_INITIALIZED;
	}

	if ( sm->by_asm_cb == MMRADIO_ASM_CB_NONE ) //|| sm->state == ASM_STATE_PLAYING )
	{
		int ret = 0;

		if( ! ASM_set_sound_state( sm->handle, event_type, state, resource, &ret) )
		{
			MMRADIO_LOG_ERROR("set ASM state to [%d] failed 0x%X\n", state, ret);
			return MM_ERROR_POLICY_BLOCKED;
		}

		sm->state = state;
	}
	else // by asm callback
	{
		sm->by_asm_cb = MMRADIO_ASM_CB_NONE;
		sm->state = state;
	}

	MMRADIO_LOG_FLEAVE();

	return MM_ERROR_NONE;
}

