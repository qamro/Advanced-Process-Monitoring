#include <stdio.h>
#include "system.h"

long get_total_cpu_time() {
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return 0;

    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    fclose(f);

    long user, nice, system, idle, iowait, irq, softirq;
    sscanf(line,
           "cpu %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);

    return user + nice + system + idle + iowait + irq + softirq;
}
