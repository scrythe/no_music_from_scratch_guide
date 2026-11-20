#include "base.c"
#include "profile.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  f32 real;
  f32 im;
} c32;

typedef struct {
  f64 real;
  f64 im;
} c64;

create_slice_type(f32);
create_slice_type(f64);
create_slice_type(c32);

#define IS_MAIN
#include "utility.c"

slice(c32) fft(slice(c32) audio_frame_data) {
  if (audio_frame_data.len == 1)
    return audio_frame_data;

  slice(c32) Pe = malloc_slice(c32, audio_frame_data.len / 2);
  slice(c32) Po = malloc_slice(c32, audio_frame_data.len / 2);

  for (u32 j = 0; j < audio_frame_data.len / 2; j++) {
    Pe.ptr[j] = audio_frame_data.ptr[2 * j];
    Po.ptr[j] = audio_frame_data.ptr[2 * j + 1];
  }

  slice(c32) ye = fft(Pe);
  slice(c32) yo = fft(Po);

  slice(c32) y = malloc_slice(c32, audio_frame_data.len);
  for (u32 j = 0; j < audio_frame_data.len / 2; j++) {
    f64 wj_real = cos(-2 * M_PI * j / audio_frame_data.len);
    f64 wj_im = sin(-2 * M_PI * j / audio_frame_data.len);

    y.ptr[j] = (c32){
        .real =
            ye.ptr[j].real + (wj_real * yo.ptr[j].real - wj_im * yo.ptr[j].im),
        .im = ye.ptr[j].im + (wj_real * yo.ptr[j].im + wj_im * yo.ptr[j].real),
    };
    y.ptr[j + audio_frame_data.len / 2] = (c32){
        .real =
            ye.ptr[j].real - (wj_real * yo.ptr[j].real - wj_im * yo.ptr[j].im),
        .im = ye.ptr[j].im - (wj_real * yo.ptr[j].im + wj_im * yo.ptr[j].real),
    };
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
  slice(c32) fft_audio_frequencies = fft(audio_complex);
  slice(f32)
      fft_audio_frequencies_f32 = {.ptr = (f32 *)fft_audio_frequencies.ptr,
                                   .len = fft_audio_frequencies.len + 2};
  write_f32("../no_music_2/mine/fft.f32", fft_audio_frequencies_f32);
  return 0;
}

static_assert(__COUNTER__ < MAX_PROFILE_POINTS,
              concat(can_not_have_more_than_, MAX_PROFILE_POINTS));
