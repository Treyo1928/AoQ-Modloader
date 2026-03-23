/*
 * ui_inject.c — inject Mods button into the main menu canvas
 *
 * Scans scene transforms for MainMenuCanvas, clones a game button to create
 * the Mods button, adjusts MenuFront background, updates the version text,
 * and kicks off asset bundle loading.
 */

#include <string.h>
#include <stdio.h>
#include "log.h"
#include "il2cpp.h"
#include "utils.h"
#include "ui.h"
#include "modlist.h"
#include "modconfig.h"

/* ======================================================================
 * UI GLOBALS OWNED BY THIS FILE
 * ====================================================================== */

void *g_canvas_tr        = NULL;
void *g_interact_menu_tr = NULL;
void *g_mods_button_go   = NULL;
int   g_inject_done      = 0;
void *g_menu_front_mat   = NULL;
void *g_ovr_rig_tr       = NULL;

/* ======================================================================
 * inject_ui_from_canvas
 * ====================================================================== */

void inject_ui_from_canvas(void *canvas_tr)
{
    if (g_inject_done) return;

    /* Sync disabled-mods path from config before any mod scanning */
    init_disabled_dir();

    g_canvas_tr        = canvas_tr;
    g_interact_menu_tr = tr_find(canvas_tr, "InteractMenu");
    if (!g_interact_menu_tr) {
        LOGE("inject: InteractMenu not found under canvas");
        return;
    }

    int child_count = fn_tr_get_cc(g_interact_menu_tr);
    LOGI("inject: InteractMenu has %d children", child_count);
    if (child_count <= 0) { LOGE("inject: no children in InteractMenu"); return; }

    /* ── Find exit button + a template (first child with a Button) ── */
    void *exit_tr     = NULL;
    void *template_tr = NULL;
    for (int i = 0; i < child_count; i++) {
        void *child_tr = fn_tr_get_child(g_interact_menu_tr, i);
        void *child_go = fn_comp_get_go(child_tr);
        char  name_buf[64];
        cs_str_to_c(fn_obj_get_name(child_go), name_buf, sizeof(name_buf));
        char lower[64];
        int j;
        for (j = 0; j < 63 && name_buf[j]; j++) lower[j] = name_buf[j] | 0x20;
        lower[j] = '\0';
        if (strstr(lower, "exit")) {
            exit_tr = child_tr;
        } else if (!template_tr) {
            void *btn = fn_comp_get_comp(child_tr, g_button_type_obj);
            if (btn) template_tr = child_tr;
        }
    }
    if (!template_tr) template_tr = fn_tr_get_child(g_interact_menu_tr, 0);
    LOGI("inject: exit_tr=%p template_tr=%p", exit_tr, template_tr);

    /* ── Shift all non-exit buttons up +0.04 ── */
    for (int i = 0; i < child_count; i++) {
        void *child_tr = fn_tr_get_child(g_interact_menu_tr, i);
        if (child_tr == exit_tr) continue;
        Vec2 pos = fn_rect_get_apos(child_tr);
        fn_rect_set_apos(child_tr, pos.x, pos.y + 0.04f);
    }

    /* ── Position for Mods button ── */
    float mods_x, mods_y;
    if (exit_tr) {
        Vec2 exit_pos = fn_rect_get_apos(exit_tr);
        mods_x = exit_pos.x;
        mods_y = exit_pos.y + 0.04f;
    } else {
        Vec2 tpl_pos = fn_rect_get_apos(template_tr);
        mods_x = tpl_pos.x;
        mods_y = tpl_pos.y - 0.02f;
    }

    /* ── Clone template ── */
    void *template_go = fn_comp_get_go(template_tr);
    g_mods_button_go = fn_obj_instantiate(template_go);
    if (!g_mods_button_go) { LOGE("inject: Instantiate Mods button failed"); return; }

    fn_obj_set_name(g_mods_button_go, make_str("Button_Mods"));
    void *mods_tr = fn_go_get_tr(g_mods_button_go);
    fn_tr_set_parent(mods_tr, g_interact_menu_tr, 0);
    fn_rect_set_apos(mods_tr, mods_x, mods_y);
    fn_go_set_active(g_mods_button_go, 1);

    /* ── Label ── */
    void *tmp = get_tmp_in_go(g_mods_button_go);
    if (tmp) { tmp_set(tmp, "Mods"); LOGI("inject: TMP label set"); }
    else { LOGE("inject: no TMP on Mods button"); }

    /* ── Ensure interactable ── */
    if (fn_set_interactable) {
        void *btn_comp = fn_comp_get_comp(mods_tr, g_button_type_obj);
        if (btn_comp) fn_set_interactable(btn_comp, 1);
    }

    register_button(g_mods_button_go, on_mods_button, 0);

    LOGI("inject: Mods button go=%p pos=(%.3f,%.3f)", g_mods_button_go, mods_x, mods_y);

    /* ── Expand MenuFront background ── */
    void *front_tr = tr_find(g_interact_menu_tr, "MenuFront");
    if (!front_tr) {
        void *parent_tr = fn_tr_get_parent(g_interact_menu_tr);
        if (parent_tr) front_tr = tr_find(parent_tr, "MenuFront");
    }
    if (front_tr) {
        Vec2 fp = fn_rect_get_apos(front_tr);
        fn_rect_set_apos(front_tr, fp.x, 0.215f);
        Vec3 fs; fn_tr_get_lscale(&fs, front_tr);
        fn_tr_set_lscale(front_tr, fs.x, 0.34f, fs.z);
        LOGI("inject: MenuFront adjusted (scale was %.3f,%.3f,%.3f)", fs.x, fs.y, fs.z);

        /* Capture MenuFront's MeshRenderer sharedMaterial for BackgroundFront fix */
        LOGI("inject: MeshRenderer capture: fn=%p type=%p",
             (void*)fn_renderer_get_smat, g_meshrenderer_type_obj);
        if (fn_renderer_get_smat && g_meshrenderer_type_obj) {
            void *mr = fn_comp_get_comp(front_tr, g_meshrenderer_type_obj);
            if (mr) {
                g_menu_front_mat = fn_renderer_get_smat(mr);
                LOGI("inject: MenuFront MeshRenderer mat=%p", g_menu_front_mat);
            } else { LOGI("inject: MenuFront has no MeshRenderer"); }
        } else { LOGI("inject: MeshRenderer capture skipped (fn or type null)"); }
    } else { LOGI("inject: MenuFront not found"); }

    /* ── Version text ── */
    {
        /* Check ShowModdedText config (default true) */
        int show_modded = 1;
        ModConfig self_cfg;
        if (load_config(SELF_MOD_NAME, &self_cfg) == 0) {
            ModCfgEntry *e = get_entry(&self_cfg, "ShowModdedText");
            if (e) show_modded = (int)e->value_num;
        }

        if (show_modded) {
            /* Game uses "DevVersion (TMP)" on Quest; fall back to "DevVersion" */
            void *ver_tr = tr_find(canvas_tr, "DevVersion (TMP)");
            if (!ver_tr) ver_tr = tr_find(canvas_tr, "DevVersion");
            if (ver_tr) {
                void *ver_tmp = get_tmp_in_go(fn_comp_get_go(ver_tr));
                if (ver_tmp) {
                    int n = count_enabled_mods();
                    char new_ver[64];
                    snprintf(new_ver, sizeof(new_ver),
                             "MODDED (%d mod%s)", n, n == 1 ? "" : "s");
                    tmp_set(ver_tmp, new_ver);
                    LOGI("inject: version text updated");
                }
            } else { LOGI("inject: DevVersion text not found in canvas"); }
        }
    }

    /* ── Kick off bundle load / reload immediately ── */
    /* If the bundle is already in memory (previous scene), skip LoadFromFileAsync and
     * go straight to LoadAssetAsync — Unity won't load the same file twice. */
    g_bundle_state = g_bundle ? BS_RELOAD : BS_IDLE;
    g_inject_done = 1;
    /* Advance the state machine now rather than waiting for the next
     * UpdateAnchors tick — on reload this starts LoadAssetAsync this frame. */
    update_bundle_state();
    LOGI("inject: done (bundle_state=%d)", (int)g_bundle_state);
}

/* ======================================================================
 * try_inject_ui — scan all scene transforms for MainMenuCanvas
 * ====================================================================== */

void try_inject_ui(void)
{
    if (g_inject_done) return;
    if (!fn_obj_find_type || !g_transform_type_obj) {
        LOGI("try_inject: prerequisites not ready");
        return;
    }

    void *arr = fn_obj_find_type(g_transform_type_obj);
    if (!arr) { LOGI("try_inject: FindObjectsOfType returned null"); return; }

    int len = arr_len(arr);
    LOGI("try_inject: %d transforms in scene", len);

    void *canvas_tr = NULL;
    for (int i = 0; i < len && !canvas_tr; i++) {
        void *tr = arr_get(arr, i);
        if (!tr) continue;
        void *go = fn_comp_get_go(tr);
        if (!go) continue;
        if (cs_str_eq(fn_obj_get_name(go), "MainMenuCanvas"))
            canvas_tr = tr;
    }

    if (!canvas_tr && g_ovr_rig_tr) {
        /* FindObjectsOfType only returns active objects.  If the menu hierarchy
         * is disabled, fall back to navigating from OVRCameraRig (always active)
         * via the known scene path.  Transform.Find works on inactive objects. */
        void *ni_tr = tr_find(g_ovr_rig_tr,
                              "TrackingSpace/LeftHandAnchor/NonInteractMenu");
        if (ni_tr) canvas_tr = tr_find(ni_tr, "MainMenuCanvas");
        if (canvas_tr)
            LOGI("try_inject: found canvas via OVR path (was inactive)");
        else
            LOGI("try_inject: canvas not found via OVR path either");
    }

    if (!canvas_tr) { LOGI("try_inject: MainMenuCanvas not in scene"); return; }
    LOGI("try_inject: MainMenuCanvas=%p", canvas_tr);
    inject_ui_from_canvas(canvas_tr);
}
