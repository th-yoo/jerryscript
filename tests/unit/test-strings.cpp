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

#include "ecma-helpers.h"
#include "lit-strings.h"

#include "test-common.h"

// Iterations count
#define test_iters (1024)

// Sub iterations count
#define test_subiters (128)

// Max bytes in string
#define max_bytes_in_string (16 * 1024)
#define max_code_units_in_string (max_bytes_in_string)

typedef enum
{
  UTF8_ANY_SIZE,
  UTF8_ONE_BYTE,
  UTF8_TWO_BYTES,
  UTF8_THREE_BYTES,
  UTF8_FOUR_BYTES
} utf8_char_size;

static lit_utf8_size_t
generate_utf8_char (utf8_char_size char_size,
                    lit_utf8_byte_t *buf)
{
  JERRY_ASSERT (char_size >= 0 && char_size <= LIT_UTF8_MAX_BYTES_IN_CODE_POINT);
  lit_code_point_t code_point = (lit_code_point_t) rand ();

  if (char_size == 1)
  {
    code_point %= LIT_UTF8_1_BYTE_CODE_POINT_MAX;
  }
  else if (char_size == 2)
  {
    code_point = LIT_UTF8_2_BYTE_CODE_POINT_MIN + code_point % (LIT_UTF8_2_BYTE_CODE_POINT_MAX -
                                                                LIT_UTF8_2_BYTE_CODE_POINT_MIN);
  }
  else if (char_size == 3)
  {
    code_point = LIT_UTF8_3_BYTE_CODE_POINT_MIN + code_point % (LIT_UTF8_3_BYTE_CODE_POINT_MAX -
                                                                LIT_UTF8_3_BYTE_CODE_POINT_MIN);
  }
  else if (char_size == 4)
  {
    code_point = LIT_UTF8_4_BYTE_CODE_POINT_MIN + code_point % (LIT_UTF8_4_BYTE_CODE_POINT_MAX -
                                                                LIT_UTF8_4_BYTE_CODE_POINT_MIN);
  }
  else
  {
    code_point %= LIT_UTF8_4_BYTE_CODE_POINT_MAX;
  }

  if (code_point >= LIT_UTF16_HIGH_SURROGATE_MIN
      && code_point <= LIT_UTF16_LOW_SURROGATE_MAX)
  {
    code_point = LIT_UTF16_HIGH_SURROGATE_MIN - 1;
  }

  return lit_code_point_to_utf8 (code_point, buf);
}

static ecma_length_t
generate_utf8_string (lit_utf8_byte_t *buf_p,
                      lit_utf8_size_t buf_size)
{
  ecma_length_t length = 0;

  lit_utf8_size_t size = 0;
  while (size  < buf_size)
  {
    const utf8_char_size char_size = (((buf_size - size) > LIT_UTF8_MAX_BYTES_IN_CODE_POINT)
                                      ? UTF8_ANY_SIZE
                                      : (utf8_char_size) (buf_size - size));

    lit_utf8_size_t bytes_generated = generate_utf8_char (char_size, buf_p);

    JERRY_ASSERT (lit_is_utf8_string_valid (buf_p, bytes_generated));

    size += bytes_generated;
    buf_p += bytes_generated;
    length += (bytes_generated == LIT_UTF8_MAX_BYTES_IN_CODE_POINT) ? 2 : 1;
  }

  JERRY_ASSERT (size == buf_size);

  return length;
}


int
main (int __attr_unused___ argc,
      char __attr_unused___ **argv)
{
  TEST_INIT ();

  mem_init ();

  lit_utf8_byte_t utf8_string[max_bytes_in_string];
  ecma_char_t code_units[max_code_units_in_string];
  lit_utf8_iterator_pos_t saved_positions[max_code_units_in_string];

  for (int i = 0; i < test_iters; i++)
  {
    lit_utf8_size_t utf8_string_size = (i == 0) ? 0 : (lit_utf8_size_t) (rand () % max_bytes_in_string);
    ecma_length_t length = generate_utf8_string (utf8_string, utf8_string_size);

    JERRY_ASSERT (lit_utf8_string_length (utf8_string, utf8_string_size) == length);

    lit_utf8_iterator_t iter = lit_utf8_iterator_create (utf8_string, utf8_string_size);
    ecma_length_t calculated_length = 0;

    ecma_length_t code_units_count = 0;
    while (!lit_utf8_iterator_is_eos (&iter))
    {
      code_units[code_units_count] = lit_utf8_iterator_peek_next (&iter);
      saved_positions[code_units_count] = lit_utf8_iterator_get_pos (&iter);
      code_units_count++;
      calculated_length++;

      lit_utf8_iterator_incr (&iter);
    }

    JERRY_ASSERT (length == calculated_length);

    if (code_units_count > 0)
    {
      for (int j = 0; j < test_subiters; j++)
      {
        ecma_length_t index = (ecma_length_t) rand () % code_units_count;
        lit_utf8_iterator_seek (&iter, saved_positions[index]);
        JERRY_ASSERT (lit_utf8_iterator_peek_next (&iter) == code_units[index]);
        JERRY_ASSERT (lit_utf8_iterator_get_index (&iter) == index);
      }
    }

    lit_utf8_iterator_seek_eos (&iter);
    while (!lit_utf8_iterator_is_bos (&iter))
    {
      JERRY_ASSERT (code_units_count > 0);
      calculated_length--;
      JERRY_ASSERT (code_units[calculated_length] == lit_utf8_iterator_peek_prev (&iter));
      lit_utf8_iterator_decr (&iter);
    }

    JERRY_ASSERT (calculated_length == 0);

    while (!lit_utf8_iterator_is_eos (&iter))
    {
      ecma_char_t code_unit = lit_utf8_iterator_read_next (&iter);
      JERRY_ASSERT (code_unit == code_units[calculated_length]);
      calculated_length++;
    }

    JERRY_ASSERT (length == calculated_length);

    while (!lit_utf8_iterator_is_bos (&iter))
    {
      JERRY_ASSERT (code_units_count > 0);
      calculated_length--;
      JERRY_ASSERT (code_units[calculated_length] == lit_utf8_iterator_read_prev (&iter));
    }

    JERRY_ASSERT (calculated_length == 0);
  }

  /* Overlong-encoded code point */
  lit_utf8_byte_t invalid_utf8_string_1[] = {0xC0, 0x82};
  JERRY_ASSERT (!lit_is_utf8_string_valid (invalid_utf8_string_1, sizeof (invalid_utf8_string_1)));

  /* Overlong-encoded code point */
  lit_utf8_byte_t invalid_utf8_string_2[] = {0xE0, 0x80, 0x81};
  JERRY_ASSERT (!lit_is_utf8_string_valid (invalid_utf8_string_2, sizeof (invalid_utf8_string_2)));

  /* Pair of surrogates: 0xD901 0xDFF0 which encode Unicode character 0x507F0 */
  lit_utf8_byte_t invalid_utf8_string_3[] = {0xED, 0xA4, 0x81, 0xED, 0xBF, 0xB0};
  JERRY_ASSERT (!lit_is_utf8_string_valid (invalid_utf8_string_3, sizeof (invalid_utf8_string_3)));

  /* Isolated high surrogate 0xD901 */
  lit_utf8_byte_t valid_utf8_string_1[] = {0xED, 0xA4, 0x81};
  JERRY_ASSERT (lit_is_utf8_string_valid (valid_utf8_string_1, sizeof (valid_utf8_string_1)));

  /* 4-byte long utf-8 character - Unicode character 0x507F0 */
  lit_utf8_byte_t valid_utf8_string_2[] = {0xF1, 0x90, 0x9F, 0xB0};
  JERRY_ASSERT (lit_is_utf8_string_valid (valid_utf8_string_2, sizeof (valid_utf8_string_2)));

  lit_utf8_byte_t buf[] = {0xF0, 0x90, 0x8D, 0x88};
  lit_code_point_t code_point;
  lit_utf8_size_t bytes_count = lit_read_code_point_from_utf8 (buf, sizeof (buf), &code_point);
  JERRY_ASSERT (bytes_count == 4);
  JERRY_ASSERT (code_point == 0x10348);

  lit_utf8_byte_t res_buf[3];
  lit_utf8_size_t res_size;

  res_size = lit_code_unit_to_utf8 (0x73, res_buf);
  JERRY_ASSERT (res_size == 1);
  JERRY_ASSERT (res_buf[0] == 0x73);

  res_size = lit_code_unit_to_utf8 (0x41A, res_buf);
  JERRY_ASSERT (res_size == 2);
  JERRY_ASSERT (res_buf[0] == 0xD0);
  JERRY_ASSERT (res_buf[1] == 0x9A);

  res_size = lit_code_unit_to_utf8 (0xD7FF, res_buf);
  JERRY_ASSERT (res_size == 3);
  JERRY_ASSERT (res_buf[0] == 0xED);
  JERRY_ASSERT (res_buf[1] == 0x9F);
  JERRY_ASSERT (res_buf[2] == 0xBF);

  lit_utf8_byte_t bytes[] = {0xF0, 0x90, 0x8D, 0x88};
  lit_utf8_iterator_t iter = lit_utf8_iterator_create (bytes, sizeof (bytes));
  ecma_char_t code_unit = lit_utf8_iterator_read_next (&iter);
  JERRY_ASSERT (!lit_utf8_iterator_is_eos (&iter));
  JERRY_ASSERT (code_unit == 0xD800);
  code_unit = lit_utf8_iterator_read_next (&iter);
  JERRY_ASSERT (lit_utf8_iterator_is_eos (&iter));
  JERRY_ASSERT (code_unit == 0xDF48);

  mem_finalize (true);
  return 0;
}
