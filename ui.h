#ifndef UI_H
#define UI_H

#define MAX_CORES      8
#define SPARK_LEN      40   /* sparkline history samples */

void init_ui();
void draw_ui(double mem, double cores[], long net_rx, long net_tx,
             long disk_r, long disk_w, long uptime_secs);

#endif
