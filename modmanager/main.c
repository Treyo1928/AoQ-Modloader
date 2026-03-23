/*
 * main.c — hooks and entry point for AoQModManager
 *
 * Installs three hooks:
 *   StartMenu.Start      — tries UI inject as early as possible
 *   OVRCameraRig.UpdateAnchors — per-frame: late init, deferred inject, input
 *   Button.Press         — intercepts VR pointer clicks via button registry
 *
 * All application logic lives in ui_inject.c, ui_panel.c, shader_fix.c.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include "log.h"
#include "rva.h"
#include "il2cpp.h"
#include "utils.h"
#include "ui.h"
#include "bundle_data.h"
#include "../shared/inline-hook/inlineHook.h"
#include "../shared/utils/utils.h"

/* ======================================================================
 * BUNDLE EXTRACTION — write embedded bundle bytes to app data dir
 * ====================================================================== */

static void extract_bundle_to_disk(void)
{
    FILE *f = fopen(BUNDLE_PATH, "wb");
    if (!f) {
        LOGE("extract_bundle: fopen(%s) failed: %s", BUNDLE_PATH, strerror(errno));
        return;
    }
    size_t written = fwrite(g_bundle_data, 1, g_bundle_data_len, f);
    fclose(f);
    if (written != (size_t)g_bundle_data_len)
        LOGE("extract_bundle: wrote %zu/%u bytes — truncated!", written, g_bundle_data_len);
    else
        LOGI("extract_bundle: %u bytes written to %s", g_bundle_data_len, BUNDLE_PATH);
}

/* ======================================================================
 * DEFAULT CONFIG EXTRACTION — write embedded JSON if not already present
 * ====================================================================== */

#define AOQMGR_CFG_PATH \
    "/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs/aoqmodmanager.json"

static const char s_default_config[] =
    "{\n"
    "  \"entries\": [\n"
    "    {\n"
    "      \"key\": \"ShowModdedText\",\n"
    "      \"type\": \"bool\",\n"
    "      \"value\": true,\n"
    "      \"description\": \"Append a MODDED indicator and mod count to the version text in the main menu.\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"ShowAllMods\",\n"
    "      \"type\": \"bool\",\n"
    "      \"value\": false,\n"
    "      \"description\": \"Show all loaded mods in the mod list, including those with no configurable settings.\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"DisabledModsFolder\",\n"
    "      \"type\": \"string\",\n"
    "      \"value\": \"disabledmods\",\n"
    "      \"description\": \"Subfolder inside mods/ where disabled .so files are stored. Default: disabledmods (full path: mods/disabledmods/).\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"ModRefreshInterval\",\n"
    "      \"type\": \"int\",\n"
    "      \"value\": 0,\n"
    "      \"description\": \"How often (in seconds) to refresh the mod list automatically. 0 = never.\"\n"
    "    }\n"
    "  ]\n"
    "}\n";

static void extract_default_config(void)
{
    /* Only write if the file doesn't already exist — preserve user edits */
    struct stat st;
    if (stat(AOQMGR_CFG_PATH, &st) == 0) {
        LOGI("extract_default_config: config already exists, skipping");
        return;
    }

    /* Ensure the modconfigs directory exists */
    mkdir("/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs", 0755);

    FILE *f = fopen(AOQMGR_CFG_PATH, "w");
    if (!f) {
        LOGE("extract_default_config: fopen(%s) failed: %s", AOQMGR_CFG_PATH, strerror(errno));
        return;
    }
    fputs(s_default_config, f);
    fclose(f);
    LOGI("extract_default_config: wrote default config to %s", AOQMGR_CFG_PATH);
}

/* ======================================================================
 * HOOK: StartMenu.Start  (postfix)
 * ====================================================================== */

MAKE_HOOK(StartMenu_Start, RVA_StartMenu_Start, void, void *self)
{
    LOGI("HOOK StartMenu.Start fired (self=%p)", self);
    StartMenu_Start(self);
    late_init();
    if (g_inject_done) {
        LOGI("StartMenu.Start: scene restart — resetting UI state");
        reset_ui_state();
    }

    void *sm_tr          = fn_comp_get_tr(self);
    void *noninteract_tr = sm_tr ? find_ancestor_named(sm_tr, "NonInteractMenu") : NULL;
    void *canvas_tr      = noninteract_tr ? tr_find(noninteract_tr, "MainMenuCanvas") : NULL;

    LOGI("StartMenu.Start: noninteract=%p canvas=%p", noninteract_tr, canvas_tr);

    if (canvas_tr) inject_ui_from_canvas(canvas_tr);
    else           try_inject_ui();
}

/* ======================================================================
 * HOOK: OVRCameraRig.UpdateAnchors  (per-frame)
 * ====================================================================== */

MAKE_HOOK(OVRCameraRig_UpdateAnchors, RVA_OVRCameraRig_UpdateAnchors,
          void, void *self, int updateEye, int updateHand)
{
    OVRCameraRig_UpdateAnchors(self, updateEye, updateHand);

    static int s_frame = 0;
    s_frame++;

    if (s_frame == 1) {
        LOGI("HOOK OVRCameraRig.UpdateAnchors: first fire!");
        late_init();
        /* Save OVRCameraRig transform once — it's always active and lets us
         * navigate to MainMenuCanvas even when the menu hierarchy is inactive. */
        g_ovr_rig_tr = fn_comp_get_tr(self);
        LOGI("UpdateAnchors: g_ovr_rig_tr=%p", g_ovr_rig_tr);
    }

    /* Detect scene change: if our Mods button has been destroyed Unity has
     * zeroed its m_cachedPtr (offset 0x8).  Reset so re-injection can happen. */
    if (g_inject_done && g_mods_button_go && (s_frame % 120 == 0)) {
        if (!unity_alive(g_mods_button_go)) {
            LOGI("UpdateAnchors frame %d: Mods button destroyed — scene changed, resetting", s_frame);
            reset_ui_state();
        }
    }

    /* Poll every 10 frames until injection succeeds (~0.14 s max latency) */
    if (!g_inject_done && (s_frame % 10 == 0)) {
        LOGI("UpdateAnchors frame %d: inject attempt", s_frame);
        try_inject_ui();
    }

    if (g_inject_done && g_bundle_state != BS_DONE && g_bundle_state != BS_FAILED)
        update_bundle_state();

    if (!fn_ovr_getdown || !g_panel_open) return;

    if (fn_ovr_getdown(OVR_BTN_TWO, OVR_CTRL_ACTIVE)) {
        close_mod_manager();
        return;
    }

    if (g_active_tab == 1 && g_config_loaded) {
        if (fn_ovr_getdown(OVR_BTN_PRIMARY_TS_U, OVR_CTRL_ACTIVE))
            if (g_selected_entry > 0)
                on_cfg_entry_select(g_selected_entry - 1);
        if (fn_ovr_getdown(OVR_BTN_PRIMARY_TS_D, OVR_CTRL_ACTIVE))
            if (g_selected_entry < g_active_config.count - 1)
                on_cfg_entry_select(g_selected_entry + 1);
    }
}

/* ======================================================================
 * HOOK: Button.Press  (click interception via registry)
 * ====================================================================== */

MAKE_HOOK(Button_Press, RVA_Button_Press, void, void *self)
{
    void *go = fn_comp_get_go ? fn_comp_get_go(self) : NULL;
    if (go) {
        for (int i = 0; i < g_btn_reg_count; i++) {
            if (g_btn_registry[i].go == go) {
                LOGI("Button_Press: registry[%d] fired (go=%p)", i, go);
                if (g_btn_registry[i].handler)
                    g_btn_registry[i].handler(g_btn_registry[i].user_index);
                /* Do NOT call the original — Button.Press() only fires onClick
                 * listeners; the visual press transition is driven by Selectable
                 * (OnPointerDown/Up) which runs regardless.  Calling the original
                 * would also fire any prefab onClick, causing side-effects. */
                return;
            }
        }
    }
    Button_Press(self);
}

/* ======================================================================
 * MOD ENTRY POINT
 * ====================================================================== */

void modmanager_init(void)
{
    LOGI("aoqmodmanager: loading...");
    extract_bundle_to_disk();
    extract_default_config();

    long addr_ss = getRealOffset(RVA_StartMenu_Start);
    long addr_ua = getRealOffset(RVA_OVRCameraRig_UpdateAnchors);
    long addr_bp = getRealOffset(RVA_Button_Press);
    LOGI("Hook addresses: StartMenu.Start=%p  UpdateAnchors=%p  Button.Press=%p",
         (void*)addr_ss, (void*)addr_ua, (void*)addr_bp);

    INSTALL_HOOK(StartMenu_Start);
    INSTALL_HOOK(OVRCameraRig_UpdateAnchors);
    INSTALL_HOOK(Button_Press);

    LOGI("aoqmodmanager: loaded. %d mods enabled. Hooks installed.",
         count_enabled_mods());
}
