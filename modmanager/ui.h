#pragma once
#include "il2cpp.h"
#include "modlist.h"
#include "modconfig.h"

/* ======================================================================
 * BUTTON REGISTRY
 * ====================================================================== */

typedef void (*BtnHandler)(int index);
typedef struct { void *go; BtnHandler handler; int user_index; } BtnEntry;

#define MAX_REG_BUTTONS 256
extern BtnEntry g_btn_registry[MAX_REG_BUTTONS];
extern int      g_btn_reg_count;

void register_button(void *go, BtnHandler handler, int user_index);

/* ======================================================================
 * ASSET BUNDLE STATE
 * ====================================================================== */

typedef enum {
    BS_IDLE = 0,
    BS_LOADING_BUNDLE,
    BS_RELOAD,          /* bundle already in memory — skip file load, redo asset load */
    BS_LOADING_PREFAB,
    BS_DONE,
    BS_FAILED
} BundleState;

extern BundleState  g_bundle_state;
extern void        *g_bundle_load_req;
extern void        *g_bundle;
extern void        *g_asset_load_req;

/* ======================================================================
 * GLOBAL UI STATE
 * ====================================================================== */

extern void *g_canvas_tr;
extern void *g_interact_menu_tr;
extern void *g_mods_button_go;
extern int   g_inject_done;
extern void *g_menu_front_mat;   /* sharedMaterial from game MenuFront MeshRenderer */
extern void *g_ovr_rig_tr;       /* OVRCameraRig transform — always active, used to find canvas */

extern void *g_panel_go;
extern void *g_manage_panel_go;
extern void *g_config_panel_go;
extern void *g_status_text_comp;
extern void *g_sel_mod_label;
extern void *g_sel_cfg_label;
extern void *g_desc_label_comp;   /* TMP: WhiteBackground/ModCfgDescText */
extern int   g_panel_open;
extern int   g_active_tab;
extern int   g_panel_ready;

/* Interaction widget GOs (bool / number / string) */
extern void *g_bool_widget_go;
extern void *g_number_widget_go;
extern void *g_string_widget_go;
extern void *g_bool_val_tmp;      /* TMP inside bool widget */
extern void *g_num_val_tmp;       /* TMP "Current Value Label" in number widget */
extern void *g_num_step_tmp;      /* TMP "StepPrecisionLabel" in number widget */
extern void *g_str_val_tmp;       /* TMP "ActualStringValue" in string widget */
extern double g_num_step;         /* current step precision for number widget */
extern int    g_str_shifted;      /* keyboard shift state */

extern void *g_manage_content_tr;
extern void *g_cfg_mod_content_tr;
extern void *g_cfg_ent_content_tr;
extern void *g_manage_template_go;
extern void *g_cfg_mod_template_go;
extern void *g_cfg_ent_template_go;

#define MAX_LIST_BUTTONS 48
extern void *g_manage_btns[MAX_LIST_BUTTONS];
extern int   g_manage_btn_count;
extern void *g_cfg_mod_btns[MAX_LIST_BUTTONS];
extern int   g_cfg_mod_btn_count;
extern void *g_cfg_ent_btns[MAX_LIST_BUTTONS];
extern int   g_cfg_ent_btn_count;

extern ModEntry  g_mod_entries[MAX_MODS];
extern int       g_mod_count;
extern ModConfig g_active_config;
extern int       g_config_loaded;
extern int       g_selected_entry;

/* ======================================================================
 * FUNCTION DECLARATIONS
 * ====================================================================== */

/* ui_panel.c */
void update_bundle_state(void);
void open_mod_manager(void);
void close_mod_manager(void);
void populate_cfg_entry_list(void);
void fix_panel_shaders(void *panel_go);
void on_mods_button(int idx);       /* handler for the injected Mods button */
void on_cfg_entry_select(int idx);  /* select a config entry, update widget+labels */
void reset_ui_state(void);          /* called on scene restart to allow re-injection */

/* ui_inject.c */
void inject_ui_from_canvas(void *canvas_tr);
void try_inject_ui(void);
