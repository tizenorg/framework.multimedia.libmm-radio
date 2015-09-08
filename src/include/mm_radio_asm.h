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
 
#ifndef MM_RADIO_ASM_H_
#define MM_RADIO_ASM_H_

#include <mm_types.h>
#include <mm_error.h>

#include <mm_session.h>
#include <mm_session_private.h>
#include <audio-session-manager.h>

enum {
	MMRADIO_ASM_CB_NONE,
	MMRADIO_ASM_CB_POSTMSG,
	MMRADIO_ASM_CB_SKIP_POSTMSG
};
typedef struct {
	int handle;
	int pid;
	int by_asm_cb;
	int event_src;
	ASM_sound_states_t state;
} MMRadioASM;

/* returns allocated handle */
int mmradio_asm_register(MMRadioASM* sm, ASM_sound_cb_t callback, void* param);
int mmradio_asm_deregister(MMRadioASM* sm);
int mmradio_asm_set_state(MMRadioASM* sm, ASM_sound_states_t state, ASM_resource_t resource);

#endif /* MM_RADIO_ASM_H_ */
