/*  ui.c  -  GHOST MONITOR
 *
 *  100% ASCII + ncurses ACS line-drawing only.
 *  No UTF-8 box chars, no Unicode sparklines, no multibyte anything.
 *
 *  Color pairs:
 *    1  GREEN   - low load / good
 *    2  CYAN    - headers, borders, labels
 *    3  AMBER   - medium load 50-79%
 *    4  RED     - high load 80%+
 *    5  DIM     - decorative / inactive
 *    6  WHITE   - values, numbers
 *    7  MAGENTA - disk panel accent
 *    8  HEADER  - top bar (black on cyan)
 */

#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "ui.h"
#include "mem.h"

/* colour pair IDs */
#define C_GREEN   1
#define C_CYAN    2
#define C_AMBER   3
#define C_RED     4
#define C_DIM     5
#define C_WHITE   6
#define C_MAGENTA 7
#define C_HEADER  8

/* sparkline history */
static double cpu_spark[MAX_CORES][SPARK_LEN];
static double mem_spark[SPARK_LEN];
static double net_spark[SPARK_LEN];
static int    spark_pos = 0;
static int    frame     = 0;

/* ASCII sparkline chars - 5 levels, always single-byte */
static const char SPARK_CHARS[] = { ' ', '.', ':', 'i', '|', 'I', '#' };
#define SPARK_LEVELS 7

/* bar chars - pure ASCII, single byte */
#define BAR_FILL '|'
#define BAR_MID  '+'
#define BAR_LOW  '-'
#define BAR_EMPTY '.'

/* =========================================================================
 * helpers
 * ========================================================================= */

static int color_for(double pct) {
    if (pct >= 80.0) return C_RED;
    if (pct >= 50.0) return C_AMBER;
    return C_GREEN;
}

static void set_fg(int pair)  { attron(COLOR_PAIR(pair));  }
static void clr_fg(int pair)  { attroff(COLOR_PAIR(pair)); }

/* draw a box using ncurses ACS_ line-drawing (uses terminal's own charset) */
static void draw_box(int y, int x, int h, int w, const char *title) {
    set_fg(C_CYAN);

    /* top-left corner */
    mvaddch(y, x, ACS_ULCORNER);

    if (title && title[0]) {
        char label[64];
        snprintf(label, sizeof(label), "[ %s ]", title);
        int tlen = (int)strlen(label);
        int pad  = (w - 2 - tlen) / 2;

        for (int i = 0; i < pad; i++) addch(ACS_HLINE);

        clr_fg(C_CYAN);
        attron(COLOR_PAIR(C_WHITE) | A_BOLD);
        printw("%s", label);
        attroff(COLOR_PAIR(C_WHITE) | A_BOLD);
        set_fg(C_CYAN);

        int drawn = pad + tlen;
        for (int i = drawn; i < w - 2; i++) addch(ACS_HLINE);
    } else {
        for (int i = 0; i < w - 2; i++) addch(ACS_HLINE);
    }

    addch(ACS_URCORNER);

    /* sides */
    for (int row = 1; row < h - 1; row++) {
        mvaddch(y + row, x,         ACS_VLINE);
        mvaddch(y + row, x + w - 1, ACS_VLINE);
    }

    /* bottom */
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    for (int i = 0; i < w - 2; i++) addch(ACS_HLINE);
    addch(ACS_LRCORNER);

    clr_fg(C_CYAN);
}

/* gradient bar: ASCII chars, color-coded by load */
static void draw_bar(int y, int x, int width, double pct) {
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;

    int fill  = (int)(pct * width / 100.0);
    int cpair = color_for(pct);

    move(y, x);
    set_fg(cpair);
    attron(A_BOLD);

    for (int i = 0; i < width; i++) {
        if (i < fill) {
            double pos = (double)i / (fill > 0 ? fill : 1);
            if      (pos > 0.75) addch(BAR_FILL);
            else if (pos > 0.4)  addch(BAR_MID);
            else                 addch(BAR_LOW);
        } else {
            attroff(A_BOLD);
            clr_fg(cpair);
            set_fg(C_DIM);
            addch(BAR_EMPTY);
            clr_fg(C_DIM);
            set_fg(cpair);
            attron(A_BOLD);
        }
    }

    attroff(A_BOLD);
    clr_fg(cpair);
}

/* ASCII sparkline */
static void draw_sparkline(int y, int x, int width,
                           double *data, int len, int cpair) {
    double max_val = 0.01;
    for (int i = 0; i < len; i++)
        if (data[i] > max_val) max_val = data[i];

    int start = (len > width) ? len - width : 0;

    set_fg(cpair);
    for (int i = start; i < len; i++) {
        double norm = data[i] / max_val;
        int    idx  = (int)(norm * (SPARK_LEVELS - 1));
        if (idx >= SPARK_LEVELS) idx = SPARK_LEVELS - 1;
        mvaddch(y, x + (i - start), SPARK_CHARS[idx]);
    }
    clr_fg(cpair);
}

/* format bytes/s */
static void fmt_bytes(char *buf, size_t sz, long bps) {
    if (bps < 0) bps = 0;
    if      (bps >= 1024L*1024*1024) snprintf(buf, sz, "%5.1f GB/s", bps/(1024.0*1024*1024));
    else if (bps >= 1024*1024)       snprintf(buf, sz, "%5.1f MB/s", bps/(1024.0*1024));
    else if (bps >= 1024)            snprintf(buf, sz, "%5.1f KB/s", bps/1024.0);
    else                             snprintf(buf, sz, "%5ld  B/s",  bps);
}

static void fmt_sectors(char *buf, size_t sz, long sectors) {
    fmt_bytes(buf, sz, sectors * 512);
}

/* =========================================================================
 * init_ui
 * ========================================================================= */

void init_ui() {
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    start_color();
    use_default_colors();

    init_pair(C_GREEN,   COLOR_GREEN,   -1);
    init_pair(C_CYAN,    COLOR_CYAN,    -1);
    init_pair(C_AMBER,   COLOR_YELLOW,  -1);
    init_pair(C_RED,     COLOR_RED,     -1);
    init_pair(C_DIM,     COLOR_WHITE,   -1);   /* fallback: white dim */
    init_pair(C_WHITE,   COLOR_WHITE,   -1);
    init_pair(C_MAGENTA, COLOR_MAGENTA, -1);
    init_pair(C_HEADER,  COLOR_BLACK,   COLOR_CYAN);

    if (COLORS >= 256) {
        init_pair(C_DIM, 240, -1);
    }

    memset(cpu_spark, 0, sizeof(cpu_spark));
    memset(mem_spark, 0, sizeof(mem_spark));
    memset(net_spark, 0, sizeof(net_spark));
}

/* =========================================================================
 * draw_ui
 * ========================================================================= */

void draw_ui(double mem, double cores[], long net_rx, long net_tx,
             long disk_r, long disk_w, long uptime_secs) {

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    clear();
    frame++;

    /* update sparkline history */
    for (int i = 0; i < MAX_CORES; i++)
        cpu_spark[i][spark_pos] = cores[i];
    mem_spark[spark_pos] = mem;
    net_spark[spark_pos] = (double)(net_rx / 1024.0);
    spark_pos = (spark_pos + 1) % SPARK_LEN;

    /* layout */
    int left_w  = cols / 2;
    int right_w = cols - left_w;
    int cpu_h   = 3 + MAX_CORES + 2;
    int mem_h   = 7;
    int net_h   = 6;
    int disk_h  = 6;
    int status_h = 3;

    /* ---- HEADER BAR ---------------------------------------------------- */
    attron(COLOR_PAIR(C_HEADER) | A_BOLD);
    for (int c = 0; c < cols; c++) mvaddch(0, c, ' ');

    /* animated spinner */
    const char spinners[] = { '-', '\\', '|', '/' };
    mvprintw(0, 1, " [%c] GHOST MONITOR", spinners[frame % 4]);

    char hostname[64] = "localhost";
    gethostname(hostname, sizeof(hostname));
    mvprintw(0, 22, " %s", hostname);

    long h = uptime_secs / 3600;
    long m = (uptime_secs % 3600) / 60;
    long s = uptime_secs % 60;
    char upstr[32];
    snprintf(upstr, sizeof(upstr), "UP %02ld:%02ld:%02ld ", h, m, s);
    mvprintw(0, cols - (int)strlen(upstr) - 1, "%s", upstr);

    attroff(COLOR_PAIR(C_HEADER) | A_BOLD);

    /* ---- CPU PANEL (left, top) ----------------------------------------- */
    int cy = 1, cx = 0;
    draw_box(cy, cx, cpu_h, left_w, "CPU CORES");

    /* count actual cores */
    int num_cores = 0;
    {
        FILE *f = fopen("/proc/stat", "r");
        char line[64];
        if (f) {
            (void)fgets(line, sizeof(line), f);
            while (fgets(line, sizeof(line), f) && num_cores < MAX_CORES)
                if (strncmp(line, "cpu", 3) == 0 && line[3] != ' ') num_cores++;
            fclose(f);
        }
    }
    if (num_cores == 0) num_cores = 4;

    int bar_w = left_w - 20;

    for (int i = 0; i < num_cores && i < MAX_CORES; i++) {
        int row = cy + 1 + i;
        double pct = cores[i];
        int    cp  = color_for(pct);

        set_fg(C_DIM);
        mvprintw(row, cx + 2, "CPU%-2d", i);
        clr_fg(C_DIM);

        draw_bar(row, cx + 7, bar_w, pct);

        set_fg(cp); attron(A_BOLD);
        mvprintw(row, cx + 7 + bar_w + 1, "%5.1f%%", pct);
        attroff(A_BOLD); clr_fg(cp);

        /* mini sparkline: "|" separator then 8 ASCII chars */
        set_fg(C_DIM);
        mvaddch(row, cx + 7 + bar_w + 9, ACS_VLINE);
        clr_fg(C_DIM);
        draw_sparkline(row, cx + 7 + bar_w + 10, 8,
                       cpu_spark[i], SPARK_LEN, cp);
    }

    /* aggregate history sparkline */
    int spark_row = cy + 1 + num_cores;
    set_fg(C_DIM);
    mvprintw(spark_row, cx + 2, "HIST");
    clr_fg(C_DIM);

    double avg_hist[SPARK_LEN];
    for (int t = 0; t < SPARK_LEN; t++) {
        double sum = 0;
        for (int i = 0; i < num_cores; i++) sum += cpu_spark[i][t];
        avg_hist[t] = sum / num_cores;
    }
    draw_sparkline(spark_row, cx + 7, left_w - 10, avg_hist, SPARK_LEN, C_GREEN);

    /* ---- MEMORY PANEL (left, below CPU) --------------------------------- */
    int my = cy + cpu_h;
    draw_box(my, cx, mem_h, left_w, "MEMORY");

    long total_kb = 0, used_kb = 0, cached_kb = 0;
    get_memory_info(&total_kb, &used_kb, &cached_kb);

    int mem_bar_w = left_w - 20;

    set_fg(C_DIM); mvprintw(my + 1, cx + 2, "USED "); clr_fg(C_DIM);
    draw_bar(my + 1, cx + 8, mem_bar_w, mem);
    set_fg(color_for(mem)); attron(A_BOLD);
    mvprintw(my + 1, cx + 8 + mem_bar_w + 1, "%5.1f%%", mem);
    attroff(A_BOLD); clr_fg(color_for(mem));

    double cache_pct = total_kb > 0 ? (double)cached_kb / total_kb * 100.0 : 0;
    set_fg(C_DIM); mvprintw(my + 2, cx + 2, "CACHE"); clr_fg(C_DIM);
    draw_bar(my + 2, cx + 8, mem_bar_w, cache_pct);
    set_fg(C_CYAN);
    mvprintw(my + 2, cx + 8 + mem_bar_w + 1, "%5.1f%%", cache_pct);
    clr_fg(C_CYAN);

    double used_gb  = used_kb   / (1024.0 * 1024.0);
    double total_gb = total_kb  / (1024.0 * 1024.0);
    double cache_gb = cached_kb / (1024.0 * 1024.0);
    set_fg(C_DIM);
    mvprintw(my + 3, cx + 2,
             "%.2fG used / %.2fG cache / %.2fG total",
             used_gb, cache_gb, total_gb);
    clr_fg(C_DIM);

    set_fg(C_DIM); mvprintw(my + 4, cx + 2, "HIST"); clr_fg(C_DIM);
    draw_sparkline(my + 4, cx + 7, left_w - 10, mem_spark, SPARK_LEN, color_for(mem));

    /* ---- NETWORK PANEL (right, top) ------------------------------------- */
    int nx = left_w, ny = 1;
    draw_box(ny, nx, net_h, right_w, "NETWORK I/O");

    char rxbuf[24], txbuf[24];
    fmt_bytes(rxbuf, sizeof(rxbuf), net_rx);
    fmt_bytes(txbuf, sizeof(txbuf), net_tx);

    int net_bar_w = right_w - 22;
    double rx_pct = (net_rx > 0) ? (log10((double)net_rx + 1) / 7.0 * 100.0) : 0;
    double tx_pct = (net_tx > 0) ? (log10((double)net_tx + 1) / 7.0 * 100.0) : 0;
    if (rx_pct > 100) rx_pct = 100;
    if (tx_pct > 100) tx_pct = 100;

    set_fg(C_DIM); mvprintw(ny + 1, nx + 2, "v RX  "); clr_fg(C_DIM);
    draw_bar(ny + 1, nx + 8, net_bar_w, rx_pct);
    set_fg(C_CYAN); attron(A_BOLD);
    mvprintw(ny + 1, nx + 8 + net_bar_w + 1, "%s", rxbuf);
    attroff(A_BOLD); clr_fg(C_CYAN);

    set_fg(C_DIM); mvprintw(ny + 2, nx + 2, "^ TX  "); clr_fg(C_DIM);
    draw_bar(ny + 2, nx + 8, net_bar_w, tx_pct);
    set_fg(C_GREEN); attron(A_BOLD);
    mvprintw(ny + 2, nx + 8 + net_bar_w + 1, "%s", txbuf);
    attroff(A_BOLD); clr_fg(C_GREEN);

    set_fg(C_DIM); mvprintw(ny + 3, nx + 2, "HIST"); clr_fg(C_DIM);
    draw_sparkline(ny + 3, nx + 7, right_w - 10, net_spark, SPARK_LEN, C_CYAN);

    set_fg(C_DIM);
    mvprintw(ny + 4, nx + 2, "session: accumulating rx+tx since start");
    clr_fg(C_DIM);

    /* ---- DISK PANEL (right, below net) ---------------------------------- */
    int dy = ny + net_h;
    draw_box(dy, nx, disk_h, right_w, "DISK I/O");

    char rbuf[24], wbuf[24];
    fmt_sectors(rbuf, sizeof(rbuf), disk_r);
    fmt_sectors(wbuf, sizeof(wbuf), disk_w);

    long disk_r_bytes = disk_r * 512;
    long disk_w_bytes = disk_w * 512;
    double dr_pct = (disk_r_bytes > 0) ? (log10((double)disk_r_bytes+1)/9.0*100.0) : 0;
    double dw_pct = (disk_w_bytes > 0) ? (log10((double)disk_w_bytes+1)/9.0*100.0) : 0;
    if (dr_pct > 100) dr_pct = 100;
    if (dw_pct > 100) dw_pct = 100;

    int disk_bar_w = right_w - 22;

    set_fg(C_DIM); mvprintw(dy + 1, nx + 2, "> READ"); clr_fg(C_DIM);
    draw_bar(dy + 1, nx + 9, disk_bar_w, dr_pct);
    set_fg(C_MAGENTA); attron(A_BOLD);
    mvprintw(dy + 1, nx + 9 + disk_bar_w + 1, "%s", rbuf);
    attroff(A_BOLD); clr_fg(C_MAGENTA);

    set_fg(C_DIM); mvprintw(dy + 2, nx + 2, "> WRIT"); clr_fg(C_DIM);
    draw_bar(dy + 2, nx + 9, disk_bar_w, dw_pct);
    set_fg(C_AMBER); attron(A_BOLD);
    mvprintw(dy + 2, nx + 9 + disk_bar_w + 1, "%s", wbuf);
    attroff(A_BOLD); clr_fg(C_AMBER);

    set_fg(C_DIM);
    mvprintw(dy + 3, nx + 2, "sectors x512B  |  delta since last tick");
    clr_fg(C_DIM);

    /* ---- QUICK STATS (right, below disk) -------------------------------- */
    int tile_y = dy + disk_h;
    if (tile_y + 4 < rows - status_h) {
        draw_box(tile_y, nx, 4, right_w, "QUICK STATS");

        double load1 = 0, load5 = 0, load15 = 0;
        FILE *lf = fopen("/proc/loadavg", "r");
        if (lf) { (void)fscanf(lf, "%lf %lf %lf", &load1, &load5, &load15); fclose(lf); }

        int tw  = (right_w - 2) / 3;
        int t1x = nx + 1;
        int t2x = nx + 1 + tw;
        int t3x = nx + 1 + 2*tw;

        set_fg(C_DIM);
        mvprintw(tile_y + 1, t1x + 1, "LOAD 1m");
        mvprintw(tile_y + 1, t2x + 1, "LOAD 5m");
        mvprintw(tile_y + 1, t3x + 1, "LOAD 15m");
        clr_fg(C_DIM);

        int lcp1 = (load1 > 2.0) ? C_RED : (load1 > 1.0) ? C_AMBER : C_GREEN;
        int lcp5 = (load5 > 2.0) ? C_RED : (load5 > 1.0) ? C_AMBER : C_GREEN;

        set_fg(lcp1); attron(A_BOLD);
        mvprintw(tile_y + 2, t1x + 1, "%.2f", load1);
        attroff(A_BOLD); clr_fg(lcp1);

        set_fg(lcp5); attron(A_BOLD);
        mvprintw(tile_y + 2, t2x + 1, "%.2f", load5);
        attroff(A_BOLD); clr_fg(lcp5);

        set_fg(C_CYAN); attron(A_BOLD);
        mvprintw(tile_y + 2, t3x + 1, "%.2f", load15);
        attroff(A_BOLD); clr_fg(C_CYAN);
    }

    /* ---- STATUS BAR (bottom) -------------------------------------------- */
    int sb = rows - 1;

    set_fg(C_DIM);
    for (int c = 0; c < cols; c++) mvaddch(sb - 1, c, ACS_HLINE);
    clr_fg(C_DIM);

    attron(COLOR_PAIR(C_HEADER));
    for (int c = 0; c < cols; c++) mvaddch(sb, c, ' ');
    mvprintw(sb, 1, " [Q] Quit   frame:%d   %dx%d", frame, cols, rows);

    /* critical alert */
    for (int i = 0; i < num_cores; i++) {
        if (cores[i] >= 80.0) {
            attroff(COLOR_PAIR(C_HEADER));
            attron(COLOR_PAIR(C_RED) | A_BOLD | A_BLINK);
            mvprintw(sb, cols - 20, " !! CPU%d CRITICAL !! ", i);
            attroff(COLOR_PAIR(C_RED) | A_BOLD | A_BLINK);
            break;
        }
    }
    attroff(COLOR_PAIR(C_HEADER));

    set_fg(C_DIM);
    mvprintw(rows - 2, cols - 13, "ghost v1.0 <>");
    clr_fg(C_DIM);

    refresh();
}
