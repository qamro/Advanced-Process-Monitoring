#include <stdio.h>
#include <string.h>
#include "net.h"

void get_network_usage(long* rx, long* tx) {
    FILE* f = fopen("/proc/net/dev", "r");
    if (!f) return;

    char line[256];
    *rx = 0;
    *tx = 0;

    /* skip two header lines */
    (void)fgets(line, sizeof(line), f);
    (void)fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f)) {
        char iface[32];
        long r = 0, t = 0;

        /* trim leading whitespace before sscanf */
        char *p = line;
        while (*p == ' ') p++;

        if (sscanf(p, "%31[^:]: %ld %*d %*d %*d %*d %*d %*d %*d %ld",
                   iface, &r, &t) == 3) {
            /* skip loopback */
            if (strcmp(iface, "lo") == 0) continue;
            *rx += r;
            *tx += t;
        }
    }

    fclose(f);
}
