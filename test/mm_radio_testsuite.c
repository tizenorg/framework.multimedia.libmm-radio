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

/* testsuite for mm-radio library */
#include <stdlib.h>
#include <stdio.h>
#include <mm_ta.h>


#include "mm_radio.h"
#include "mm_radio_test_type.h"
#include "mm_radio_rt_api_test.h"

#define DEFAULT_TEST_FREQ	107700

/* test items...*/
int __test_radio_init(void);
int __test_radio_listen_gorealra(void);
int __test_repeat_init_release(void);
int __test_repeat_start_stop(void);
int __test_repeat_seek(void);
int __test_repeat_whole(void);
int __test_manual_api_calling(void);
int __test_radio_hw_debug(void);

static int __msg_callback(int message, void *param, void *user_param);

/* functions*/
static void __print_menu(void);
static void __run_test(int key);

/* list of tests*/
test_item_t g_tests[100] =
{
	/* menu string : short string to be displayed to menu
	     description : detailed description
	     test function :  a pointer to a actual test function
	     0 : to be filled with return value of test function
	 */
	{
		"init test",
  		"check radio init function",
  		__test_radio_init,
      	0
	},

	{
		"listening gorealra",
  		"let's listen to the gorealra!",
  		__test_radio_listen_gorealra,
      	0
	},

	{
		"repeat_init_release",
  		"repeat init and release and check if it working and memory usage increment",
  		__test_repeat_init_release,
      	0
	},

	{
		"repeat_start_stop",
  		"repeat start and stop and check if it working and memory usage increment",
  		__test_repeat_start_stop,
      	0
	},

	{
		"repeat_seek",
  		"repeat seek and check if it working and memory usage increment",
  		__test_repeat_seek,
      	0
	},

	{
		"repeat_whole",
  		"repeat whole radio sequence and check if it working and memory usage increment",
  		__test_repeat_whole,
      	0
	},

	{
		"manual api calling test",
  		"mapping each api to each test manu. just like other testsuite. try to reproduce the bugs with it.",
  		__test_manual_api_calling,
      	0
	},

 	/* add tests here*/

 	/* NOTE : do not remove this last item */
 	{"end", "", NULL, 0},
};

int g_num_of_tests = 0;

int main(int argc, char **argv)
{
	MMTA_INIT();
	int key = 0;

	do {
		__print_menu();

		do {
			key = getchar();
			
			if ( key >= '0' && key <= '9')
			{
				__run_test( key - '0' );
			}
		}while ( key == '\n' );
	}while(key != 'q' && key == 'Q');
	
	printf("radio test client finished\n");

	return 0;
}

void __print_menu(void)
{
	int i = 0;

	printf("\n\nFMRadio testing menu\n");
	printf("------------------------------------------\n");

	for ( i = 0; g_tests[i].func; i++ )
	{
		printf( "[%d] %s\n", i, g_tests[i].menu_string );
	}
	printf("[q] quit\n");

	g_num_of_tests = i;

	printf("Choose one : ");
}

void __run_test(int key)
{
	int ret = 0;

	/* check index */
	printf("#tests : %d    key : %d\n", g_num_of_tests, key);
	if ( key >= g_num_of_tests || key < 0 )
	{
		printf("unassigned key has pressed : %d\n", key);
		return;
	}

	/* display description*/
	printf( "excuting test : %s\n", g_tests[key].menu_string );
	printf( "description : %s\n", g_tests[key].description );

	/* calling test function*/
	ret = g_tests[key].func();

	g_tests[key].result =  ret;

	if ( ret )
	{
		printf( "TEST FAILED. ret code : %d\n", g_tests[key].result);
	}
	else
	{
		printf( "TEST SUCCEDED. ret code : %d\n", g_tests[key].result);
	}
}

static int __msg_callback(int message, void *pParam, void *user_param)
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
		printf("MM_MESSAGE_RADIO_SCAN_INFO : freq : %d KHz\n", param->radio_scan.frequency);
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
		printf("MM_MESSAGE_RADIO_SEEK_FINISHED : freq : %d KHz\n", param->radio_scan.frequency);
		break;
	default:
		printf("ERROR : unknown message received!\n");
		break;
	}

	return true;
}

/* test items...*/
int __test_radio_init(void)
{
	printf("%s\n", __FUNCTION__);

	int ret = MM_ERROR_NONE;
	MMHandleType radio = 0;

	RADIO_TEST__(	mm_radio_create(&radio);	)
	RADIO_TEST__( mm_radio_set_message_callback( radio, (MMMessageCallback)__msg_callback, (void*)radio ); )
	RADIO_TEST__( mm_radio_realize(radio); )
	RADIO_TEST__( mm_radio_unrealize(radio); )
	RADIO_TEST__( mm_radio_destroy(radio); )
	return ret;
}

int __test_radio_listen_gorealra(void)
{
	printf("%s\n", __FUNCTION__);

	int ret = MM_ERROR_NONE;
	MMHandleType radio = 0;

	RADIO_TEST__(	mm_radio_create(&radio);	)
	RADIO_TEST__( mm_radio_set_message_callback( radio, (MMMessageCallback)__msg_callback, (void*)radio ); )
	RADIO_TEST__( mm_radio_realize(radio); )
	RADIO_TEST__( mm_radio_set_frequency( radio, DEFAULT_TEST_FREQ ); )
	RADIO_TEST__( mm_radio_start(radio); )

	return ret;
}

int __test_repeat_init_release(void)
{
	printf("%s\n", __FUNCTION__);

	int ret = MM_ERROR_NONE;
	int cnt = 0;
	MMHandleType radio = 0;

	while ( 1 )
	{
		RADIO_TEST__(	mm_radio_create(&radio);	)
		RADIO_TEST__( mm_radio_set_message_callback( radio, (MMMessageCallback)__msg_callback, (void*)radio ); )
		RADIO_TEST__( mm_radio_realize(radio); )
		RADIO_TEST__( mm_radio_unrealize(radio); )
		RADIO_TEST__( mm_radio_destroy(radio); )

		cnt++;

		printf("%s : repeat count : %d\n", __FUNCTION__, cnt);
	}

	return 0;
}

int __test_repeat_start_stop(void)
{
	printf("%s\n", __FUNCTION__);
	int ret = MM_ERROR_NONE;
	int cnt = 0;
	MMHandleType radio = 0;

	RADIO_TEST__(	mm_radio_create(&radio);	)
	RADIO_TEST__( mm_radio_set_message_callback( radio, (MMMessageCallback)__msg_callback, (void*)radio ); )
	RADIO_TEST__( mm_radio_realize(radio); )
	RADIO_TEST__( mm_radio_set_frequency( radio, DEFAULT_TEST_FREQ ); )

	while(1)
	{
		RADIO_TEST__( mm_radio_start(radio); )
		RADIO_TEST__( mm_radio_stop(radio); )

		cnt++;

		printf("%s : repeat count : %d\n", __FUNCTION__, cnt);
	}

	return 0;
}

int __test_repeat_seek(void)
{
	printf("__test_repeat_seek\n");
	return 0;
}

int __test_repeat_whole(void)
{
	printf("__test_repeat_whole\n");
	return 0;
}

int __test_manual_api_calling(void)
{

	mm_radio_rt_api_test();

	return 0;
}

