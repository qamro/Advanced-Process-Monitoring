#include <stdio.h>
#include <string.h>
#include "disk.h"

void get_disk_io(long* read_sectors, long* write_sectors) {
    FILE* f = fopen("/proc/diskstats", "r");
    if (!f) return;

    char line[256];
    *read_sectors  = 0;
    *write_sectors = 0;

    while (fgets(line, sizeof(line), f)) {
        int major, minor;
        char name[32];
        long r = 0, w = 0;

        if (sscanf(line, "%d %d %31s", &major, &minor, name) < 3) continue;

        /* match whole disks: sda, vda, nvme0n1, hda — skip partitions */
        int is_disk = 0;
        if (strncmp(name, "sd",   2) == 0 && (name[3] == '\0')) is_disk = 1;
        if (strncmp(name, "vd",   2) == 0 && (name[3] == '\0')) is_disk = 1;
        if (strncmp(name, "hd",   2) == 0 && (name[3] == '\0')) is_disk = 1;
        /* nvme0n1 style: ends with n<digit> */
        if (strncmp(name, "nvme", 4) == 0) {
            char *n = strrchr(name, 'n');
            if (n && *(n+1) >= '0' && *(n+1) <= '9' && *(n+2) == '\0') is_disk = 1;
        }

        if (!is_disk) continue;

        sscanf(line,
               "%d %d %*s "
               "%*d %*d %ld %*d "   /* reads: merged, sectors, ms */
               "%*d %*d %ld",       /* writes: merged, sectors */
               &major, &minor, &r, &w);

        *read_sectors  += r;
        *write_sectors += w;
    }

    fclose(f);
}
