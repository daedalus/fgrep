#ifndef CPU_H
#define CPU_H

#include <stdbool.h>

typedef struct {
    bool sse2;
    bool sse4_1;
    bool avx2;
    bool avx512f;
    bool avx512bw;
    bool popcnt;
} fgrep_cpu_features_t;

void cpu_detect(fgrep_cpu_features_t *features);
const char *cpu_features_string(const fgrep_cpu_features_t *features);

#endif
