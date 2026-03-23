/*
 * ui_panel.c — panel management
 *
 * Owns all panel UI global state, the button registry, asset bundle state
 * machine, panel wiring, and tab/list population logic.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "il2cpp.h"
#include "utils.h"
#include "ui.h"
#include "modlist.h"
#include "modconfig.h"

/* ======================================================================
 * BUTTON REGISTRY
 * ====================================================================== */

BtnEntry g_btn_registry[MAX_REG_BUTTONS];
int      g_btn_reg_count = 0;

void register_button(void *go, BtnHandler handler, int user_index)
{
    if (g_btn_reg_count >= MAX_REG_BUTTONS) { LOGE("button registry full"); return; }
    g_btn_registry[g_btn_reg_count].go         = go;
    g_btn_registry[g_btn_reg_count].handler    = handler;
    g_btn_registry[g_btn_reg_count].user_index = user_index;
    g_btn_reg_count++;
}

/* ======================================================================
 * ASSET BUNDLE STATE
 * ====================================================================== */

BundleState  g_bundle_state    = BS_IDLE;
void        *g_bundle_load_req = NULL;
void        *g_bundle          = NULL;
void        *g_asset_load_req  = NULL;

/* ======================================================================
 * GLOBAL UI STATE
 * ====================================================================== */

void *g_panel_go          = NULL;
void *g_manage_panel_go   = NULL;
void *g_config_panel_go   = NULL;
void *g_status_text_comp  = NULL;
void *g_sel_mod_label     = NULL;
void *g_sel_cfg_label     = NULL;
void *g_desc_label_comp   = NULL;
int   g_panel_open        = 0;
int   g_active_tab        = 0;
int   g_panel_ready       = 0;

void *g_bool_widget_go    = NULL;
void *g_number_widget_go  = NULL;
void *g_string_widget_go  = NULL;
void *g_bool_val_tmp      = NULL;
void *g_num_val_tmp       = NULL;
void *g_num_step_tmp      = NULL;
void *g_str_val_tmp       = NULL;
double g_num_step         = 1.0;
int    g_str_shifted      = 1;    /* keyboard starts in uppercase */

/* TMP refs for A–Z keyboard buttons — updated on shift toggle */
static void *s_letter_tmps[26];

void *g_manage_content_tr  = NULL;
void *g_cfg_mod_content_tr = NULL;
void *g_cfg_ent_content_tr = NULL;
void *g_manage_template_go  = NULL;
void *g_cfg_mod_template_go = NULL;
void *g_cfg_ent_template_go = NULL;

void *g_manage_btns[MAX_LIST_BUTTONS];
int   g_manage_btn_count = 0;
void *g_cfg_mod_btns[MAX_LIST_BUTTONS];
int   g_cfg_mod_btn_count = 0;
void *g_cfg_ent_btns[MAX_LIST_BUTTONS];
int   g_cfg_ent_btn_count = 0;

ModEntry  g_mod_entries[MAX_MODS];
int       g_mod_count       = 0;
ModConfig g_active_config;
int       g_config_loaded   = 0;
int       g_selected_entry  = -1;

/* ======================================================================
 * FORWARD DECLARATIONS
 * ====================================================================== */

static void show_tab(int tab);
static void show_status(const char *msg);
static void populate_manage_list(void);
static void populate_cfg_mod_list(void);
static void wire_mod_panel(void *panel_go);
static void hide_all_widgets(void);
static void refresh_bool_widget(void);
static void refresh_number_widget(void);
static void refresh_str_widget(void);
static void clear_list(void **btns, int *count);
static void *spawn_list_btn(void *content_tr, void *template_go,
                            const char *label, BtnHandler handler,
                            int idx, float anchor_y);

/* ======================================================================
 * INTERACTION WIDGET HELPERS
 * ====================================================================== */

static void hide_all_widgets(void)
{
    if (g_bool_widget_go)   fn_go_set_active(g_bool_widget_go,   0);
    if (g_number_widget_go) fn_go_set_active(g_number_widget_go, 0);
    if (g_string_widget_go) fn_go_set_active(g_string_widget_go, 0);
}

static void refresh_bool_widget(void)
{
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    if (g_bool_val_tmp)
        tmp_set(g_bool_val_tmp, e->value_num ? "Enabled" : "Disabled");
}

static void refresh_number_widget(void)
{
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    if (g_num_val_tmp) {
        char buf[64];
        if (strcmp(e->type, MODCFG_TYPE_INT) == 0)
            snprintf(buf, sizeof(buf), "Current Value: %d", (int)e->value_num);
        else
            snprintf(buf, sizeof(buf), "Current Value: %.3f", (float)e->value_num);
        tmp_set(g_num_val_tmp, buf);
    }
    if (g_num_step_tmp) {
        char buf[32];
        if (g_num_step >= 1.0)
            snprintf(buf, sizeof(buf), "Step Precision: %d", (int)g_num_step);
        else
            snprintf(buf, sizeof(buf), "Step Precision: %.4f", (float)g_num_step);
        tmp_set(g_num_step_tmp, buf);
    }
}

static void refresh_str_widget(void)
{
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    if (g_str_val_tmp) tmp_set(g_str_val_tmp, e->value_str);
}

/* ======================================================================
 * BUTTON HANDLERS
 * ====================================================================== */

void on_mods_button(int idx)          { (void)idx; LOGI("handler: Mods"); open_mod_manager(); }
static void on_back_button(int idx)   { (void)idx; LOGI("handler: Back"); close_mod_manager(); }
static void on_tab_manage(int idx)    { (void)idx; show_tab(0); }
static void on_tab_configure(int idx) { (void)idx; show_tab(1); }
static void on_restart_game(int idx)  { (void)idx; LOGI("handler: Quit"); exit(0); }

/* -- Bool widget toggle -- */
static void on_cfg_bool_toggle(int idx)
{
    (void)idx;
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    set_entry_value(e, e->value_num ? 0.0 : 1.0, NULL);
    save_config(&g_active_config);
    refresh_bool_widget();
    populate_cfg_entry_list();
}

/* -- Number widget value +/- -- */
static void on_num_val_dec(int idx)
{
    (void)idx;
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    double step = (strcmp(e->type, MODCFG_TYPE_INT) == 0 && g_num_step < 1.0) ? 1.0 : g_num_step;
    set_entry_value(e, e->value_num - step, NULL);
    save_config(&g_active_config);
    refresh_number_widget();
    populate_cfg_entry_list();
}

static void on_num_val_inc(int idx)
{
    (void)idx;
    if (!g_config_loaded || g_selected_entry < 0) return;
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    double step = (strcmp(e->type, MODCFG_TYPE_INT) == 0 && g_num_step < 1.0) ? 1.0 : g_num_step;
    set_entry_value(e, e->value_num + step, NULL);
    save_config(&g_active_config);
    refresh_number_widget();
    populate_cfg_entry_list();
}

/* -- Number widget step precision +/- -- */
static void on_num_step_dec(int idx)
{
    (void)idx;
    g_num_step /= 10.0;
    if (g_num_step < 0.0001) g_num_step = 0.0001;
    refresh_number_widget();
}

static void on_num_step_inc(int idx)
{
    (void)idx;
    g_num_step *= 10.0;
    if (g_num_step > 1000000.0) g_num_step = 1000000.0;
    refresh_number_widget();
}

/* -- String widget keyboard -- */
/* idx 0-25 = A-Z, 26-35 = 0-9, 36 = '-', 37 = '_', 38 = '+' */
static const char s_kbd_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_+";

static void on_str_append(int idx)
{
    if (!g_str_val_tmp || idx < 0 || idx >= (int)(sizeof(s_kbd_chars) - 1)) return;
    char cur[MAX_CFG_STR_LEN];
    tmp_get(g_str_val_tmp, cur, sizeof(cur));
    int len = 0; while (cur[len]) len++;
    if (len >= MAX_CFG_STR_LEN - 2) return;
    char c = s_kbd_chars[idx];
    if (idx < 26 && !g_str_shifted) c |= 0x20;  /* lowercase */
    cur[len] = c; cur[len + 1] = '\0';
    tmp_set(g_str_val_tmp, cur);
}

static void on_str_backspace(int idx)
{
    (void)idx;
    if (!g_str_val_tmp) return;
    char cur[MAX_CFG_STR_LEN];
    tmp_get(g_str_val_tmp, cur, sizeof(cur));
    int len = 0; while (cur[len]) len++;
    if (len > 0) { cur[len - 1] = '\0'; tmp_set(g_str_val_tmp, cur); }
}

static void on_str_shift(int idx)
{
    (void)idx;
    g_str_shifted = !g_str_shifted;
    for (int k = 0; k < 26; k++) {
        if (!s_letter_tmps[k]) continue;
        char buf[4];
        buf[0] = g_str_shifted ? ('A' + k) : ('a' + k);
        buf[1] = '\0';
        tmp_set(s_letter_tmps[k], buf);
    }
}

static void on_str_submit(int idx)
{
    (void)idx;
    if (!g_str_val_tmp || !g_config_loaded || g_selected_entry < 0) return;
    char cur[MAX_CFG_STR_LEN];
    tmp_get(g_str_val_tmp, cur, sizeof(cur));
    ModCfgEntry *e = &g_active_config.entries[g_selected_entry];
    set_entry_value(e, 0.0, cur);
    save_config(&g_active_config);

    /* Migrate the disabled-mods folder immediately when the path config changes */
    if (strcmp(e->key, "DisabledModsFolder") == 0)
        set_disabled_folder(cur);

    populate_cfg_entry_list();
}

static void on_manage_toggle(int idx)
{
    if (idx < 0 || idx >= g_mod_count) return;
    ModEntry *e = &g_mod_entries[idx];
    int r = e->enabled ? disable_mod(e->name) : enable_mod(e->name);
    if (r == -2) { show_status("Can't disable self — restart for other changes"); return; }
    if (r != 0)  { show_status("File move failed"); return; }
    e->enabled = !e->enabled;
    populate_manage_list();
    show_status("Restart required for changes to take effect");
}

static void on_cfg_mod_select(int idx)
{
    g_config_loaded  = 0;
    g_selected_entry = -1;
    if (g_sel_cfg_label)   tmp_set(g_sel_cfg_label,   "");
    if (g_desc_label_comp) tmp_set(g_desc_label_comp, "");
    hide_all_widgets();
    clear_list(g_cfg_ent_btns, &g_cfg_ent_btn_count);

    if (idx < 0 || idx >= g_mod_count) return;
    if (g_sel_mod_label) tmp_set(g_sel_mod_label, g_mod_entries[idx].name);

    if (!config_exists(g_mod_entries[idx].name)) {
        LOGI("cfg_mod_select: '%s' has no config", g_mod_entries[idx].name);
        return;  /* entry list left empty; mod button was greyed out already */
    }

    if (load_config(g_mod_entries[idx].name, &g_active_config) == 0) {
        g_config_loaded = 1;
        if (g_sel_mod_label) tmp_set(g_sel_mod_label, g_active_config.mod_name);
        populate_cfg_entry_list();
        if (g_active_config.count > 0)
            on_cfg_entry_select(0);
    }
}

void on_cfg_entry_select(int idx)
{
    g_selected_entry = idx;
    if (!g_config_loaded || idx < 0 || idx >= g_active_config.count) return;
    ModCfgEntry *e = &g_active_config.entries[idx];

    if (g_sel_cfg_label)   tmp_set(g_sel_cfg_label, e->key);
    if (g_desc_label_comp)
        tmp_set(g_desc_label_comp, e->description[0] ? e->description : e->type);

    hide_all_widgets();
    if (strcmp(e->type, MODCFG_TYPE_BOOL) == 0) {
        if (g_bool_widget_go) fn_go_set_active(g_bool_widget_go, 1);
        refresh_bool_widget();
    } else if (strcmp(e->type, MODCFG_TYPE_FLOAT) == 0
            || strcmp(e->type, MODCFG_TYPE_INT)   == 0) {
        if (g_number_widget_go) {
            fn_go_set_active(g_number_widget_go, 1);
            g_num_step = 1.0;
        }
        refresh_number_widget();
    } else if (strcmp(e->type, MODCFG_TYPE_STRING) == 0) {
        if (g_string_widget_go) fn_go_set_active(g_string_widget_go, 1);
        refresh_str_widget();
    }
}

/* ======================================================================
 * UI STATE MANAGEMENT
 * ====================================================================== */

void open_mod_manager(void)
{
    if (!g_panel_go)    { LOGE("open: panel not ready"); return; }
    if (!g_panel_ready) { show_status("Bundle still loading..."); return; }
    fn_go_set_active(g_panel_go, 1);
    if (g_interact_menu_tr) fn_go_set_active(fn_comp_get_go(g_interact_menu_tr), 0);
    g_panel_open = 1;

    /* Re-dirty all panel Graphic components now that the Canvas is live.
     * SetAllDirty calls during fix_panel_shaders (while inactive) may be
     * dropped by CanvasUpdateRegistry.  Repeat here while Canvas is active. */
    if (fn_get_comps_ch_arr && g_graphic_type_obj && fn_graphic_set_dirty) {
        void *panel_tr = fn_go_get_tr(g_panel_go);
        void *arr = fn_get_comps_ch_arr(panel_tr, g_graphic_type_obj, 1);
        int n = arr_len(arr);
        for (int i = 0; i < n; i++) {
            void *comp = arr_get(arr, i);
            if (comp && (uintptr_t)comp > 0x10000u) fn_graphic_set_dirty(comp);
        }
        LOGI("open: re-dirtied %d graphics on panel activation", n);
    }

    show_tab(1);  /* show_tab calls populate_cfg_mod_list internally */

    /* Auto-select the mod manager's own config entry */
    for (int i = 0; i < g_mod_count; i++) {
        if (strcmp(g_mod_entries[i].name, SELF_MOD_NAME) == 0) {
            on_cfg_mod_select(i);
            break;
        }
    }
}

void close_mod_manager(void)
{
    if (g_panel_go)         fn_go_set_active(g_panel_go, 0);
    if (g_interact_menu_tr) fn_go_set_active(fn_comp_get_go(g_interact_menu_tr), 1);
    g_panel_open = 0;
}

void reset_ui_state(void)
{
    /* Scene-specific pointers — all destroyed with the old scene */
    g_inject_done      = 0;
    g_canvas_tr        = NULL;
    g_interact_menu_tr = NULL;
    g_mods_button_go   = NULL;
    g_menu_front_mat   = NULL;

    /* Button registry — all old GOs are gone */
    g_btn_reg_count = 0;

    /* Panel state — panel GO was destroyed with the scene */
    g_panel_go        = NULL;
    g_panel_ready     = 0;
    g_panel_open      = 0;
    g_active_tab      = 0;
    g_manage_panel_go = NULL;
    g_config_panel_go = NULL;
    g_status_text_comp = NULL;
    g_sel_mod_label   = NULL;
    g_sel_cfg_label   = NULL;
    g_desc_label_comp = NULL;
    g_bool_widget_go  = NULL;
    g_number_widget_go = NULL;
    g_string_widget_go = NULL;
    g_bool_val_tmp    = NULL;
    g_num_val_tmp     = NULL;
    g_num_step_tmp    = NULL;
    g_str_val_tmp     = NULL;
    g_num_step        = 1.0;
    g_str_shifted     = 1;
    for (int k = 0; k < 26; k++) s_letter_tmps[k] = NULL;

    /* Content/template pointers — destroyed with panel */
    g_manage_content_tr  = NULL;
    g_cfg_mod_content_tr = NULL;
    g_cfg_ent_content_tr = NULL;
    g_manage_template_go  = NULL;
    g_cfg_mod_template_go = NULL;
    g_cfg_ent_template_go = NULL;
    g_manage_btn_count    = 0;
    g_cfg_mod_btn_count   = 0;
    g_cfg_ent_btn_count   = 0;

    /* Config state */
    g_config_loaded  = 0;
    g_selected_entry = -1;

    /* Reset bundle state so update_bundle_state re-instantiates the panel.
     * If the bundle is already loaded (normal case), skip the async file read
     * and jump straight to the asset-load step. */
    if (g_bundle_state != BS_FAILED)
        g_bundle_state = g_bundle ? BS_RELOAD : BS_IDLE;

    LOGI("reset_ui_state: done (bundle=%p state=%d)", g_bundle, (int)g_bundle_state);
}

static void show_tab(int tab)
{
    g_active_tab = tab;
    if (g_manage_panel_go) fn_go_set_active(g_manage_panel_go, tab == 0);
    if (g_config_panel_go) fn_go_set_active(g_config_panel_go, tab == 1);
    if (tab == 0) populate_manage_list();
    if (tab == 1) populate_cfg_mod_list();
}

static void show_status(const char *msg)
{
    if (g_status_text_comp) tmp_set(g_status_text_comp, msg);
    LOGI("STATUS: %s", msg);
}

/* ======================================================================
 * LIST POPULATION
 * ====================================================================== */

static int cmp_mod_entries(const void *a, const void *b)
{
    return strcmp(((const ModEntry *)a)->name, ((const ModEntry *)b)->name);
}

/* Add a "StateOverlay" child GO with a full-stretch Image component to color a
 * button row. Mirrors PCVR AddStateOverlay exactly. Because the Image is NOT
 * the Button's targetGraphic, Unity's DoStateTransition tween never touches it,
 * so the color persists. Direct m_Color write is safe here (no tween). */
static void add_state_overlay(void *row_go, float r, float g, float b, float a)
{
    if (!row_go) return;

    void *ov_go = create_go_named("StateOverlay");
    if (!ov_go) { LOGE("add_state_overlay: create_go_named failed for go=%p", row_go); return; }

    /* ov.transform.SetParent(row.transform, false) */
    void *row_tr = fn_go_get_tr(row_go);
    void *ov_tr  = fn_go_get_tr(ov_go);
    fn_tr_set_parent(ov_tr, row_tr, 0);

    /* RectTransform rt = ov.AddComponent<RectTransform>(); full-stretch */
    if (fn_go_add_comp && g_recttransform_type_obj) {
        void *rt = fn_go_add_comp(ov_go, g_recttransform_type_obj);
        if (rt) {
            if (fn_rect_set_anchor_min) fn_rect_set_anchor_min(rt, 0.0f, 0.0f);
            if (fn_rect_set_anchor_max) fn_rect_set_anchor_max(rt, 1.0f, 1.0f);
            if (fn_rect_set_offset_min) fn_rect_set_offset_min(rt, 0.0f, 0.0f);
            if (fn_rect_set_offset_max) fn_rect_set_offset_max(rt, 0.0f, 0.0f);
        } else { LOGI("add_state_overlay: AddComponent<RectTransform> returned NULL"); }
    }

    /* ov.AddComponent<Image>().color = color — write m_Color directly at 0x10 */
    if (fn_go_add_comp && g_image_type_obj) {
        void *img = fn_go_add_comp(ov_go, g_image_type_obj);
        if (img) {
            float *col = (float *)((uint8_t *)img + 0x10);
            col[0] = r; col[1] = g; col[2] = b; col[3] = a;
            if (fn_graphic_set_dirty) fn_graphic_set_dirty(img);
        } else { LOGI("add_state_overlay: AddComponent<Image> returned NULL"); }
    }

    /* ov.SetActive(color.a > 0f) */
    fn_go_set_active(ov_go, a > 0.0f ? 1 : 0);
    LOGI("add_state_overlay: row=%p overlay=%p (%.2f,%.2f,%.2f,%.2f)", row_go, ov_go, r, g, b, a);
}

/* Load a bool entry from our own config, returning def if not found */
static int self_cfg_bool(const char *key, int def)
{
    ModConfig cfg;
    if (load_config(SELF_MOD_NAME, &cfg) != 0) return def;
    ModCfgEntry *e = get_entry(&cfg, key);
    return e ? (int)e->value_num : def;
}

static void clear_list(void **btns, int *count)
{
    for (int i = 0; i < *count; i++)
        if (btns[i]) fn_go_set_active(btns[i], 0);
    *count = 0;
}

static void *spawn_list_btn(void *content_tr, void *template_go,
                            const char *label, BtnHandler handler, int idx,
                            float anchor_y)
{
    if (!content_tr || !template_go) return NULL;
    void *new_go = fn_obj_instantiate(template_go);
    if (!new_go) return NULL;
    void *tr = fn_go_get_tr(new_go);
    fn_tr_set_parent(tr, content_tr, 0);
    fn_rect_set_apos(tr, 0.0f, anchor_y);
    fn_go_set_active(new_go, 1);
    void *tmp = get_tmp_in_go(new_go);
    if (tmp) tmp_set(tmp, label);
    /* Find Button component — may be on root or a child */
    void *btn_comp = fn_comp_get_comp(tr, g_button_type_obj);
    if (!btn_comp && fn_get_comp_ch)
        btn_comp = fn_get_comp_ch(tr, g_button_type_obj, 1);
    void *btn_go = btn_comp ? fn_comp_get_go(btn_comp) : new_go;
    if (btn_go != new_go)
        LOGI("spawn_list_btn: Button on child GO (root=%p btn_go=%p) label='%s'",
             new_go, btn_go, label);
    /* Interactable only when there's a handler — Unity greys out non-interactable buttons */
    if (btn_comp && fn_set_interactable) fn_set_interactable(btn_comp, handler ? 1 : 0);
    if (handler) register_button(btn_go, handler, idx);
    return new_go;  /* return root GO for show/hide tracking */
}

static void populate_manage_list(void)
{
    clear_list(g_manage_btns, &g_manage_btn_count);
    if (!g_manage_content_tr || !g_manage_template_go) return;

    g_mod_count = scan_mods(g_mod_entries, MAX_MODS);
    qsort(g_mod_entries, g_mod_count, sizeof(ModEntry), cmp_mod_entries);
    LOGI("populate_manage_list: %d mods", g_mod_count);

    float y = 0.0f;
    int shown = 0;
    for (int i = 0; i < g_mod_count && g_manage_btn_count < MAX_LIST_BUTTONS; i++) {
        int is_self = g_mod_entries[i].builtin;

        const char *label = g_mod_entries[i].display_name[0] ? g_mod_entries[i].display_name : g_mod_entries[i].name;

        /* Self: show greyed-out, non-interactable. Others: color by enabled state. */
        BtnHandler h = is_self ? NULL : on_manage_toggle;
        void *btn = spawn_list_btn(g_manage_content_tr, g_manage_template_go,
                                   label, h, i, y);
        if (btn) {
            if (is_self)
                add_state_overlay(btn, 0.3f, 0.3f, 0.3f, 0.6f);
            else if (g_mod_entries[i].enabled)
                add_state_overlay(btn, 0.0f, 0.6f, 0.0f, 0.5f);
            else
                add_state_overlay(btn, 0.6f, 0.0f, 0.0f, 0.5f);
        }
        g_manage_btns[g_manage_btn_count++] = btn;
        y -= 50.0f;
        shown++;
    }
    if (shown == 0)
        spawn_list_btn(g_manage_content_tr, g_manage_template_go,
                       "No other mods found", NULL, -1, 0.0f);
}

static void populate_cfg_mod_list(void)
{
    clear_list(g_cfg_mod_btns, &g_cfg_mod_btn_count);
    if (!g_cfg_mod_content_tr || !g_cfg_mod_template_go) return;

    g_mod_count = scan_mods(g_mod_entries, MAX_MODS);
    qsort(g_mod_entries, g_mod_count, sizeof(ModEntry), cmp_mod_entries);
    LOGI("populate_cfg_mod_list: %d mods", g_mod_count);

    int show_all = self_cfg_bool("ShowAllMods", 0);

    float y = 0.0f;
    int shown = 0;
    for (int i = 0; i < g_mod_count && g_cfg_mod_btn_count < MAX_LIST_BUTTONS; i++) {
        int has_cfg = config_exists(g_mod_entries[i].name);
        if (!has_cfg && !show_all) continue;  /* hide no-config mods unless ShowAllMods */

        const char *label = g_mod_entries[i].display_name[0] ? g_mod_entries[i].display_name : g_mod_entries[i].name;
        BtnHandler h = has_cfg ? on_cfg_mod_select : NULL;
        void *btn = spawn_list_btn(g_cfg_mod_content_tr, g_cfg_mod_template_go,
                                   label, h, i, y);
        /* Dark overlay for mods with no config (only shown when ShowAllMods = true) */
        if (btn && !has_cfg)
            add_state_overlay(btn, 0.0f, 0.0f, 0.0f, 0.5f);
        g_cfg_mod_btns[g_cfg_mod_btn_count++] = btn;
        y -= 50.0f;
        shown++;
    }
    if (shown == 0)
        spawn_list_btn(g_cfg_mod_content_tr, g_cfg_mod_template_go,
                       "No mods found", NULL, -1, 0.0f);
}

void populate_cfg_entry_list(void)
{
    clear_list(g_cfg_ent_btns, &g_cfg_ent_btn_count);
    if (!g_cfg_ent_content_tr || !g_cfg_ent_template_go || !g_config_loaded) return;

    float y = 0.0f;
    for (int i = 0; i < g_active_config.count && g_cfg_ent_btn_count < MAX_LIST_BUTTONS; i++) {
        ModCfgEntry *e = &g_active_config.entries[i];
        char label[192];
        snprintf(label, sizeof(label), "%s", e->key);
        void *btn = spawn_list_btn(g_cfg_ent_content_tr, g_cfg_ent_template_go,
                                   label, on_cfg_entry_select, i, y);
        g_cfg_ent_btns[g_cfg_ent_btn_count++] = btn;
        y -= 50.0f;
    }
}

/* ======================================================================
 * wire_mod_panel — wire bundle prefab children to handlers/globals
 * ====================================================================== */

static void wire_mod_panel(void *panel_go)
{
    LOGI("wire_mod_panel: panel_go=%p", panel_go);
    void *pt = fn_go_get_tr(panel_go);

    struct { const char *path; BtnHandler h; int idx; } nav_btns[] = {
        { "Button_Back",          on_back_button,   0 },
        { "Button_ManageMods",    on_tab_manage,    0 },
        { "Button_ConfigureMods", on_tab_configure, 0 },
        { "Button_RestartGame",   on_restart_game,  0 },
    };
    for (int i = 0; i < 4; i++) {
        void *tr = tr_find(pt, nav_btns[i].path);
        if (!tr) { LOGI("wire: '%s' not found", nav_btns[i].path); continue; }
        register_button(fn_comp_get_go(tr), nav_btns[i].h, nav_btns[i].idx);
        LOGI("wire: '%s' registered", nav_btns[i].path);
    }

    /* ManageMods panel */
    void *manage_tr = tr_find(pt, "ManageMods");
    if (manage_tr) {
        g_manage_panel_go = fn_comp_get_go(manage_tr);
        void *content_tr = tr_find(manage_tr, "Mods Scroll View/Viewport/Content");
        if (content_tr) {
            g_manage_content_tr = content_tr;
            void *tpl = tr_find(content_tr, "Button_Template");
            if (tpl) {
                void *tpl_go = fn_comp_get_go(tpl);
                g_manage_template_go = fn_obj_instantiate(tpl_go);
                if (g_manage_template_go) fn_go_set_active(g_manage_template_go, 0);
                fn_go_set_active(tpl_go, 0);  /* hide original so content starts clean */
            }
            LOGI("wire: manage content=%p template=%p", content_tr, g_manage_template_go);
        } else { LOGE("wire: ManageMods content not found"); }
        fn_go_set_active(g_manage_panel_go, 1);
    } else { LOGE("wire: ManageMods not found"); }

    /* ConfigureMods panel */
    void *config_tr = tr_find(pt, "ConfigureMods");
    if (config_tr) {
        g_config_panel_go = fn_comp_get_go(config_tr);

        void *mod_content = tr_find(config_tr, "Mods Scroll View/Viewport/Content");
        if (mod_content) {
            g_cfg_mod_content_tr = mod_content;
            void *tpl = tr_find(mod_content, "Button_Template");
            if (tpl) {
                void *tpl_go = fn_comp_get_go(tpl);
                g_cfg_mod_template_go = fn_obj_instantiate(tpl_go);
                if (g_cfg_mod_template_go) fn_go_set_active(g_cfg_mod_template_go, 0);
                fn_go_set_active(tpl_go, 0);
            }
        } else { LOGI("wire: cfg mod scroll content not found"); }

        void *ent_content = tr_find(config_tr, "ConfigScrollView/Viewport/Content");
        if (ent_content) {
            g_cfg_ent_content_tr = ent_content;
            void *tpl = tr_find(ent_content, "Button_Template");
            if (tpl) {
                void *tpl_go = fn_comp_get_go(tpl);
                g_cfg_ent_template_go = fn_obj_instantiate(tpl_go);
                if (g_cfg_ent_template_go) fn_go_set_active(g_cfg_ent_template_go, 0);
                fn_go_set_active(tpl_go, 0);
            }
        } else { LOGI("wire: cfg entry scroll content not found"); }

        void *mod_lbl_tr = tr_find(config_tr, "ModConfigsScrollLabel");
        if (mod_lbl_tr) g_sel_mod_label = get_tmp_in_go(fn_comp_get_go(mod_lbl_tr));

        void *cfg_lbl_tr = tr_find(config_tr, "SelectedConfigName");
        if (cfg_lbl_tr) g_sel_cfg_label = get_tmp_in_go(fn_comp_get_go(cfg_lbl_tr));

        void *desc_tr = tr_find(config_tr, "WhiteBackground/ModCfgDescText");
        if (desc_tr) {
            g_desc_label_comp = get_tmp_in_go(fn_comp_get_go(desc_tr));
            if (g_desc_label_comp) {
                /* Mirror PCVR: auto-size down to 6pt, truncate on overflow */
                if (fn_tmp_set_autosize) fn_tmp_set_autosize(g_desc_label_comp, 1);
                if (fn_tmp_set_fsmin)    fn_tmp_set_fsmin(g_desc_label_comp, 6.0f);
                if (fn_tmp_set_overflow) fn_tmp_set_overflow(g_desc_label_comp, 3); /* Truncate */
            }
        } else LOGI("wire: ModCfgDescText not found");

        void *holder_tr = tr_find(config_tr, "ConfigInteractionsHolder");
        if (holder_tr) {
            int nc = fn_tr_get_cc(holder_tr);
            for (int i = 0; i < nc; i++) {
                void *child_tr = fn_tr_get_child(holder_tr, i);
                void *child_go = fn_comp_get_go(child_tr);
                char  wname[32];
                cs_str_to_c(fn_obj_get_name(child_go), wname, sizeof(wname));

                if (strcmp(wname, "bool") == 0) {
                    g_bool_widget_go = child_go;
                    /* The whole bool GO is the button (Button component on root or child) */
                    void *bool_btn = fn_get_comp_ch
                        ? fn_get_comp_ch(child_tr, g_button_type_obj, 1) : NULL;
                    void *bool_btn_go = bool_btn ? fn_comp_get_go(bool_btn) : child_go;
                    register_button(bool_btn_go, on_cfg_bool_toggle, 0);
                    g_bool_val_tmp = get_tmp_in_go(child_go);
                    fn_go_set_active(child_go, 0);
                    LOGI("wire: bool widget (tmp=%p)", g_bool_val_tmp);

                } else if (strcmp(wname, "number") == 0) {
                    g_number_widget_go = child_go;
                    /* Buttons may be nested — use deep find */
                    void *vd = tr_find_deep(child_tr, "Button_ValueMinus");
                    if (vd) register_button(fn_comp_get_go(vd), on_num_val_dec, 0);
                    void *vi = tr_find_deep(child_tr, "Button_ValuePlus");
                    if (vi) register_button(fn_comp_get_go(vi), on_num_val_inc, 0);
                    void *pd = tr_find_deep(child_tr, "Button_PrecisionMinus");
                    if (pd) register_button(fn_comp_get_go(pd), on_num_step_dec, 0);
                    void *pi = tr_find_deep(child_tr, "Button_PrecisionPlus");
                    if (pi) register_button(fn_comp_get_go(pi), on_num_step_inc, 0);
                    void *val_lbl = tr_find_deep(child_tr, "Current Value Label");
                    if (val_lbl) g_num_val_tmp  = get_tmp_in_go(fn_comp_get_go(val_lbl));
                    void *step_lbl = tr_find_deep(child_tr, "StepPrecisionLabel");
                    if (step_lbl) g_num_step_tmp = get_tmp_in_go(fn_comp_get_go(step_lbl));
                    fn_go_set_active(child_go, 0);
                    LOGI("wire: number widget (val=%p step=%p)", g_num_val_tmp, g_num_step_tmp);

                } else if (strcmp(wname, "string") == 0) {
                    g_string_widget_go = child_go;
                    void *entry_tr = tr_find(child_tr, "StringEntry/Entry");
                    if (entry_tr) {
                        void *str_val_tr = tr_find(entry_tr, "ActualStringValue");
                        if (str_val_tr)
                            g_str_val_tmp = get_tmp_in_go(fn_comp_get_go(str_val_tr));
                        void *kbd_tr  = tr_find(entry_tr, "Keyboard");
                        void *grid1   = kbd_tr ? tr_find(kbd_tr, "KeyboardGrid1") : NULL;
                        void *grid2   = kbd_tr ? tr_find(kbd_tr, "KeyboardGrid2") : NULL;
                        if (grid1) {
                            for (int k = 0; k < 26; k++) {
                                char bname[16];
                                snprintf(bname, sizeof(bname), "Button_%c", 'A' + k);
                                void *bt = tr_find(grid1, bname);
                                if (bt) {
                                    void *btn_go = fn_comp_get_go(bt);
                                    register_button(btn_go, on_str_append, k);
                                    s_letter_tmps[k] = get_tmp_in_go(btn_go);
                                }
                            }
                            void *bt;
                            bt = tr_find(grid1, "Button_Dash");
                            if (bt) register_button(fn_comp_get_go(bt), on_str_append, 36);
                            bt = tr_find(grid1, "Button_BackSpace");
                            if (bt) register_button(fn_comp_get_go(bt), on_str_backspace, 0);
                            bt = tr_find(grid1, "Button_Shift");
                            if (bt) register_button(fn_comp_get_go(bt), on_str_shift, 0);
                        }
                        if (grid2) {
                            for (int k = 0; k < 10; k++) {
                                char bname[16];
                                snprintf(bname, sizeof(bname), "Button_%d", k);
                                void *bt = tr_find(grid2, bname);
                                if (bt) register_button(fn_comp_get_go(bt), on_str_append, 26 + k);
                            }
                            void *bt;
                            bt = tr_find(grid2, "Button_+");
                            if (bt) register_button(fn_comp_get_go(bt), on_str_append, 38);
                            bt = tr_find(grid2, "Button__");
                            if (bt) register_button(fn_comp_get_go(bt), on_str_append, 37);
                        }
                        if (kbd_tr) {
                            void *sub = tr_find(kbd_tr, "Button_Submit");
                            if (sub) register_button(fn_comp_get_go(sub), on_str_submit, 0);
                        }
                    } else { LOGI("wire: StringEntry/Entry not found in string widget"); }
                    fn_go_set_active(child_go, 0);
                    LOGI("wire: string widget (str_val=%p)", g_str_val_tmp);
                }
            }
        } else { LOGE("wire: ConfigInteractionsHolder not found"); }

        fn_go_set_active(g_config_panel_go, 0);
    } else { LOGE("wire: ConfigureMods not found"); }

    void *status_tr = tr_find(pt, "StatusText");
    if (status_tr) g_status_text_comp = get_tmp_in_go(fn_comp_get_go(status_tr));

    fn_go_set_active(panel_go, 0);

    if (g_manage_panel_go && g_config_panel_go) {
        g_panel_ready = 1;
        LOGI("wire_mod_panel: done. manage=%p config=%p", g_manage_panel_go, g_config_panel_go);
    } else {
        g_panel_ready = 0;
        LOGE("wire_mod_panel: critical panels missing — panel not ready (manage=%p config=%p)",
             g_manage_panel_go, g_config_panel_go);
    }
}

/* ======================================================================
 * ASSET BUNDLE STATE MACHINE
 * ====================================================================== */

void update_bundle_state(void)
{
    switch (g_bundle_state) {

    case BS_IDLE:
        if (!fn_bundle_load_async || !fn_str_new) return;
        LOGI("bundle: starting LoadFromFileAsync(%s)", BUNDLE_PATH);
        g_bundle_load_req = fn_bundle_load_async(make_str(BUNDLE_PATH));
        if (!g_bundle_load_req) {
            LOGE("bundle: LoadFromFileAsync returned null");
            g_bundle_state = BS_FAILED;
            return;
        }
        g_bundle_state = BS_LOADING_BUNDLE;
        break;

    case BS_RELOAD:
        /* Bundle is already in memory from the previous load — skip the async
         * file read and call LoadAssetAsync directly on the cached bundle. */
        if (!fn_bundle_load_asset || !fn_str_new || !g_go_type_obj) return;
        LOGI("bundle: reloading asset from existing bundle %p", g_bundle);
        g_asset_load_req = fn_bundle_load_asset(g_bundle, make_str("ModsMenu"), g_go_type_obj);
        if (!g_asset_load_req) {
            LOGE("bundle: LoadAssetAsync returned null on reload");
            g_bundle_state = BS_FAILED;
            return;
        }
        g_bundle_state = BS_LOADING_PREFAB;
        break;

    case BS_LOADING_BUNDLE:
        if (!fn_async_is_done(g_bundle_load_req)) return;
        g_bundle = fn_bundle_get_bundle(g_bundle_load_req);
        if (!g_bundle) {
            LOGE("bundle: get_assetBundle returned null");
            g_bundle_state = BS_FAILED;
            return;
        }
        LOGI("bundle: loaded %p — loading asset 'ModsMenu'", g_bundle);
        if (!g_go_type_obj) {
            LOGE("bundle: go_type_obj not resolved");
            g_bundle_state = BS_FAILED;
            return;
        }
        g_asset_load_req = fn_bundle_load_asset(g_bundle, make_str("ModsMenu"), g_go_type_obj);
        if (!g_asset_load_req) {
            LOGE("bundle: LoadAssetAsync returned null");
            g_bundle_state = BS_FAILED;
            return;
        }
        g_bundle_state = BS_LOADING_PREFAB;
        break;

    case BS_LOADING_PREFAB:
        if (!fn_async_is_done(g_asset_load_req)) return;
        {
            void *prefab = fn_req_get_asset(g_asset_load_req);
            if (!prefab) {
                LOGE("bundle: get_asset returned null");
                g_bundle_state = BS_FAILED;
                return;
            }
            LOGI("bundle: prefab=%p — instantiating under canvas", prefab);
            void *panel_go = fn_obj_inst_parent
                ? fn_obj_inst_parent(prefab, g_canvas_tr, 0)
                : fn_obj_instantiate(prefab);
            if (!panel_go) {
                LOGE("bundle: Instantiate returned null");
                g_bundle_state = BS_FAILED;
                return;
            }
            fn_obj_set_name(panel_go, make_str("AoQModManagerPanel"));
            g_panel_go = panel_go;
            fix_panel_shaders(panel_go);
            wire_mod_panel(panel_go);
            g_bundle_state = BS_DONE;
        }
        break;

    case BS_DONE:
    case BS_FAILED:
        break;
    }
}
