/* SPDX-License-Identifier: GPL-3.0-only */
/* Browser host for SLOOP's real panel UI and DSP.  It deliberately reuses the
 * UI harness: the pixels on the simulated LCD are drawn by firmware code. */
#define UI_PAGES_HARNESS_ONLY 1
#include "../../tests/ui_pages_test.c"
#undef __attribute__
#define EXPORT __attribute__((used, visibility("default")))

static float out_l[1024], out_r[1024];
static uint32_t samples, last_frame, draws;

EXPORT void web_boot(void)
{
    uint32_t i;
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) {
        set_engine_of(&trk[i], TRK_DEF[i][0]);
        apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].engine = trk[i].eng_req;
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    go_home();
    ui.force = 1;
    ui_leds();
    ui_draw();
    draws++;
}

EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
EXPORT uint16_t *web_screen(void) { return screen; }
EXPORT uint32_t web_screen_draws(void) { return draws; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
EXPORT uint32_t web_playing(void) { return song.playing; }
EXPORT void web_master(uint32_t value) { song.master_q12 = (value > 1023u ? 1023u : value) * 4u; }

EXPORT void web_render(uint32_t n)
{
    int32_t block[CTL * 2];
    uint32_t pos, k;
    if (n > 1024) n = 1024;
    for (pos = 0; pos < n; pos += CTL) {
        uint32_t count = n - pos < CTL ? n - pos : CTL;
        fm1_ms = (uint32_t)((uint64_t)samples * 1000 / FS);
        ui_input();
        mix_block(block, count);
        for (k = 0; k < count; k++) {
            out_l[pos + k] = block[k * 2] / 65536.0f;
            out_r[pos + k] = block[k * 2 + 1] / 65536.0f;
        }
        samples += count;
        if (fm1_ms - last_frame >= 16) {
            last_frame = fm1_ms;
            ui_leds();
            ui_draw();
            draws++;
        }
    }
}

/* The browser sends logical button labels; the firmware panel maps them to
 * the physical matrix exactly as the hardware does. */
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t matrix = 0, b;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            matrix |= 1u << panel.btn[b];
    edges_btn |= matrix & ~fm1_in.buttons;
    fm1_in.buttons = matrix;
}
EXPORT void web_keys(uint32_t mask) { fm1_in.notes = mask & ((1u << 27) - 1u); }
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        encs[panel.enc[role] % 7] += n * panel.dir[role];
}

EXPORT uint32_t web_lit_buttons(void)
{
    uint32_t labels = 0, b;
    for (b = 0; b < NB; b++)
        if (fm1_led[panel.btn[b]]) labels |= 1u << b;
    return labels;
}
EXPORT uint32_t web_lit_keys(void) { return fm1_in.notes; }
