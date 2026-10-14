/* SPDX-License-Identifier: GPL-3.0-only */
/* SONG manager.  It follows SAVE > PRESETS: choose with K4, then confirm
 * LOAD or SAVE with a second clockwise turn of K1/K2. */
static uint8_t song_pick = 0xFFu, song_action, song_manager_page;   /* 1 LOAD, 2 SAVE, 3 CLEAR */
#define SONG_DARK RGB(54, 54, 60)
#define SONG_DIM  RGB(118, 118, 126)
static int on_song_manager(void) { return !ui.home && cur_page()->scope == SC_SONGMGR; }
static void song_screen_draw(void);
static void song_screen_input(uint32_t pressed, uint32_t home);

static void song_pick_fix(void)
{
    if (song_pick >= SONG_COUNT) song_pick = (uint8_t)(song_number() - 1u);
}
static void song_manager_draw(void)
{
    static uint32_t previous;
    static const uint16_t KNOB_COL[4] = {RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26)};
    uint32_t i, live = song_number() - 1u;
    uint32_t sig;
    char b[16];
    if (song_manager_page) { song_screen_draw(); return; }
    song_pick_fix();
    sig = song_pick + 7u * live + 31u * song_action + 71u * song.playing + 127u * arrangement_enabled;
    if (!ui.force && !ui.msg_t && sig == previous) return;
    previous = ui.msg_t ? ~sig : sig;
    cv_begin(240, 72, C_BLACK);
    for (i = 0; i < 4u; i++) cv_rect((int32_t)i * 60 + 4, 6, 52, 3, KNOB_COL[i]);
    cv_icon(5, 10, ICON_LOAD, KNOB_COL[0]);
    cv_icon(65, 10, ICON_SAVE, KNOB_COL[1]);
    cv_icon(123, 10, ICON_CLEAR, KNOB_COL[2]);
    cv_icon(185, 10, ICON_ARP, KNOB_COL[3]);
    cv_text(19, 9, &FONT_S, "LOAD", SONG_DIM);
    cv_text(79, 9, &FONT_S, "SAVE", SONG_DIM);
    cv_text(137, 9, &FONT_S, "CLEAR", SONG_DIM);
    cv_text(199, 9, &FONT_S, "SONG", SONG_DIM);
    cv_text(17, 29, &FONT_S, song_action == 1u ? "OK?" : "--", song_action == 1u ? C_WHITE : SONG_DIM);
    cv_text(79, 29, &FONT_S, song_action == 2u ? "OK?" : "--", song_action == 2u ? C_WHITE : SONG_DIM);
    cv_text(139, 29, &FONT_S, song_action == 3u ? "OK?" : "--", song_action == 3u ? C_WHITE : SONG_DIM);
    fmt_int(b, (int32_t)song_pick + 1);
    cv_text(207, 29, &FONT_S, b, C_WHITE);
    cv_blit(0, 0);
    for (i = 0; i < SONG_COUNT; i++) {
        int selected = i == song_pick, is_live = i == live;
        cv_begin(240, 36, C_BLACK);
        if (selected) cv_rect(0, 0, 240, 36, SONG_DARK);
        if (is_live) cv_text(8, 10, &FONT_S, "LIVE", RGB(255, 98, 26));
        if (song_saved(i))
            cv_icon(76, 10, ICON_SAVE, selected ? C_WHITE : SONG_DIM);
        str_cpy(b, "SONG ", sizeof b);
        fmt_int(b + str_len(b), (int32_t)i + 1);
        cv_text_scaled(100, 0, &FONT_S, b, selected ? C_WHITE : C_GRAY, 2, 1);
        if (selected) cv_rect(0, 0, 2, 36, C_WHITE);
        cv_blit(0, 72 + 36u * i);
    }
    cv_begin(240, 60, C_BLACK);
    if (ui.msg_t) {
        cv_rect(0, 8, 240, 30, C_WHITE);
        cv_text((240 - text_w(&FONT_S, ui.msg)) / 2, 15, &FONT_S, ui.msg, C_BLACK);
    } else {
        cv_text(8, 24, &FONT_S, "K1/K2: TURN AGAIN TO CONFIRM", SONG_DIM);
        cv_text(8, 44, &FONT_S, "K4: SELECT SONG", SONG_DIM);
    }
    cv_blit(0, 180);
}

static void song_manager_input(uint32_t pressed, uint32_t home)
{
    uint32_t k, b;
    int32_t steps;
    steps = panel_enc(EN_SELECT);                    /* SELECT: SONG list <-> arrangement editor */
    if (steps) {
        song_manager_page ^= 1u;
        song_action = 0;
        ui.force = 1;
        return;
    }
    if (song_manager_page) { song_screen_input(pressed, home); return; }
    song_pick_fix();
    if (home == 1u) { go_home(); return; }       /* BT_TAP */
    for (k = 0; k < NB; k++) {
        if (!((pressed >> panel.btn[k]) & 1u)) continue;
        b = k;
        if ((b == B_PLAY || b == B_REC) && ft_owns_press()) continue;
        if (b == B_PLAY) {
            if (!song.playing && arrangement_enabled && !arr_valid(&arrangement, arrangement_ready())) ui_message("EMPTY SCENE: REC");
            else transport_req = song.playing ? 2 : 1;
        } else if (b == B_SEQ) {
            open_family(FAM_SEQ);
        } else if (b == B_OCTDN) {
            arrangement_enabled ^= 1u;
            ui_message(arrangement_enabled ? "SONG MODE" : "LOOP MODE");
        } else if (b == B_SAVE) {
            arrangement_save();
        }
    }
    steps = panel_enc(EN_K1);
    if (steps) {
        if (steps > 0 && song_action == 1u) { song_load(song_pick); song_action = 0; }
        else if (steps > 0) { song_action = 1; ui_message("LOAD: TURN AGAIN"); }
        else song_action = 0;
    }
    steps = panel_enc(EN_K2);
    if (steps) {
        if (steps > 0 && song_action == 2u) { song_save_as(song_pick); song_action = 0; }
        else if (steps > 0) { song_action = 2; ui_message("SAVE: TURN AGAIN"); }
        else song_action = 0;
    }
    steps = panel_enc(EN_K3);
    if (steps) {
        if (steps > 0 && song_action == 3u) { song_clear(song_pick); song_action = 0; }
        else if (steps > 0) { song_action = 3; ui_message("CLEAR: TURN AGAIN"); }
        else song_action = 0;
    }
    steps = panel_enc(EN_K4);
    if (steps) {
        song_pick = (uint8_t)clamp((int32_t)song_pick + steps, 0, (int32_t)SONG_COUNT - 1);
        song_action = 0;
    }
    panel_enc(EN_ALGO); panel_enc(EN_PRESET);
}

/* The HOME-hold SONG manager has two PRESET-knob pages: its song list and
 * this arrangement editor. */
static uint8_t song_cursor, song_store_armed, song_load_armed;
static uint32_t song_store_deadline;
static int on_song_page(void) { return on_song_manager() && song_manager_page; }
static void song_sane(void)
{
    uint32_t i, ok = arrangement.count >= 1u && arrangement.count <= ARR_STEPS;
    for (i = 0; ok && i < arrangement.count; i++)
        ok = arrangement.entry[i].scene < ARR_SCENES && arrangement.entry[i].bars >= 1u && arrangement.entry[i].bars <= 64u;
    if (!ok)
        arr_defaults(&arrangement);
    if (song_cursor >= arrangement.count)
        song_cursor = (uint8_t)(arrangement.count - 1u);
}

static void song_screen_draw(void)
{
    uint32_t i, start;
    static uint32_t previous;
    static const uint16_t SC[4] = {RGB(40, 124, 255), RGB(30, 204, 112), RGB(255, 198, 24), RGB(255, 98, 26)};
    uint32_t sig;
    char b[30];
    song_sane();
    start = song_cursor > 3u ? song_cursor - 3u : 0u;
    sig = song_cursor + 17u * arrangement_enabled + 37u * song.playing + 71u * arrangement_clock.index +
          127u * arrangement_clock.bar + 257u * arrangement.count + 509u * song.g[G_BPM];
    for (i = 0; i < arrangement.count; i++)
        sig = sig * 31u + arrangement.entry[i].scene + 7u * arrangement.entry[i].bars;
    for (i = 0; i < ARR_SCENES; i++) sig = sig * 3u + (uint32_t)project_used(i);
    if (!ui.force && !ui.msg_t && sig == previous) return;
    previous = ui.msg_t ? ~sig : sig;
    cv_begin(240, 40, C_BLACK);
    cv_text(4, 4, &FONT_S, "SONG EDIT", C_WHITE);
    cv_text(76, 20, &FONT_S, arrangement_enabled ? "song mode" : "loop mode",
            arrangement_enabled ? SC[3] : RGB(118, 118, 126));
    fmt_int(b, song.g[G_BPM]);
    cv_text(236 - text_w(&FONT_S, b) - 28, 4, &FONT_S, b, C_WHITE);
    cv_text(212, 4, &FONT_S, "bpm", RGB(118, 118, 126));
    cv_text(236 - text_w(&FONT_S, song.playing ? "playing" : "stopped"), 20, &FONT_S,
            song.playing ? "playing" : "stopped", song.playing ? SC[1] : RGB(118, 118, 126));
    cv_rect(0, 39, 240, 1, RGB(26, 26, 30));
    cv_blit(0, 0);
    for (i = 0; i < 5u; i++) {
        uint32_t pos = start + i;
        cv_begin(240, 26, C_BLACK);
        if (pos < arrangement.count) {
            const arr_entry_t *e = &arrangement.entry[pos];
            int selected = pos == song_cursor, used = project_used(e->scene);
            uint16_t sc = SC[e->scene & 3u];
            int32_t w;
            fmt_int(b, (int32_t)pos + 1);
            cv_text(4, 5, &FONT_S, b, selected ? C_WHITE : RGB(118, 118, 126));
            cv_rect(30, 3, 22, 20, used ? sc : RGB(26, 26, 30));
            b[0] = (char)('A' + e->scene); b[1] = 0;
            cv_text(37, 5, &FONT_S, b, used ? C_BLACK : sc);
            w = e->bars * 120 / 64 + 4;
            cv_rect(60, 9, w, 8, used ? (selected ? sc : RGB(54, 54, 60)) : RGB(26, 26, 30));
            fmt_int(b, e->bars);
            str_cpy(b + str_len(b), e->bars == 1 ? " bar" : " bars", 8);
            cv_text(60 + w + 6, 5, &FONT_S, used ? b : "empty", selected ? C_WHITE : RGB(118, 118, 126));
            if (arrangement_clock.running && arrangement_clock.index == pos) {
                cv_rect(60, 19, (int32_t)(arrangement_clock.bar + 1u) * w / (e->bars ? e->bars : 1), 2, C_WHITE);
                cv_rect(226, 9, 9, 9, SC[1]);
            }
            if (selected) cv_rect(0, 1, 240, 1, RGB(54, 54, 60)), cv_rect(0, 24, 240, 1, RGB(54, 54, 60));
        }
        cv_blit(0, 42 + 26u * i);
    }
    cv_begin(240, 68, C_BLACK);
    if (ui.msg_t) {
        cv_rect(0, 4, 240, 30, C_WHITE);
        cv_text((240 - text_w(&FONT_S, ui.msg)) / 2, 11, &FONT_S, ui.msg, C_BLACK);
    } else {
        static const char *const L[4] = {"entry", "scene", "bars", "length"};
        for (i = 0; i < 4u; i++) {
            cv_rect((int32_t)i * 60 + 4, 6, 52, 3, SC[i]);
            cv_text((int32_t)i * 60 + 30 - text_w(&FONT_S, L[i]) / 2, 12, &FONT_S, L[i], RGB(118, 118, 126));
        }
    }
    cv_text(4, 34, &FONT_S, "rec: store   save: chain", RGB(196, 196, 204));
    cv_text(4, 50, &FONT_S, "oct-: loop/song  oct++: load", RGB(118, 118, 126));
    cv_blit(0, 172);
}

static void song_screen_input(uint32_t pressed, uint32_t home)
{
    uint32_t k, b;
    int32_t steps;
    song_sane();
    if (home == 1u) { go_home(); return; }       /* BT_TAP */
    for (k = 0; k < NB; k++) {
        if (!((pressed >> panel.btn[k]) & 1u)) continue;
        b = k;
        if ((b == B_PLAY || b == B_REC) && ft_owns_press()) continue;
        if (b == B_PLAY) {
            if (!song.playing && arrangement_enabled && !arr_valid(&arrangement, arrangement_ready())) ui_message("EMPTY SCENE: REC");
            else transport_req = song.playing ? 2 : 1;
        } else if (b == B_SEQ) {
            open_family(FAM_SEQ);
        } else if (b == B_SAVE || b == B_REC || b == B_OCTDN || b == B_OCTUP) {
            uint32_t scene = arrangement.entry[song_cursor].scene;
            if (song.playing || transport_req) { ui_message("STOP FIRST"); continue; }
            if (b == B_SAVE) {
                arrangement_save(); song_store_armed = 0;
            } else if (b == B_OCTDN) {
                arrangement_enabled ^= 1u;
                ui_message(arrangement_enabled ? "SONG MODE" : "LOOP MODE");
            } else if (b == B_OCTUP) {
                if (song_load_armed && (int32_t)(fm1_ms - song_store_deadline) <= 0) {
                    song_load_armed = 0; project_load(scene);
                } else {
                    song_load_armed = 1; song_store_armed = 0; song_store_deadline = fm1_ms + 3000u;
                    ui_message("OCT+ AGAIN: LOAD");
                }
            } else if (project_used(scene) && (!song_store_armed || (int32_t)(fm1_ms - song_store_deadline) > 0)) {
                song_store_armed = 1; song_store_deadline = fm1_ms + 3000u; ui_message("REC AGAIN: REPLACE");
            } else {
                project_save(scene); settings_save(); song_store_armed = 0;
            }
        }
    }
    for (k = 0; k < 4u; k++) {
        steps = panel_enc(EN_K1 + k);
        if (!steps) continue;
        song_store_armed = 0; song_load_armed = 0;
        if (k == 0) { song_cursor = (uint8_t)clamp(song_cursor + steps, 0, arrangement.count - 1); continue; }
        if (song.playing || transport_req) { ui_message("STOP FIRST"); continue; }
        if (k == 1) arrangement.entry[song_cursor].scene = (uint8_t)clamp(arrangement.entry[song_cursor].scene + steps, 0, 3);
        if (k == 2) arrangement.entry[song_cursor].bars = (uint8_t)clamp(arrangement.entry[song_cursor].bars + steps, 1, 64);
        if (k == 3) {
            arrangement.count = (uint8_t)clamp(arrangement.count + steps, 1, ARR_STEPS);
            if (song_cursor >= arrangement.count) song_cursor = arrangement.count - 1;
        }
    }
    panel_enc(EN_ALGO); panel_enc(EN_PRESET);
}
