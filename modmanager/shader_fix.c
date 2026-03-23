/*
 * shader_fix.c — fix_panel_shaders
 *
 * Repairs the stereo rendering of the ModsMenu asset bundle panel.
 * The bundle was built on desktop, so its compiled materials lack the
 * STEREO_INSTANCING_ON GPU program variant required by Quest single-pass
 * stereo (OVR multiview).
 *
 * ── Why shader pointer swap alone doesn't work ──────────────────────────
 * Simply doing Material.shader = gameShader does NOT clear the cached
 * compiled GPU program that was linked when the bundle material was first
 * loaded.  The GL driver keeps the old (non-stereo) program until the
 * Material object itself is replaced.
 *
 * ── The fix ─────────────────────────────────────────────────────────────
 * TMP text:  TMP's own fn_tmp_set_all_dirty invalidates internal mesh state
 *            deeply enough to force a full material re-submission, which
 *            picks up the new shader.  So plain shader swap + TMP dirty works.
 *
 * Images:    Clone the Material via Object.Instantiate first.  The clone is a
 *            fresh object with no GPU program cache.  Swap the shader on the
 *            clone, then write the clone to Graphic.m_Material (offset 0xC).
 *            On first draw Unity compiles the stereo variant fresh.
 *
 * BackgroundFront: This child is a Quad with a MeshRenderer (NOT a UI Graphic).
 *            GetComponentsInChildren<Graphic> never returns it.  The "green"
 *            bundle material has no STEREO_INSTANCING_ON variant.
 *            Fix: at inject time capture the game's MenuFront MeshRenderer
 *            sharedMaterial (already stereo-compiled) into g_menu_front_mat.
 *            Here, clone that material and assign the clone via
 *            Renderer.set_sharedMaterial on BackgroundFront's MeshRenderer.
 */

#include <string.h>
#include "log.h"
#include "il2cpp.h"
#include "utils.h"
#include "ui.h"

void fix_panel_shaders(void *panel_go)
{
    if (!fn_tmp_get_smat || !fn_mat_get_shader || !fn_mat_set_shader ||
        !fn_get_comps_ch_arr || !g_tmpugui_type_obj || !g_graphic_type_obj ||
        !g_mods_button_go || !fn_obj_instantiate) {
        LOGE("fix_panel_shaders: prerequisites missing");
        return;
    }

    void *ref_tmp_shader = NULL;
    void *ref_img_mat    = NULL;   /* game Image material — used as clone source */

    void *btn_tr = fn_go_get_tr(g_mods_button_go);

    /* ── TMP shader: from Mods button's TMP fontSharedMaterial ────────── */
    {
        void *arr = fn_get_comps_ch_arr(btn_tr, g_tmpugui_type_obj, 1);
        for (int i = 0; i < arr_len(arr) && !ref_tmp_shader; i++) {
            void *comp = arr_get(arr, i);
            if (!valid_ptr(comp)) continue;
            void *mat = fn_tmp_get_smat(comp);
            if (!valid_ptr(mat)) continue;
            void *sh = fn_mat_get_shader(mat);
            if (valid_ptr(sh)) ref_tmp_shader = sh;
        }
    }

    /* ── Image material: from Mods button's first non-TMP Graphic ──────── */
    /* We save the whole material (not just its shader) so we can clone it.
     * The Mods button was cloned from a game button, so this material is
     * game-compiled with the stereo variant already baked in. */
    {
        void *arr = fn_get_comps_ch_arr(btn_tr, g_graphic_type_obj, 1);
        for (int i = 0; i < arr_len(arr) && !ref_img_mat; i++) {
            void *comp = arr_get(arr, i);
            if (!valid_ptr(comp)) continue;
            void *mat = *(void **)((uint8_t *)comp + 0xC);
            if (!valid_ptr(mat)) continue;
            void *sh = fn_mat_get_shader(mat);
            if (!valid_ptr(sh)) continue;
            char sname[64];
            cs_str_to_c(fn_obj_get_name(sh), sname, sizeof(sname));
            if (!strstr(sname, "TextMeshPro") && !strstr(sname, "TMP"))
                ref_img_mat = mat;
        }
    }

    LOGI("fix_panel_shaders: ref_tmp_sh=%p  ref_img_mat=%p",
         ref_tmp_shader,
         ref_img_mat ? fn_mat_get_shader(ref_img_mat) : NULL);

    void *panel_tr = fn_go_get_tr(panel_go);

    /* ── Canvas: ensure WorldSpace + copy worldCamera from parent canvas ── */
    if (fn_canvas_get_mode && fn_canvas_set_mode && g_canvas_comp_type) {
        void *panel_canvas = fn_comp_get_comp(panel_tr, g_canvas_comp_type);
        LOGI("fix: panel_canvas=%p  get_cam=%p  set_cam=%p  canvas_tr=%p",
             panel_canvas, (void*)fn_canvas_get_cam, (void*)fn_canvas_set_cam, g_canvas_tr);
        if (panel_canvas) {
            int mode_before = fn_canvas_get_mode(panel_canvas);
            if (mode_before != 2)
                fn_canvas_set_mode(panel_canvas, 2);
            LOGI("fix: canvas mode %d -> 2", mode_before);

            if (fn_canvas_get_cam && fn_canvas_set_cam && g_canvas_tr) {
                void *menu_canvas = fn_comp_get_comp(g_canvas_tr, g_canvas_comp_type);
                if (menu_canvas) {
                    void *vr_cam = fn_canvas_get_cam(menu_canvas);
                    LOGI("fix: parent worldCamera=%p", vr_cam);
                    fn_canvas_set_cam(panel_canvas, vr_cam);
                } else { LOGE("fix: menu_canvas not found"); }
            } else { LOGI("fix: skipping worldCamera copy (missing fn or canvas_tr)"); }
        }
    } else { LOGI("fix: canvas setup skipped (missing mode fns or type)"); }

    /* ── TMP pass: replace shader + force SetAllDirty on every panel TMP ── */
    if (ref_tmp_shader) {
        void *arr = fn_get_comps_ch_arr(panel_tr, g_tmpugui_type_obj, 1);
        int n = arr_len(arr), fixed = 0;
        for (int i = 0; i < n; i++) {
            void *comp = arr_get(arr, i);
            if (!valid_ptr(comp)) continue;
            void *mat = fn_tmp_get_smat(comp);
            if (!valid_ptr(mat)) continue;
            void *cur_sh = fn_mat_get_shader(mat);
            if (i == 0) LOGI("fix: TMP[0] mat=%p cur_sh=%p ref_sh=%p same=%d",
                              mat, cur_sh, ref_tmp_shader, cur_sh == ref_tmp_shader);
            if (cur_sh != ref_tmp_shader) {
                fn_mat_set_shader(mat, ref_tmp_shader);
                fixed++;
            }
            if (fn_tmp_set_all_dirty) fn_tmp_set_all_dirty(comp);
        }
        LOGI("fix_panel_shaders: TMP — shader swaps=%d  dirty=%d", fixed, n);
    }

    /* ── Graphic pass: for each Image with a custom material, replace it ──
     *
     * Instead of swapping the shader on the existing Material (which leaves
     * the stale cached GPU program), we:
     *   1. Clone the offending material via Object.Instantiate
     *   2. Swap the shader on the fresh clone
     *   3. Assign the clone to Graphic.m_Material (offset 0xC)
     *
     * The clone has no cached GPU program so Unity compiles the stereo
     * variant on the first draw call.
     */
    if (ref_img_mat) {
        void *ref_img_shader = fn_mat_get_shader(ref_img_mat);
        void *arr = fn_get_comps_ch_arr(panel_tr, g_graphic_type_obj, 1);
        int n = arr_len(arr), fixed = 0;
        for (int i = 0; i < n; i++) {
            void *comp = arr_get(arr, i);
            if (!valid_ptr(comp)) continue;

            void *mat = *(void **)((uint8_t *)comp + 0xC);
            if (!valid_ptr(mat)) {
                if (fn_graphic_set_dirty) fn_graphic_set_dirty(comp);
                continue;
            }

            void *cur_sh = fn_mat_get_shader(mat);
            if (!valid_ptr(cur_sh)) {
                if (fn_graphic_set_dirty) fn_graphic_set_dirty(comp);
                continue;
            }

            /* Skip TMP materials — handled above */
            char sname[64];
            cs_str_to_c(fn_obj_get_name(cur_sh), sname, sizeof(sname));
            if (strstr(sname, "TextMeshPro") || strstr(sname, "TMP")) {
                if (fn_graphic_set_dirty) fn_graphic_set_dirty(comp);
                continue;
            }

            /* Clone the material so we get a fresh GPU program cache */
            void *fresh = fn_obj_instantiate(mat);
            if (valid_ptr(fresh)) {
                fn_mat_set_shader(fresh, ref_img_shader);
                *(void **)((uint8_t *)comp + 0xC) = fresh;
                fixed++;
            } else {
                LOGE("fix: Object.Instantiate(mat) failed for a Graphic");
            }
            if (fn_graphic_set_dirty) fn_graphic_set_dirty(comp);
        }
        LOGI("fix_panel_shaders: Graphic — mat_replaces=%d  dirty=%d", fixed, n);

    }

    /* ── BackgroundFront MeshRenderer fix ──────────────────────────────────
     * BackgroundFront is a Quad (MeshFilter + MeshRenderer) with a material
     * called "green" from the asset bundle.  It is NOT a UI Graphic, so the
     * loop above never touches it.  The "green" shader has no
     * STEREO_INSTANCING_ON variant → right eye sees only the left-eye matrix
     * → panel appears invisible in the right eye.
     *
     * Fix: clone the game's own MenuFront sharedMaterial (captured at inject
     * time, already compiled with the stereo variant) and assign the clone to
     * BackgroundFront's MeshRenderer via Renderer.set_sharedMaterial.
     * Cloning ensures a fresh GPU program compilation on first draw. */
    if (g_menu_front_mat && fn_renderer_get_smat && fn_renderer_set_smat &&
        g_meshrenderer_type_obj) {
        void *bg_tr = tr_find(panel_tr, "BackgroundFront");
        LOGI("fix: BackgroundFront tr=%p  menu_front_mat=%p", bg_tr, g_menu_front_mat);
        if (bg_tr) {
            void *mr = fn_comp_get_comp(bg_tr, g_meshrenderer_type_obj);
            LOGI("fix: BackgroundFront MeshRenderer=%p", mr);
            if (mr) {
                void *cur_mat = fn_renderer_get_smat(mr);
                void *fresh   = fn_obj_instantiate(g_menu_front_mat);
                LOGI("fix: BackgroundFront cur_mat=%p  fresh=%p", cur_mat, fresh);
                if (valid_ptr(fresh)) {
                    fn_renderer_set_smat(mr, fresh);
                    LOGI("fix: BackgroundFront — assigned stereo-safe mat clone %p", fresh);
                } else { LOGE("fix: BackgroundFront — clone of menu_front_mat failed"); }
            } else { LOGE("fix: BackgroundFront has no MeshRenderer"); }
        } else { LOGE("fix: BackgroundFront transform not found"); }
    } else { LOGI("fix: BackgroundFront MeshRenderer fix skipped (missing fn or mat)"); }
}
