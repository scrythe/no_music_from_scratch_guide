#include "base.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

create_slice_type(f32);
create_slice_type(f64);
create_slice_type(c32);

#define IS_MAIN
#include "profile.c"
#include "utility.c"

u16 calc_rev_u11(u16 i) {
  // profile_function;
  u16 rev_i = 0;
  for (u32 j = 0; j < 11; j++) {
    rev_i = rev_i << 1;
    rev_i = rev_i | (i & 1);
    i = i >> 1;
  }
  return rev_i;
}

slice(c32) fft2(slice(f32) audio_frame_data) {
  u32 len = audio_frame_data.len;
  slice(c32) audio_complex = calloc_slice(c32, 2048);
  for (u32 i = 0; i < audio_frame_data.len; i++) {
    c32 val = {.real = audio_frame_data.ptr[i], .im = 0};
    audio_complex.ptr[i] = val;
  }
  // bit reversal
  slice(c32) y = malloc_slice(c32, 2048);
  // next power of 2 for audio length (log2(2048)) is 11, so 2048
  for (u32 i = 0; i < 2048; i++) {
    u16 reversed_i = calc_rev_u11(i);
    y.ptr[i] = audio_complex.ptr[reversed_i];
  }

  // why k instead of j:
  // i*2^level + k % (2^level) = k
  // also sinpi and cospi somehow worse in speed?
  // static might be faster but idk, but having it compute once at
  // startup or compile time would probs be better anyway
  // lookup is for  wj and w(j+N/2) same, so only 1024 instead of 2048
  f64 lookup_cos[1024];
  f64 lookup_sin[1024];
  for (u32 k = 0; k < 1024; k++) {
    lookup_cos[k] = cos(-2.0 * M_PI * k / 2048);
    lookup_sin[k] = sin(-2.0 * M_PI * k / 2048);
  }

  // first level (0) is skipped (base case)
  for (u32 level = 1; level < 12; level++) {
    // for level 1 its 2, level 2 its 4, level 3 its 8, for 4 its 16... so
    // 2^(level) or 1<<(level)
    u32 block_size = 1 << level;
    u32 blocks_n = audio_frame_data.len / block_size;
    u32 half_block_size = block_size / 2;
    for (u32 i = 0; i < blocks_n; i++) {
      for (u32 k = 0; k < half_block_size; k++) {
        u32 j = (1 << level) * i + k;

        u32 je = j;
        u32 jo = j + half_block_size;

        c64 wj = {
            .real = lookup_cos[k * blocks_n],
            .im = lookup_sin[k * blocks_n],
        };
        c64 wj_times_yoj = {
            .real = wj.real * y.ptr[jo].real - wj.im * y.ptr[jo].im,
            .im = wj.real * y.ptr[jo].im + wj.im * y.ptr[jo].real,
        };
        c32 ye = {
            .real = y.ptr[je].real + wj_times_yoj.real,
            .im = y.ptr[je].im + wj_times_yoj.im,
        };
        c32 yo = {
            .real = y.ptr[je].real - wj_times_yoj.real,
            .im = y.ptr[je].im - wj_times_yoj.im,
        };

        y.ptr[je] = ye;
        y.ptr[jo] = yo;
      }
    }
  }

  return y;
}

int main() {
  profile_program;
  slice(f32) audio_data = load_f32("../no_music_2/reference/levels/frame.f32");
  slice(c32) audio_complex = malloc_slice(c32, audio_data.len);
  for (u32 i = 0; i < audio_data.len; i++) {
    c32 val = {.real = audio_data.ptr[i], .im = 0};
    audio_complex.ptr[i] = val;
  }
  slice(c32) fft_audio_frequencies = fft2(audio_data);
  slice(f32)
      fft_audio_frequencies_f32 = {.ptr = (f32 *)fft_audio_frequencies.ptr,
                                   .len = fft_audio_frequencies.len + 2};
  write_f32("../no_music_2/mine/fft.f32", fft_audio_frequencies_f32);
  return 0;
}

static_assert(__COUNTER__ < MAX_PROFILE_POINTS,
              concat(can_not_have_more_than_, MAX_PROFILE_POINTS));
