#include "cpu.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __x86_64__
#include <cpuid.h>
#endif

void cpu_detect(fgrep_cpu_features_t *f) {
    __builtin_memset(f, 0, sizeof(*f));
#ifdef __x86_64__
    unsigned int eax, ebx, ecx, edx;

    if (__get_cpuid_count(1, 0, &eax, &ebx, &ecx, &edx)) {
        f->sse2    = (edx & (1 << 29)) != 0;
        f->sse4_1  = (ecx & (1 << 19)) != 0;
        f->avx2    = false;
        f->avx512f = false;
        f->avx512bw = false;
        f->popcnt  = (ecx & (1 << 23)) != 0;
    }

    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        f->avx2    = (ebx & (1 << 5)) != 0;
        f->avx512f = (ebx & (1 << 16)) != 0;
        f->avx512bw = (ebx & (1 << 30)) != 0;
    }
#else
    f->sse2 = true;
#endif
}

const char *cpu_features_string(const fgrep_cpu_features_t *f) {
    static char buf[128];
    int n = 0;
    if (f->sse2)    n += snprintf(buf + n, sizeof(buf) - n, "SSE2 ");
    if (f->sse4_1)  n += snprintf(buf + n, sizeof(buf) - n, "SSE4.1 ");
    if (f->avx2)    n += snprintf(buf + n, sizeof(buf) - n, "AVX2 ");
    if (f->avx512f) n += snprintf(buf + n, sizeof(buf) - n, "AVX512F ");
    if (f->avx512bw)n += snprintf(buf + n, sizeof(buf) - n, "AVX512BW ");
    if (f->popcnt)  n += snprintf(buf + n, sizeof(buf) - n, "POPCNT ");
    if (n == 0)     snprintf(buf, sizeof(buf), "(none)");
    else            buf[n - 1] = '\0';
    return buf;
}
