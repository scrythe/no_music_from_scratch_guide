#ifndef IS_MAIN
#include "base.c"
#include <stdio.h>
#include <time.h>
#endif

#define MAX_PROFILE_POINTS 50

#define NANOSEC_PER_SEC 1e9
#define NANOSEC_PER_MS 1e6
#define MS_PER_SEC 1e3

u64 get_nano_sec() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts.tv_sec * NANOSEC_PER_SEC + ts.tv_nsec;
}

u64 get_cpu_freq(u64 ms_wait_time) {
  u64 ns_wait_time = ms_wait_time * NANOSEC_PER_MS;
  u64 os_start = get_nano_sec();
  u64 cpu_start = __rdtsc();
  u64 os_elapsed = 0;
  while (os_elapsed < ns_wait_time) {
    os_elapsed = get_nano_sec() - os_start;
  }
  u64 cpu_elapsed = __rdtsc() - cpu_start;
  // there are cpu_elapsed (amount) "ticks" in os_elapsed time (nano sec, or
  //  1/NANOSEC_PER_SEC sec), so cpu_elapsed/(os_elpased/NANOSEC_PER_SEC)
  return cpu_elapsed * NANOSEC_PER_SEC / os_elapsed;
}

typedef struct {
  u64 start;
  const char *profile_name;
  u16 profile_anchor;
  u16 parent_anchor;
} Profile_Start;

typedef struct {
  u64 elapsed_total;
  u64 elapsed_children;
  const char *profile_name;
  u32 hits;
} Profile_Anchor;

u16 current_anchor = __COUNTER__;
Profile_Anchor profile_anchors[MAX_PROFILE_POINTS];

void end_profile_block(Profile_Start *input) {
  Profile_Anchor *anchor = &profile_anchors[input->profile_anchor];
  Profile_Anchor *parent_anchor = &profile_anchors[input->parent_anchor];
  anchor->profile_name = input->profile_name;
  anchor->elapsed_total += __rdtsc() - input->start;
  parent_anchor->elapsed_children += __rdtsc() - input->start;
  anchor->hits += 1;

  current_anchor = input->parent_anchor;
}

void end_profile_and_print(u64 *program_start) {
  u64 program_elapsed = __rdtsc() - *program_start;
  u64 cpu_freq = get_cpu_freq(1000);

  printf("Total time %.4fms\n",
         (f64)program_elapsed / (f64)cpu_freq * MS_PER_SEC);
  for (u8 i = 0; i < MAX_PROFILE_POINTS; i++) {
    Profile_Anchor profile_anchor = profile_anchors[i];
    if (profile_anchor.elapsed_total) {
      u64 elapsed =
          profile_anchor.elapsed_total - profile_anchor.elapsed_children;
      printf("  %s[%d]: %lu (%.4f%%", profile_anchor.profile_name,
             profile_anchor.hits, elapsed,
             (f64)elapsed / program_elapsed * 100);
      if (profile_anchor.elapsed_children) {
        printf(", %.4f%% w/childrend",
               (f64)profile_anchor.elapsed_total / program_elapsed * 100);
      }
      printf(")\n");
    }
  }
}

#define profile_block(name)                                                    \
  u16 parent_anchor = current_anchor;                                          \
  current_anchor = __COUNTER__;                                                \
  __attribute__((cleanup(end_profile_block))) Profile_Start concat(            \
      profile_block_start, __LINE__) = {                                       \
      .profile_name = name,                                                    \
      .start = __rdtsc(),                                                      \
      .profile_anchor = current_anchor,                                        \
      .parent_anchor = parent_anchor,                                          \
  }

#define profile_function profile_block(__func__)

#define profile_program                                                        \
  __attribute__((cleanup(end_profile_and_print))) u64 concat(                  \
      program_start, __LINE__) = __rdtsc()
