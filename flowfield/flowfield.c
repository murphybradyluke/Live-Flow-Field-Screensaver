/*
 * flowfield.c -- a Perlin-noise flow field screensaver for the terminal.
 *
 * The same sketch as the Processing version: particles follow a smooth noise
 * field and leave fading trails. Here the canvas is a grid of Braille dots
 * (2 x 4 per terminal cell), so a 200 x 50 terminal is a 400 x 200 picture.
 *
 * Built for low cost: a few thousand noise evaluations per frame, one
 * write() per frame, and only cells that changed since the last frame are
 * sent to the terminal.
 *
 * Exits on any key, mouse movement, focus loss, or SIGTERM/SIGINT/SIGHUP.
 *
 *   cc -O2 -o omarchy-flowfield flowfield.c -lm
 *
 * Options:
 *   --fps N          frames per second (default 20)
 *   --density N      particles per 1000 dots (default 20)
 *   --fade N         trail persistence, 90..99 (default 96)
 *   --frames N       stop after N frames (for benchmarks)
 *   --size CxR       pretend the terminal is C columns by R rows (benchmarks)
 */
#define _GNU_SOURCE
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* ----------------------------------------------------------------------
 * Perlin noise (Ken Perlin's improved noise, 2002), seeded per run.
 * ---------------------------------------------------------------------- */
static int perm[512];

static void perlin_seed(unsigned seed) {
    int p[256];
    for (int i = 0; i < 256; i++) p[i] = i;
    srand(seed);
    for (int i = 255; i > 0; i--) {
        int j = rand() % (i + 1), t = p[i];
        p[i] = p[j];
        p[j] = t;
    }
    for (int i = 0; i < 512; i++) perm[i] = p[i & 255];
}

static inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float lerpf(float t, float a, float b) { return a + t * (b - a); }
static inline float grad(int h, float x, float y, float z) {
    h &= 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

/* Returns roughly 0..1. */
static float noise3(float x, float y, float z) {
    int X = (int)floorf(x) & 255, Y = (int)floorf(y) & 255, Z = (int)floorf(z) & 255;
    x -= floorf(x); y -= floorf(y); z -= floorf(z);
    float u = fade(x), v = fade(y), w = fade(z);
    int A = perm[X] + Y, AA = perm[A] + Z, AB = perm[A + 1] + Z;
    int B = perm[X + 1] + Y, BA = perm[B] + Z, BB = perm[B + 1] + Z;
    float n = lerpf(w,
        lerpf(v, lerpf(u, grad(perm[AA], x, y, z), grad(perm[BA], x - 1, y, z)),
                 lerpf(u, grad(perm[AB], x, y - 1, z), grad(perm[BB], x - 1, y - 1, z))),
        lerpf(v, lerpf(u, grad(perm[AA + 1], x, y, z - 1), grad(perm[BA + 1], x - 1, y, z - 1)),
                 lerpf(u, grad(perm[AB + 1], x, y - 1, z - 1), grad(perm[BB + 1], x - 1, y - 1, z - 1))));
    n = n * 0.5f + 0.5f;
    return n < 0 ? 0 : (n > 1 ? 1 : n);
}

/* ----------------------------------------------------------------------
 * Terminal plumbing.
 * ---------------------------------------------------------------------- */
static volatile sig_atomic_t quit_flag = 0, resize_flag = 0;
static int stdin_closed = 0;
static struct termios saved_tio;
static int tio_saved = 0;
static int forced_cols = 0, forced_rows = 0;

static void on_signal(int sig) { (void)sig; quit_flag = 1; }
static void on_winch(int sig) { (void)sig; resize_flag = 1; }

static void term_size(int *cols, int *rows) {
    if (forced_cols) { *cols = forced_cols; *rows = forced_rows; return; }
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        *cols = ws.ws_col; *rows = ws.ws_row;
    } else { *cols = 80; *rows = 24; }
}

static void write_all(const char *s, size_t n) {
    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, s, n);
        if (w < 0) return;
        s += w; n -= (size_t)w;
    }
}

static void term_enter(void) {
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved_tio) == 0) {
        struct termios raw = saved_tio;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        tio_saved = 1;
    }
    /* alt screen, hide cursor, black background, clear, mouse-motion + focus reporting */
    const char *s = "\x1b[?1049h\x1b[?25l\x1b[48;2;0;0;0m\x1b[2J\x1b[H\x1b[?1003h\x1b[?1004h";
    write_all(s, strlen(s));
}

static void term_leave(void) {
    const char *s = "\x1b[?1004l\x1b[?1003l\x1b[0m\x1b[2J\x1b[H\x1b[?25h\x1b[?1049l";
    write_all(s, strlen(s));
    if (tio_saved) tcsetattr(STDIN_FILENO, TCSANOW, &saved_tio);
}

/* True when the user did something: any key, mouse motion, or focus-out.
 * A focus-in report (ESC [ I) arrives on its own when reporting is enabled
 * and is ignored. */
static int input_wants_exit(void) {
    unsigned char buf[256];
    ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
    if (n == 0) { stdin_closed = 1; return 0; }
    if (n < 0) return 0;
    ssize_t i = 0;
    while (i < n) {
        if (n - i >= 3 && buf[i] == 0x1b && buf[i + 1] == '[' && buf[i + 2] == 'I') { i += 3; continue; }
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------
 * The sketch.
 * ---------------------------------------------------------------------- */
typedef struct { float x, y; } particle;

static int cols, rows, W, H;
static uint8_t *heat, *tint;        /* per dot: brightness, and the noise value that lit it */
static uint32_t *prev_cells;        /* per cell: what was last drawn, to send only changes */
static particle *parts;
static int nparts;
static char *outbuf;
static size_t outcap;
static uint8_t decay[256];

static int opt_fps = 20, opt_density = 20, opt_fade = 96, opt_frames = 0;

static void hsv_to_rgb(float h, float s, float v, int *r, int *g, int *b) {
    h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = v - c;
    float rf, gf, bf;
    if (h < 60)       { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else              { rf = c; gf = 0; bf = x; }
    *r = (int)((rf + m) * 255); *g = (int)((gf + m) * 255); *b = (int)((bf + m) * 255);
}

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static void respawn(particle *p) { p->x = frand() * W; p->y = frand() * H; }

static void setup_canvas(void) {
    term_size(&cols, &rows);
    W = cols * 2; H = rows * 4;
    free(heat); free(tint); free(prev_cells); free(parts); free(outbuf);
    heat = calloc((size_t)W * H, 1);
    tint = calloc((size_t)W * H, 1);
    prev_cells = calloc((size_t)cols * rows, sizeof *prev_cells);
    nparts = (int)((long)W * H * opt_density / 1000);
    if (nparts < 100) nparts = 100;
    if (nparts > 8000) nparts = 8000;
    parts = malloc((size_t)nparts * sizeof *parts);
    for (int i = 0; i < nparts; i++) respawn(&parts[i]);
    outcap = (size_t)cols * rows * 28 + 64;
    outbuf = malloc(outcap);
    const char *clr = "\x1b[2J";
    write_all(clr, strlen(clr));
}

static void build_decay(void) {
    for (int v = 0; v < 256; v++) {
        int d = (int)(v * (opt_fade / 100.0f));
        if (d >= v && v > 0) d = v - 1;
        decay[v] = (uint8_t)d;
    }
}

static inline size_t put_u(char *dst, int v) {
    char tmp[12]; int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (int i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
    return (size_t)n;
}

static inline size_t put_move(char *dst, int row, int col) {
    size_t o = 0;
    dst[o++] = 0x1b; dst[o++] = '[';
    o += put_u(dst + o, row); dst[o++] = ';';
    o += put_u(dst + o, col); dst[o++] = 'H';
    return o;
}

static inline size_t put_color(char *dst, int r, int g, int b) {
    size_t o = 0;
    memcpy(dst, "\x1b[38;2;", 7); o = 7;
    o += put_u(dst + o, r); dst[o++] = ';';
    o += put_u(dst + o, g); dst[o++] = ';';
    o += put_u(dst + o, b); dst[o++] = 'm';
    return o;
}

/* Braille bit for dot (dx, dy) inside a cell. */
static const uint8_t BIT[4][2] = { {0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80} };

#define LEVELS 8

static void step(float t) {
    /* Slowly drifting knobs, the screensaver trick from the browser version. */
    float zoom  = 0.006f + 0.018f * noise3(t * 0.05f, 10.0f, 0.0f);
    float turns = 1.0f + 3.0f * noise3(t * 0.07f, 20.0f, 0.0f);
    float ftime = t * 0.08f;

    for (int i = 0; i < nparts; i++) {
        particle *p = &parts[i];
        for (int s = 0; s < 2; s++) {
            float n = noise3(p->x * zoom, p->y * zoom, ftime);
            float a = n * 6.2831853f * turns;
            p->x += cosf(a); p->y += sinf(a);
            if (p->x < 0 || p->x >= W || p->y < 0 || p->y >= H) { respawn(p); break; }
            size_t k = (size_t)(int)p->y * W + (int)p->x;
            heat[k] = 255;
            tint[k] = (uint8_t)(n * 255);
        }
        if (frand() < 0.004f) respawn(p);
    }
}

static void render(float t) {
    float hue_base = 360.0f * noise3(t * 0.02f, 30.0f, 0.0f);
    size_t o = 0;
    int cur_r = -1, cur_c = -1;          /* where the terminal cursor is now */
    uint32_t last_color = 0xffffffff;

    for (int cy = 0; cy < rows; cy++) {
        for (int cx = 0; cx < cols; cx++) {
            uint8_t bits = 0, peak = 0, tn = 0;
            for (int dy = 0; dy < 4; dy++) {
                size_t k = (size_t)(cy * 4 + dy) * W + cx * 2;
                for (int dx = 0; dx < 2; dx++) {
                    uint8_t h = heat[k + dx];
                    if (h > 24) {
                        bits |= BIT[dy][dx];
                        if (h > peak) { peak = h; tn = tint[k + dx]; }
                    }
                }
            }
            int level = peak * LEVELS / 256;
            int tq = tn >> 5;                       /* 8 tint bands, keeps colors stable */
            uint32_t cell = bits ? (uint32_t)bits | ((uint32_t)level << 8) | ((uint32_t)tq << 16) : 0;
            size_t ci = (size_t)cy * cols + cx;
            if (cell == prev_cells[ci]) continue;
            prev_cells[ci] = cell;

            if (cur_r != cy || cur_c != cx)
                o += put_move(outbuf + o, cy + 1, cx + 1);
            if (!bits) {
                outbuf[o++] = ' ';
            } else {
                uint32_t color = ((uint32_t)level << 8) | (uint32_t)tq;
                if (color != last_color) {
                    int r, g, b;
                    float v = 0.22f + 0.78f * (float)level / (LEVELS - 1);
                    hsv_to_rgb(hue_base + tq * 11.0f, 0.72f, v, &r, &g, &b);
                    o += put_color(outbuf + o, r, g, b);
                    last_color = color;
                }
                outbuf[o++] = (char)0xE2;
                outbuf[o++] = (char)(0xA0 | (bits >> 6));
                outbuf[o++] = (char)(0x80 | (bits & 0x3F));
            }
            cur_r = cy; cur_c = cx + 1;
        }
    }
    if (o) write_all(outbuf, o);

    /* Fade every dot a little for next time. */
    size_t n = (size_t)W * H;
    for (size_t k = 0; k < n; k++) heat[k] = decay[heat[k]];
}

static double now_s(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fps") && i + 1 < argc) opt_fps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--density") && i + 1 < argc) opt_density = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fade") && i + 1 < argc) opt_fade = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) opt_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &forced_cols, &forced_rows);
        else { fprintf(stderr, "usage: %s [--fps N] [--density N] [--fade 90..99] [--frames N] [--size CxR]\n", argv[0]); return 2; }
    }
    if (opt_fps < 1) opt_fps = 1;
    if (opt_fps > 120) opt_fps = 120;
    if (opt_fade < 80) opt_fade = 80;
    if (opt_fade > 99) opt_fade = 99;

    unsigned seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    perlin_seed(seed);
    build_decay();

    signal(SIGINT, on_signal); signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal); signal(SIGQUIT, on_signal);
    signal(SIGWINCH, on_winch);

    term_enter();
    setup_canvas();

    double t0 = now_s(), frame_dt = 1.0 / opt_fps, next = t0;
    long frames = 0;
    while (!quit_flag) {
        if (resize_flag) { resize_flag = 0; setup_canvas(); }
        float t = (float)(now_s() - t0);
        step(t);
        render(t);
        frames++;
        if (opt_frames && frames >= opt_frames) break;

        /* Sleep until the next frame, but wake instantly on input.
         * Input in the first 1.5 s is ignored: the launcher is still moving
         * focus around, which can look like mouse motion. */
        next += frame_dt;
        for (;;) {
            double wait = next - now_s();
            if (wait <= 0) break;
            fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
            struct timeval tv = { (time_t)wait, (suseconds_t)((wait - (time_t)wait) * 1e6) };
            int r = select(stdin_closed ? 0 : STDIN_FILENO + 1, stdin_closed ? NULL : &fds, NULL, NULL, &tv);
            if (r > 0) {
                int wants_exit = input_wants_exit();
                if (wants_exit && t > 1.5f) { quit_flag = 1; break; }
            } else if (r < 0 && quit_flag) break;
            else if (r == 0) break;
        }
        if (now_s() > next + 0.5) next = now_s();   /* fell far behind: don't try to catch up */
    }

    term_leave();
    return 0;
}
