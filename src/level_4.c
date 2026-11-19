#include "base.c"
#include "profile.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

create_slice_type(f32);
create_slice_type(f64);
#define IS_MAIN
#include "utility.c"

slice(f32) dft(slice(f32) audio_frame_data) {
  assert(audio_frame_data.len == 2048);
  profile_function;
  u32 max_bin = audio_frame_data.len / 2;
  u32 bins_n = max_bin + 1; // bin and max bin both included
  slice(f32) real_and_im = {.ptr = malloc(sizeof(f32) * bins_n * 2),
                            .len = bins_n * 2};

  slice(f64) lookup_cos = {.ptr = malloc(sizeof(f64) * audio_frame_data.len),
                           .len = audio_frame_data.len};
  slice(f64) lookup_sin = {.ptr = malloc(sizeof(f64) * audio_frame_data.len),
                           .len = audio_frame_data.len};
  // repeats every N steps (audio_frame_data.len), k*n % N
  for (u32 p = 0; p < audio_frame_data.len; p++) {
    lookup_cos.ptr[p] = cos(2 * M_PI * p / audio_frame_data.len);
    lookup_sin.ptr[p] = sin(2 * M_PI * p / audio_frame_data.len);
  }

  for (u32 k = 0; k < bins_n; k++) {
    f64 real_part = 0;
    f64 im_part = 0;
    for (u32 n = 0; n < audio_frame_data.len; n++) {
      u32 phase_index = (k * n) % audio_frame_data.len;
      real_part += audio_frame_data.ptr[n] * lookup_cos.ptr[phase_index];
      im_part += audio_frame_data.ptr[n] * lookup_sin.ptr[phase_index];
    }
    real_and_im.ptr[2 * k] = real_part;
    real_and_im.ptr[2 * k + 1] = -im_part;
  }
  return real_and_im;
}

int main() {
  profile_program;
  slice(f32) audio_data = load_f32("../no_music_2/reference/levels/frame.f32");
  slice(f32) dft_audio_data = dft(audio_data);
  write_f32("../no_music_2/mine/dft.f32", dft_audio_data);
  return 0;
}

static_assert(__COUNTER__ < MAX_PROFILE_POINTS,
              concat(can_not_have_more_than_, MAX_PROFILE_POINTS));
