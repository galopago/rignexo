/*
Copyright (c) 2021, Kazuhisa Yokota, JN1DFF
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of the nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "filter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int filter(filter_t *fp, int value) {
  int32_t sum = 0;
  int i;
  int an_idx;

  fp->x[fp->index] = (int16_t)value;
  an_idx = fp->size - 1 - fp->index;

  if (++fp->index >= fp->size) {
    fp->index = 0;
  }

  for (i = 0; i < fp->size; i++) {
    sum += (int32_t)fp->an[an_idx + i] * (int32_t)fp->x[i];
  }

  return (int)sum;
}

static float sinc(float x) {
  if (fabsf(x) < 1e-6f) {
    return 1.0f;
  }
  return sinf(x) / x;
}

static float windowf(float x) {
  return 0.54f + 0.46f * cosf(x);
}

int16_t *filter_coeff(filter_param_t const *f) {
  const int fp = f->pass_freq;
  const int fc = f->cutoff_freq;
  const int fck = f->sampling_freq;
  const int size = f->size;
  const float Rp = (float)fp / (float)fck;
  const float Rc = (float)fc / (float)fck;
  const int M = (size - 1) / 2;
  const int A = 1 << 15;
  int16_t *an;
  int n;

  an = calloc((size_t)(size * 2 - 1), sizeof(int16_t));
  if (an == NULL) {
    return NULL;
  }

  for (n = -M; n <= M; n++) {
    int val = (int)(A * 2 *
                        (Rc * sinc(2.0f * (float)M_PI * Rc * (float)n) -
                         Rp * sinc(2.0f * (float)M_PI * Rp * (float)n)) *
                        windowf((float)M_PI * (float)n / (float)M) +
                    0.5f);
    an[n + M] = (int16_t)val;
  }

  for (n = 0; n < size - 1; n++) {
    an[size + n] = an[n];
  }

  return an;
}

void filter_init(filter_t *fp, int16_t an[], int size) {
  int16_t *p;

  fp->an = an;
  fp->size = size;
  fp->index = 0;

  p = calloc((size_t)((size + 3) & ~3), sizeof(int16_t));
  if (p == NULL) {
    fp->x = NULL;
    return;
  }
  memset(p, 0, (size_t)((size + 3) & ~3) * sizeof(int16_t));
  fp->x = p;
}
