/* Copyright 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ecma-globals.h"
#include "ecma-helpers.h"

#include "test-common.h"

typedef struct
{
  ecma_number_t num;
  uint32_t uint32_num;
} uint32_test_case_t;

typedef struct
{
  ecma_number_t num;
  int32_t int32_num;
} int32_test_case_t;

/**
 * Unit test's main function.
 */
int
main (int __attr_unused___ argc,
      char __attr_unused___ **argv)
{
  TEST_INIT ();

  const uint32_test_case_t test_cases_uint32[] =
  {
#define TEST_CASE(num, uint32) { ecma_number_t(num), uint32_t(uint32) }
    TEST_CASE (1.0, 1),
    TEST_CASE (0.0, 0),
    TEST_CASE (ecma_number_negate (0.0), 0),
    TEST_CASE (NAN, 0),
    TEST_CASE (-NAN, 0),
    TEST_CASE (INFINITY, 0),
    TEST_CASE (-INFINITY, 0),
    TEST_CASE (0.1, 0),
    TEST_CASE (-0.1, 0),
    TEST_CASE (1.1, 1),
    TEST_CASE (-1.1, 4294967295ll),
    TEST_CASE (4294967295ll, 4294967295ll),
    TEST_CASE (-4294967295ll, 1),
    TEST_CASE (4294967296ll, 0),
    TEST_CASE (-4294967296ll, 0),
    TEST_CASE (4294967297ll, 1),
    TEST_CASE (-4294967297ll, 4294967295ll)
#undef TEST_CASE
  };

  for (uint32_t i = 0;
       i < sizeof (test_cases_uint32) / sizeof (test_cases_uint32[0]);
       i++)
  {
    JERRY_ASSERT (ecma_number_to_uint32 (test_cases_uint32[i].num) == test_cases_uint32[i].uint32_num);
  }

  int32_test_case_t test_cases_int32[] =
  {
#define TEST_CASE(num, int32) { ecma_number_t(num), int32_t(int32) }
    TEST_CASE (1.0, 1),
    TEST_CASE (0.0, 0),
    TEST_CASE (ecma_number_negate (0.0), 0),
    TEST_CASE (NAN, 0),
    TEST_CASE (-NAN, 0),
    TEST_CASE (INFINITY, 0),
    TEST_CASE (-INFINITY, 0),
    TEST_CASE (0.1, 0),
    TEST_CASE (-0.1, 0),
    TEST_CASE (1.1, 1),
    TEST_CASE (-1.1, -1),
    TEST_CASE (4294967295ll, -1),
    TEST_CASE (-4294967295ll, 1),
    TEST_CASE (4294967296ll, 0),
    TEST_CASE (-4294967296ll, 0),
    TEST_CASE (4294967297ll, 1),
    TEST_CASE (-4294967297ll, -1),
    TEST_CASE (2147483648ll, -2147483648ll),
    TEST_CASE (-2147483648ll, -2147483648ll),
    TEST_CASE (2147483647ll, 2147483647ll),
    TEST_CASE (-2147483647ll, -2147483647ll),
    TEST_CASE (-2147483649ll, 2147483647ll),
    TEST_CASE (2147483649ll, -2147483647ll)
#undef TEST_CASE
  };

  for (uint32_t i = 0;
       i < sizeof (test_cases_int32) / sizeof (test_cases_int32[0]);
       i++)
  {
    JERRY_ASSERT (ecma_number_to_int32 (test_cases_int32[i].num) == test_cases_int32[i].int32_num);
  }

  return 0;
} /* main */
