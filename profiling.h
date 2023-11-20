#ifndef PROFILING_H
#define PROFILING_H

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

unsigned long long cur = 0;
unsigned long long last = 0;
unsigned long long last_profile = -1;

struct s_profile {
    unsigned long long counter;
    const char *func_name;
};
struct s_profile profiles[256] = {0};

static int counter = 0;

/* rdtsc */
extern __inline unsigned long long
__attribute__((__gnu_inline__, __always_inline__, __artificial__))
__rdtsc (void)
{
  return __builtin_ia32_rdtsc ();
}

#define set_profile do {\
    static int profile = -1;\
    if (profile == -1) {\
        profile = counter++;\
        profiles[profile].func_name = __func__;\
    }\
    if (last_profile != -1) {\
        cur = __rdtsc();\
        profiles[last_profile].counter +=  cur - last;\
        last = cur;\
    }\
    last_profile = profile;\
} while (0);

void print_profiles() {
    unsigned long long total = 0;

    if (last_profile != -1) {
        cur = __rdtsc();
        profiles[last_profile].counter += cur - last;
        last = cur;
    }

    for (int i = 0; i < counter; i++) {
        total += profiles[i].counter;
    }

    for (int i = 0; i < counter; i++) {
        printf("%s -> %lf\n", profiles[i].func_name, profiles[i].counter / (double)total * 100);
    }
}
#endif
