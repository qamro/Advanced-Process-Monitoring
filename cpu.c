#include <stdio.h>
#include <string.h>
#include "cpu.h"

static long prev_user[8]   = {0};
static long prev_nice[8]   = {0};
static long prev_system[8] = {0};
static long prev_idle[8]   = {0};
static long prev_iowait[8] = {0};
static long prev_irq[8]    = {0};
static long prev_softirq[8]= {0};

void get_cpu_usage_delta(double cores[]) {
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return;

    char line[256];
    int core = 0;

    while (fgets(line, sizeof(line), f) && core < 8) {
        if (strncmp(line, "cpu", 3) != 0 || line[3] == ' ') continue;

        long user, nice, system, idle, iowait, irq, softirq;
        sscanf(line, "%*s %ld %ld %ld %ld %ld %ld %ld",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq);

        long d_user    = user    - prev_user[core];
        long d_nice    = nice    - prev_nice[core];
        long d_system  = system  - prev_system[core];
        long d_idle    = idle    - prev_idle[core];
        long d_iowait  = iowait  - prev_iowait[core];
        long d_irq     = irq     - prev_irq[core];
        long d_softirq = softirq - prev_softirq[core];

        long total = d_user + d_nice + d_system + d_idle +
                     d_iowait + d_irq + d_softirq;

        cores[core] = (total > 0)
            ? (double)(d_user + d_nice + d_system + d_irq + d_softirq) / total * 100.0
            : 0.0;

        prev_user[core]    = user;
        prev_nice[core]    = nice;
        prev_system[core]  = system;
        prev_idle[core]    = idle;
        prev_iowait[core]  = iowait;
        prev_irq[core]     = irq;
        prev_softirq[core] = softirq;

        core++;
    }

    fclose(f);
}
