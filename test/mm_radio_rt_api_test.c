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
#include <stdio.h>

#include "mm_radio.h"
#include "mm_radio_rt_api_test.h"

#define MENU_ITEM_MAX	18

static int __menu(void);
static void __call_api( int choosen );
static int __msg_rt_callback(int message, void *param, void *user_param);

static MMHandleType g_my_radio = 0;

void __call_api( int choosen )
{
	int ret = MM_ERROR_NONE;

	switch( choosen )
	{
		case 1:
		{
			RADIO_TEST__( mm_radio_create( &g_my_radio ); )
			RADIO_TEST__( mm_radio_set_message_callback( g_my_radio, __msg_rt_callback, &g_my_radio); )
		}
		break;

		case 2:
		{
			RADIO_TEST__( mm_radio_destroy( g_my_radio ); )
			g_my_radio = 0;
		}
		break;

		case 3:
		{
			RADIO_TEST__( mm_radio_realize(g_my_radio ); )
		}
		break;

		case 4:
		{
			RADIO_TEST__( mm_radio_unrealize(g_my_radio ); )
		}
		break;

		case 7:
		{
			MMRadioStateType state = 0;
			RADIO_TEST__( mm_radio_get_state(g_my_radio, &state); )

			printf("state : %d\n", state);
		}
		break;

		case 8:
		{
			RADIO_TEST__( mm_radio_start(g_my_radio); )
		}
		break;

		case 9:
		{
			RADIO_TEST__( mm_radio_stop(g_my_radio); )
		}
		break;

		case 10:
		{
			int direction = 0;
			printf("input seek direction(0:UP/1:DOWN) : ");
			if (scanf("%d", &direction) == 0)
				return;

			RADIO_TEST__( mm_radio_seek(g_my_radio, direction); )
		}
		break;

		case 11:
		{
			int freq = 0;
			printf("input freq : ");
			if (scanf("%d", &freq) == 0)
				return;

			RADIO_TEST__( mm_radio_set_frequency(g_my_radio, freq); )
		}
		break;

		case 12:
		{
			int freq = 0;
			RADIO_TEST__( mm_radio_get_frequency(g_my_radio, &freq ); )

			printf("freq : %d\n", freq);
		}
		break;

		case 13:
		{
			RADIO_TEST__( mm_radio_scan_start(g_my_radio); )
		}
		break;

		case 14:
		{
			RADIO_TEST__( mm_radio_scan_stop(g_my_radio); )
		}
		break;

		case 16:
		{
			int muted = 0;
			printf("select one(0:UNMUTE/1:MUTE) : ");
			if ( scanf("%d", &muted) == 0)
				return;
			RADIO_TEST__( mm_radio_set_mute(g_my_radio, muted); )
		}
		break;

		case 17:
		{
			MMRadioRegionType type = 0;
			RADIO_TEST__( mm_radio_get_region_type(g_my_radio, &type ); )
			printf("region type : %d\n", type);
		}
		break;

		case 18:
		{
			unsigned int min_freq = 0;
			unsigned int max_freq = 0;
			RADIO_TEST__( mm_radio_get_region_frequency_range(g_my_radio, &min_freq, &max_freq ); )
			printf("region band range: %d ~ %d KHz\n", min_freq, max_freq);
		}
		break;

		default:
			break;
	}
}

int mm_radio_rt_api_test(void)
{
	while(1)
	{
		char key = 0;
		int choosen = 0;

		choosen = __menu();

		if ( choosen == -1)
			continue;

		if ( choosen == 0 )
			break;

		__call_api( choosen );
	}

	printf("radio test client finished\n");

	return 0;
}

int __menu(void)
{
	int menu_item = 0;

	printf("---------------------------------------------------------\n");
	printf("mm-radio rt api test. try now!\n");
	printf("---------------------------------------------------------\n");
	printf("[1] mm_radio_create\n");
	printf("[2] mm_radio_destroy\n");
	printf("[3] mm_radio_realize\n");
	printf("[4] mm_radio_unrealize\n");
	printf("[7] mm_radio_get_state\n");
	printf("[8] mm_radio_start\n");
	printf("[9] mm_radio_stop\n");
	printf("[10] mm_radio_seek\n");
	printf("[11] mm_radio_set_frequency(ex.107700)\n");
	printf("[12] mm_radio_get_frequency\n");
	printf("[13] mm_radio_scan_start\n");
	printf("[14] mm_radio_scan_stop\n");
	printf("[16] mm_radio_set_mute\n");
	printf("[17] mm_radio_get_region_type\n");
	printf("[18] mm_radio_get_region_frequency_range\n");
	printf("[0] quit\n");
	printf("---------------------------------------------------------\n");
	printf("choose one : ");
	
	if ( scanf("%d", &menu_item) == 0)
		return -1;

	if ( menu_item > MENU_ITEM_MAX )
		menu_item = -1;

	return menu_item;
}


int __msg_rt_callback(int message, void *pParam, void *user_param)
{
	MMMessageParamType* param = (MMMessageParamType*)pParam;
	MMHandleType radio = (MMHandleType) user_param;
	int ret = 0;

	printf("incomming message : %d\n", message);

	switch(message)
	{
	case MM_MESSAGE_STATE_CHANGED:

		printf("MM_MESSAGE_STATE_CHANGED: current : %d    old : %d\n"
				, param->state.current, param->state.previous);
		break;
	case MM_MESSAGE_RADIO_SCAN_START:
		printf("MM_MESSAGE_RADIO_SCAN_START\n");
		break;
	case MM_MESSAGE_RADIO_SCAN_INFO:
		assert(param);
		printf("MM_MESSAGE_RADIO_SCAN_INFO : freq : %d\n", param->radio_scan.frequency);
		break;
	case MM_MESSAGE_RADIO_SCAN_STOP:
		printf("MM_MESSAGE_RADIO_SCAN_STOP\n");
		break;
	case MM_MESSAGE_RADIO_SCAN_FINISH:
		printf("MM_MESSAGE_RADIO_SCAN_FINISHED\n");
		RADIO_TEST__( mm_radio_scan_stop(radio); )
		break;
	case MM_MESSAGE_RADIO_SEEK_START:
			printf("MM_MESSAGE_RADIO_SEEK_START\n");
			break;
	case MM_MESSAGE_RADIO_SEEK_FINISH:
		printf("MM_MESSAGE_RADIO_SEEK_FINISHED : freq : %d\n", param->radio_scan.frequency);
		break;
	default:
		printf("ERROR : unknown message received!\n");
		break;
	}

	return true;
}
