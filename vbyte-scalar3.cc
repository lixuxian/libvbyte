/*
 * Copyright (C) 2005-2016 Christoph Rupp (chris@crupp.de).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "vbyte.h"

#include <assert.h>
#include <string.h>
#include <x86intrin.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef signed char int8_t;
#else
#  include <stdint.h>
#endif

#include "vbyte-internal.h"

static uint16_t
extract_16_msbs(const uint8_t *in)
{
  __m128i v = _mm_lddqu_si128((const __m128i *)in);
  return _mm_movemask_epi8(v);
}

struct lookup_line_t
{
  char num_ints;
  char num_bytes;
  char ints[16];
};

static lookup_line_t lookup_table[] = {
#  include "vbyte-gen.i"
};

uint32_t
vbyte_uncompress_scalar3(const uint8_t *in, uint32_t *out, uint32_t length)
{
  const uint8_t *initial_in = in;

  uint32_t i = 0;
  while (i + 16 < length) {
    // extract the MSBs of the next 16 bytes
    uint16_t mask = extract_16_msbs(in);

    // use a lookup table to get information about the compressed integers
    lookup_line_t *line = &lookup_table[mask];

    uint32_t v;

    // extract the compressed integers
    for (int j = 0; j < line->num_ints; ++j, ++out) {
      switch (line->ints[j]) {
        case 1:
          *out = *in;
          in += 1;
          break;
        case 2:
          v = *(uint32_t *)&in[0];
          *out =   ((v & 0x7F00u) >> 1)
                 |  (v & 0x7Fu);
          in += 2;
          break;
        case 3:
          v = *(uint32_t *)&in[0];
          *out =   ((v & 0x7F0000u) >> 2)
                 | ((v & 0x7F00u) >> 1)
                 |  (v & 0x7Fu);
          in += 3;
          break;
        case 4:
          v = *(uint32_t *)&in[0];
          *out =   ((v & 0x7F000000u) >> 3)
                 | ((v & 0x7F0000u) >> 2)
                 | ((v & 0x7F00u) >> 1)
                 |  (v & 0x7Fu);
          in += 4;
          break;
        default:
          v = *(uint32_t *)&in[0];
          assert(line->ints[j] == 5);
          *out =   ((v & 0x7F00000000u) >> 4)
                 | ((v & 0x7F000000u) >> 3)
                 | ((v & 0x7F0000u) >> 2)
                 | ((v & 0x7F00u) >> 1)
                 |  (v & 0x7Fu);
          in += 5;
          break;
      }
    }

    i += line->num_ints;
  }

  // pick up the remaining values
  for (; i < length; i++) {
    in += read_int(in, out);
    ++out;
  }
  return in - initial_in;
}


