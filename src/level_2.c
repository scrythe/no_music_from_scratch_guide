#include "base.c"
#include <stdio.h>
#include <stdlib.h>

create_slice_type(f32);

int main() {
  system("ffmpeg -v error -nostdin -i samples/before.m4a -map 0:a:0 -af "
         "aresample=async=1:first_pts=0 -f f32le -ar 48000 -ac 2 -y "
         "/tmp/audio.raw");
  FILE *audio_file = fopen("/tmp/audio.raw", "r");
  fseek(audio_file, 0, SEEK_END);
  u32 audio_file_len = ftell(audio_file);
  rewind(audio_file);
  slice(f32) audio_data_interleaved = {
      .ptr = audio_data_interleaved.ptr = malloc(audio_file_len),
  };
  fread(audio_data_interleaved.ptr, 1, audio_file_len, audio_file);
  audio_data_interleaved.len = audio_file_len / sizeof(f32);
  remove("/tmp/audio.raw");

  slice(f32) audio_data_planar = {
      .ptr = malloc(audio_data_interleaved.len * sizeof(f32)),
      .len = audio_data_interleaved.len,
  };
  u32 offset_right = audio_data_planar.len / 2;
  for (u32 i = 0; i < audio_data_planar.len / 2; i++) {
    audio_data_planar.ptr[i] = audio_data_interleaved.ptr[2 * i];
    audio_data_planar.ptr[offset_right + i] =
        audio_data_interleaved.ptr[2 * i + 1];
  }
  fclose(audio_file);
  FILE *audio_output_file = fopen("audio.f32", "w");
  fwrite(audio_data_planar.ptr, sizeof(f32), audio_data_planar.len,
         audio_output_file);
  return 0;
}
