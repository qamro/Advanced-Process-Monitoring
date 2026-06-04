#include <stdio.h>
#include <string.h>
#include "mem.h"

double get_memory_usage() {
    return get_memory_info(NULL, NULL, NULL);
}

double get_memory_info(long *out_total_kb, long *out_used_kb, long *out_cached_kb) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0.0;

    long total = 0, available = 0, cached = 0, buffers = 0;
    char key[64];
    long value;

    while (fscanf(f, "%63s %ld %*s", key, &value) == 2) {
        if (strcmp(key, "MemTotal:")    == 0) total     = value;
        if (strcmp(key, "MemAvailable:")== 0) available = value;
        if (strcmp(key, "Cached:")      == 0) cached    = value;
        if (strcmp(key, "Buffers:")     == 0) buffers   = value;
    }

    fclose(f);

    long used = total - available;
    if (out_total_kb)  *out_total_kb  = total;
    if (out_used_kb)   *out_used_kb   = used;
    if (out_cached_kb) *out_cached_kb = cached + buffers;

    return (total > 0) ? ((double)used / total) * 100.0 : 0.0;
}
