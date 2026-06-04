#include <ncurses.h>
#include <unistd.h>
#include <time.h>
#include "mem.h"
#include "cpu.h"
#include "disk.h"
#include "net.h"
#include "ui.h"

int main() {
    double cores[8] = {0};
    long rx, tx;
    long prev_rx = 0, prev_tx = 0;
    long disk_r, disk_w;
    long prev_disk_r = 0, prev_disk_w = 0;

    time_t start_time = time(NULL);

    init_ui();

    /* explicitly disable mouse events so cursor movement is ignored */
    mousemask(0, NULL);

    /* warm-up read so first-frame deltas are valid */
    get_cpu_usage_delta(cores);
    get_network_usage(&prev_rx, &prev_tx);
    get_disk_io(&prev_disk_r, &prev_disk_w);
    sleep(1);

    /* non-blocking getch — we control timing with sleep(1) */
    nodelay(stdscr, TRUE);

    while (1) {
        /* collect data AFTER the 1-second sleep so deltas are real */
        double mem = get_memory_usage();
        get_cpu_usage_delta(cores);
        get_network_usage(&rx, &tx);
        get_disk_io(&disk_r, &disk_w);

        long net_rx_delta = rx - prev_rx;
        long net_tx_delta = tx - prev_tx;
        long disk_r_delta = disk_r - prev_disk_r;
        long disk_w_delta = disk_w - prev_disk_w;

        prev_rx = rx;       prev_tx = tx;
        prev_disk_r = disk_r; prev_disk_w = disk_w;

        time_t uptime_secs = time(NULL) - start_time;

        draw_ui(mem, cores, net_rx_delta, net_tx_delta,
                disk_r_delta, disk_w_delta, uptime_secs);

        /* drain ALL pending input, only act on 'q' */
        int ch;
        while ((ch = getch()) != ERR) {
            if (ch == 'q' || ch == 'Q') {
                endwin();
                return 0;
            }
        }

        sleep(1);
    }

    endwin();
    return 0;
}
