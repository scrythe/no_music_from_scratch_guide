#include "base.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

create_slice_type(f32);

slice(f32) load_f32(char *audio_file_path) {
  FILE *audio_file = fopen(audio_file_path, "r");

  fseek(audio_file, 0, SEEK_END);
  u32 audio_file_len = ftell(audio_file);
  rewind(audio_file);
  slice(f32) audio_data = {
      .ptr = audio_data.ptr = malloc(audio_file_len),
  };
  fread(audio_data.ptr, 1, audio_file_len, audio_file);
  audio_data.len = audio_file_len / sizeof(f32);
  fclose(audio_file);
  return audio_data;
}

f64 snr_db(slice(f32) expected, slice(f32) actual) {
  assert(expected.len == actual.len);
  f64 signal = 0;
  f64 noise = 0;
  for (u32 i = 0; i < expected.len; i++) {
    signal += pow(expected.ptr[i], 2);
    noise += pow(expected.ptr[i] - actual.ptr[i], 2);
  }
  return 10 * log10(signal / noise);
}

int main() {
  slice(f32) expected = load_f32("samples/judge_a.f32");
  slice(f32) actual = load_f32("samples/judge_b.f32");
  f64 res = snr_db(expected, actual);
  printf("%lf", res);
  return 0;
}
