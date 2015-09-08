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

#ifndef MM_RADIO_TEST_TYPE_H_
#define MM_RADIO_TEST_TYPE_H_

#include <stdio.h>
#include <assert.h>

typedef int (*test_function) (void);

typedef struct __test_item
{
	char menu_string[80];
 	char description[128];
 	test_function func;
	int result;
} test_item_t;

#define RADIO_TEST__(x_test)	\
		ret = x_test	\
		if ( ! ret )	\
		{	\
			printf("PASS : %s -- %s:%d\n", #x_test, __FILE__, __LINE__);	\
		}	\
		else	\
		{	\
			printf("FAIL : %s ERR-CODE : 0x%x -- %s:%d\n", #x_test, ret, __FILE__, __LINE__);	\
		}

#endif /* MM_RADIO_TEST_TYPE_H_ */
