/*
 * render.h - draws the animated blue/green glow around a pinned window.
 * Pure integer code, no dependencies (also compiled on its own for previews).
 */
#pragma once

typedef struct { int l, t, r, b; } RRect;

/* Colour stops of the flowing gradient (R, G, B). */
#define RG_BLUE_R  0x1E
#define RG_BLUE_G  0x8C
#define RG_BLUE_B  0xFF
#define RG_GREEN_R 0x00
#define RG_GREEN_G 0xE6
#define RG_GREEN_B 0x8C

static unsigned int rg_lut[256];   /* cyclic blue -> green -> blue, 0x00RRGGBB */

static void rg_build_lut(void)
{
    for (int i = 0; i < 256; i++) {
        int t = i < 128 ? i * 2 : (255 - i) * 2;          /* triangle 0..254 */
        int s = (t * t * (765 - 2 * t)) / 65025;          /* smoothstep, 0..~254 */
        int r = RG_BLUE_R + (RG_GREEN_R - RG_BLUE_R) * s / 255;
        int g = RG_BLUE_G + (RG_GREEN_G - RG_BLUE_G) * s / 255;
        int b = RG_BLUE_B + (RG_GREEN_B - RG_BLUE_B) * s / 255;
        rg_lut[i] = ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
    }
}

static int rg_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int rg_isqrt(int v)
{
    int d = 0;
    while ((d + 1) * (d + 1) <= v) d++;
    return d;
}

/*
 * Fill one strip of the glow.
 *   bits    : sw*sh pixels, top-down, premultiplied ARGB
 *   sx, sy  : absolute screen position of the strip's top-left pixel
 *   tg      : the pinned window's visible rectangle
 *   T       : glow thickness in pixels
 *   inside  : 1 = draw inside the window edges (maximized / fullscreen)
 *   phase   : 0..255 animation phase (makes the colours flow around)
 *   whiten  : 0..255 extra brightness (flash when a window gets pinned)
 */
static void rg_render(unsigned int *bits, int sx, int sy, int sw, int sh,
                      RRect tg, int T, int inside, unsigned phase, unsigned whiten)
{
    int W = tg.r - tg.l, H = tg.b - tg.t;
    long long P = 2LL * (W + H);
    if (P <= 0) P = 1;
    int core = T / 2; if (core < 1) core = 1;

    for (int j = 0; j < sh; j++) {
        int y = sy + j;
        for (int i = 0; i < sw; i++) {
            int x = sx + i, d, side;

            if (!inside) {
                int dx = x < tg.l ? tg.l - 1 - x : (x >= tg.r ? x - tg.r : -1);
                int dy = y < tg.t ? tg.t - 1 - y : (y >= tg.b ? y - tg.b : -1);
                if (dx >= 0 && dy >= 0) d = rg_isqrt(dx * dx + dy * dy);  /* rounded corners */
                else d = dx > dy ? dx : dy;
                if (d < 0) d = 0;
                if (y < tg.t)       side = 0;
                else if (x >= tg.r) side = 1;
                else if (y >= tg.b) side = 2;
                else                side = 3;
            } else {
                int dt = y - tg.t, dr = tg.r - 1 - x, db = tg.b - 1 - y, dl = x - tg.l;
                d = dt; side = 0;
                if (dr < d) { d = dr; side = 1; }
                if (db < d) { d = db; side = 2; }
                if (dl < d) { d = dl; side = 3; }
                if (d < 0) d = 0;
            }

            /* position along the perimeter, clockwise from the top-left corner */
            int pos;
            switch (side) {
            case 0:  pos = rg_clamp(x - tg.l, 0, W); break;
            case 1:  pos = W + rg_clamp(y - tg.t, 0, H); break;
            case 2:  pos = W + H + rg_clamp(tg.r - 1 - x, 0, W); break;
            default: pos = 2 * W + H + rg_clamp(tg.b - 1 - y, 0, H); break;
            }

            unsigned a;
            if (d < core)   a = 255;
            else if (d >= T) a = 0;
            else            a = 255u * (unsigned)(T - d) / (unsigned)(T - core + 1);

            /* two colour waves travel around the window */
            int wave = (int)(((long long)pos * 512) / P);
            unsigned c = rg_lut[(unsigned)(wave - (int)phase) & 255u];
            unsigned r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            if (whiten) {
                r += (255 - r) * whiten / 255;
                g += (255 - g) * whiten / 255;
                b += (255 - b) * whiten / 255;
            }
            r = (r * a + 127) / 255;
            g = (g * a + 127) / 255;
            b = (b * a + 127) / 255;
            bits[j * sw + i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
}
