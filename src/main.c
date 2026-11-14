#include "base.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

create_slice_type(u8);
create_slice_type(f32);

int main() {
  u16 samples_in_second = 48000;
  f64 amplitude = 0.5;
  f64 frequency = 440;
  u16 samples_n = samples_in_second;
  f32 *samples = calloc(samples_n, sizeof(f32));
  for (u16 i = 0; i < samples_n; i++) {
    samples[i] =
        amplitude * sin(2 * M_PI * frequency * (f64)i / samples_in_second);
  }
  FILE *output_file = fopen("sine.f32", "w");
  fwrite(samples, sizeof(f32), samples_n, output_file);

  return 0;
}
