#include <dlfcn.h>
#include <stddef.h>
#include "log.h"
#include "rva.h"
#include "il2cpp.h"
#include "utils.h"
#include "../shared/utils/utils.h"

/* ======================================================================
 * FUNCTION POINTER DEFINITIONS
 * ====================================================================== */

pfn_comp_get_tr      fn_comp_get_tr      = NULL;
pfn_comp_get_go      fn_comp_get_go      = NULL;
pfn_comp_get_comp    fn_comp_get_comp    = NULL;
pfn_comp_get_comp_ch fn_get_comp_ch      = NULL;
pfn_go_get_tr        fn_go_get_tr        = NULL;
pfn_go_set_active    fn_go_set_active    = NULL;
pfn_obj_get_name     fn_obj_get_name     = NULL;
pfn_obj_set_name     fn_obj_set_name     = NULL;
pfn_obj_instantiate  fn_obj_instantiate  = NULL;
pfn_obj_inst_parent  fn_obj_inst_parent  = NULL;
pfn_obj_dontdestroy  fn_obj_dontdestroy  = NULL;
pfn_obj_find_type    fn_obj_find_type    = NULL;
pfn_tr_set_parent    fn_tr_set_parent    = NULL;
pfn_tr_set_lpos      fn_tr_set_lpos      = NULL;
pfn_tr_get_lscale    fn_tr_get_lscale    = NULL;
pfn_tr_set_lscale    fn_tr_set_lscale    = NULL;
pfn_tr_get_parent    fn_tr_get_parent    = NULL;
pfn_tr_get_cc        fn_tr_get_cc        = NULL;
pfn_tr_get_child     fn_tr_get_child     = NULL;
pfn_tr_find          fn_tr_find          = NULL;
pfn_rect_get_apos    fn_rect_get_apos    = NULL;
pfn_rect_set_apos    fn_rect_set_apos    = NULL;
pfn_set_interactable fn_set_interactable = NULL;
pfn_get_comps_ch_arr fn_get_comps_ch_arr = NULL;
pfn_canvas_get_mode  fn_canvas_get_mode  = NULL;
pfn_canvas_set_mode  fn_canvas_set_mode  = NULL;
pfn_canvas_get_cam   fn_canvas_get_cam   = NULL;
pfn_canvas_set_cam   fn_canvas_set_cam   = NULL;
pfn_graphic_set_dirty  fn_graphic_set_dirty  = NULL;
pfn_graphic_set_color  fn_graphic_set_color  = NULL;
pfn_tmp_set_all_dirty  fn_tmp_set_all_dirty  = NULL;
pfn_tmp_set_autosize   fn_tmp_set_autosize   = NULL;
pfn_tmp_set_fsmin      fn_tmp_set_fsmin      = NULL;
pfn_tmp_set_overflow   fn_tmp_set_overflow   = NULL;
pfn_renderer_get_smat  fn_renderer_get_smat  = NULL;
pfn_renderer_set_smat  fn_renderer_set_smat  = NULL;
pfn_shader_find      fn_shader_find      = NULL;
pfn_mat_get_shader   fn_mat_get_shader   = NULL;
pfn_mat_set_shader   fn_mat_set_shader   = NULL;
pfn_mat_enable_kw    fn_mat_enable_kw    = NULL;
pfn_tmp_get_smat     fn_tmp_get_smat     = NULL;
pfn_tmp_set_smat     fn_tmp_set_smat     = NULL;
pfn_tmp_get_text     fn_tmp_get_text     = NULL;
pfn_tmp_set_text     fn_tmp_set_text     = NULL;
pfn_ovr_getdown      fn_ovr_getdown      = NULL;
pfn_ovr_getup        fn_ovr_getup        = NULL;
pfn_go_ctor_str      fn_go_ctor_str      = NULL;
pfn_go_add_comp      fn_go_add_comp      = NULL;
pfn_rect_set_v2      fn_rect_set_anchor_min = NULL;
pfn_rect_set_v2      fn_rect_set_anchor_max = NULL;
pfn_rect_set_v2      fn_rect_set_offset_min = NULL;
pfn_rect_set_v2      fn_rect_set_offset_max = NULL;
pfn_il2_obj_new      fn_il2_obj_new      = NULL;
pfn_bundle_load_async fn_bundle_load_async = NULL;
pfn_bundle_load_asset fn_bundle_load_asset = NULL;
pfn_bundle_get_bundle fn_bundle_get_bundle = NULL;
pfn_req_get_asset    fn_req_get_asset    = NULL;
pfn_async_is_done    fn_async_is_done    = NULL;
pfn_il2_str_new      fn_str_new          = NULL;

/* ======================================================================
 * TYPE OBJECT DEFINITIONS
 * ====================================================================== */

void *g_text_type_obj      = NULL;
void *g_button_type_obj    = NULL;
void *g_transform_type_obj = NULL;
void *g_tmpugui_type_obj   = NULL;
void *g_go_type_obj        = NULL;
void *g_graphic_type_obj      = NULL;
void *g_canvas_comp_type      = NULL;
void *g_meshrenderer_type_obj = NULL;
void *g_image_type_obj        = NULL;
void *g_recttransform_type_obj= NULL;
void *g_go_klass              = NULL;
void *g_il2cpp_handle         = NULL;
int   g_late_init_done     = 0;

/* ======================================================================
 * IL2CPP HELPERS
 * ====================================================================== */

static pfn_il2_domain_get fn_domain_get  = NULL;
static pfn_il2_dom_asms   fn_dom_asms    = NULL;
static pfn_il2_asm_image  fn_asm_image   = NULL;
static pfn_il2_cls_name   fn_cls_name    = NULL;
static pfn_il2_cls_type   fn_cls_type    = NULL;
static pfn_il2_type_obj   fn_type_obj    = NULL;

static void *resolve_sym(const char *sym_name)
{
    if (!g_il2cpp_handle) {
        g_il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!g_il2cpp_handle) g_il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY);
        if (!g_il2cpp_handle) { LOGE("dlopen(libil2cpp.so) failed"); return NULL; }
    }
    void *p = dlsym(g_il2cpp_handle, sym_name);
    if (!p) LOGE("dlsym(%s) failed", sym_name);
    return p;
}

static void *find_class_in_assemblies(const char *ns, const char *name)
{
    if (!fn_domain_get || !fn_dom_asms || !fn_asm_image || !fn_cls_name) return NULL;
    void *domain = fn_domain_get();
    if (!domain) return NULL;
    size_t count = 0;
    void **asms = fn_dom_asms(domain, &count);
    if (!asms) return NULL;
    for (size_t i = 0; i < count; i++) {
        void *image = fn_asm_image(asms[i]);
        if (!image) continue;
        void *klass = fn_cls_name(image, ns, name);
        if (klass) return klass;
    }
    return NULL;
}

void *make_type_obj(const char *ns, const char *name)
{
    void *klass = find_class_in_assemblies(ns, name);
    if (!klass) { LOGE("make_type_obj: %s.%s not found", ns, name); return NULL; }
    if (!fn_cls_type || !fn_type_obj) return NULL;
    const void *type = fn_cls_type(klass);
    return type ? fn_type_obj(type) : NULL;
}

/* ======================================================================
 * LATE INIT — resolves all function pointers, called from hook context
 * ====================================================================== */

void late_init(void)
{
    if (g_late_init_done) return;
    g_late_init_done = 1;

    LOGI("late_init: resolving...");

    fn_str_new     = (pfn_il2_str_new)   resolve_sym("il2cpp_string_new");
    fn_domain_get  = (pfn_il2_domain_get)resolve_sym("il2cpp_domain_get");
    fn_dom_asms    = (pfn_il2_dom_asms)  resolve_sym("il2cpp_domain_get_assemblies");
    fn_asm_image   = (pfn_il2_asm_image) resolve_sym("il2cpp_assembly_get_image");
    fn_cls_name    = (pfn_il2_cls_name)  resolve_sym("il2cpp_class_from_name");
    fn_cls_type    = (pfn_il2_cls_type)  resolve_sym("il2cpp_class_get_type");
    fn_type_obj    = (pfn_il2_type_obj)  resolve_sym("il2cpp_type_get_object");

    fn_comp_get_tr  = (pfn_comp_get_tr)      getRealOffset(RVA_Component_get_transform);
    fn_comp_get_go  = (pfn_comp_get_go)      getRealOffset(RVA_Component_get_gameObject);
    fn_comp_get_comp= (pfn_comp_get_comp)    getRealOffset(RVA_Component_GetComponent);
    fn_get_comp_ch  = (pfn_comp_get_comp_ch) getRealOffset(RVA_Component_GetCompInChildren);
    fn_go_get_tr    = (pfn_go_get_tr)        getRealOffset(RVA_GO_get_transform);
    fn_go_set_active= (pfn_go_set_active)    getRealOffset(RVA_GO_SetActive);
    fn_obj_get_name = (pfn_obj_get_name)     getRealOffset(RVA_Object_get_name);
    fn_obj_set_name = (pfn_obj_set_name)     getRealOffset(RVA_Object_set_name);
    fn_obj_instantiate=(pfn_obj_instantiate) getRealOffset(RVA_Object_Instantiate);
    fn_obj_inst_parent=(pfn_obj_inst_parent) getRealOffset(RVA_Object_Instantiate_Parent);
    fn_obj_dontdestroy=(pfn_obj_dontdestroy) getRealOffset(RVA_Object_DontDestroyOnLoad);
    fn_obj_find_type= (pfn_obj_find_type)    getRealOffset(RVA_Object_FindObjectsOfType);
    fn_tr_set_parent= (pfn_tr_set_parent)    getRealOffset(RVA_Transform_SetParent_bool);
    fn_tr_set_lpos  = (pfn_tr_set_lpos)      getRealOffset(RVA_Transform_set_localPosition);
    fn_tr_get_lscale= (pfn_tr_get_lscale)    getRealOffset(RVA_Transform_get_localScale);
    fn_tr_set_lscale= (pfn_tr_set_lscale)    getRealOffset(RVA_Transform_set_localScale);
    fn_tr_get_parent= (pfn_tr_get_parent)    getRealOffset(RVA_Transform_get_parent);
    fn_tr_get_cc    = (pfn_tr_get_cc)        getRealOffset(RVA_Transform_get_childCount);
    fn_tr_get_child = (pfn_tr_get_child)     getRealOffset(RVA_Transform_GetChild);
    fn_tr_find      = (pfn_tr_find)          getRealOffset(RVA_Transform_Find);
    fn_rect_get_apos= (pfn_rect_get_apos)    getRealOffset(RVA_RectTransform_get_anchoredPos);
    fn_rect_set_apos= (pfn_rect_set_apos)    getRealOffset(RVA_RectTransform_set_anchoredPos);
    fn_set_interactable =(pfn_set_interactable)  getRealOffset(RVA_Selectable_set_interactable);
    fn_get_comps_ch_arr =(pfn_get_comps_ch_arr)  getRealOffset(RVA_Component_GetComponentsInCh);
    fn_canvas_get_mode  =(pfn_canvas_get_mode)   getRealOffset(RVA_Canvas_get_renderMode);
    fn_canvas_set_mode  =(pfn_canvas_set_mode)   getRealOffset(RVA_Canvas_set_renderMode);
    fn_canvas_get_cam   =(pfn_canvas_get_cam)    getRealOffset(RVA_Canvas_get_worldCamera);
    fn_canvas_set_cam   =(pfn_canvas_set_cam)    getRealOffset(RVA_Canvas_set_worldCamera);
    fn_graphic_set_dirty =(pfn_graphic_set_dirty) getRealOffset(RVA_Graphic_SetAllDirty);
    fn_graphic_set_color =(pfn_graphic_set_color) getRealOffset(RVA_Graphic_set_color);
    fn_tmp_set_all_dirty =(pfn_tmp_set_all_dirty) getRealOffset(RVA_TMP_SetAllDirty);
    fn_tmp_set_autosize  =(pfn_tmp_set_autosize)  getRealOffset(RVA_TMP_set_enableAutoSizing);
    fn_tmp_set_fsmin     =(pfn_tmp_set_fsmin)     getRealOffset(RVA_TMP_set_fontSizeMin);
    fn_tmp_set_overflow  =(pfn_tmp_set_overflow)  getRealOffset(RVA_TMP_set_overflowMode);
    fn_renderer_get_smat =(pfn_renderer_get_smat) getRealOffset(RVA_Renderer_get_sharedMaterial);
    fn_renderer_set_smat =(pfn_renderer_set_smat) getRealOffset(RVA_Renderer_set_sharedMaterial);
    fn_shader_find      = (pfn_shader_find)      getRealOffset(RVA_Shader_Find);
    fn_mat_get_shader   = (pfn_mat_get_shader)   getRealOffset(RVA_Material_get_shader);
    fn_mat_set_shader   = (pfn_mat_set_shader)   getRealOffset(RVA_Material_set_shader);
    fn_mat_enable_kw    = (pfn_mat_enable_kw)    getRealOffset(RVA_Material_EnableKeyword);
    fn_tmp_get_smat     = (pfn_tmp_get_smat)     getRealOffset(RVA_TMP_get_sharedMaterial);
    fn_tmp_set_smat     = (pfn_tmp_set_smat)     getRealOffset(RVA_TMP_set_sharedMaterial);
    fn_tmp_get_text     = (pfn_tmp_get_text)     getRealOffset(RVA_TMP_get_text);
    fn_tmp_set_text     = (pfn_tmp_set_text)     getRealOffset(RVA_TMP_set_text);
    fn_ovr_getdown      = (pfn_ovr_getdown)      getRealOffset(RVA_OVRInput_GetDown);
    fn_ovr_getup        = (pfn_ovr_getup)        getRealOffset(RVA_OVRInput_GetUp);
    fn_go_ctor_str      = (pfn_go_ctor_str)      getRealOffset(RVA_GameObject_ctor_str);
    fn_go_add_comp      = (pfn_go_add_comp)      getRealOffset(RVA_GameObject_AddComponent);
    fn_rect_set_anchor_min = (pfn_rect_set_v2)   getRealOffset(RVA_RectTransform_set_anchorMin);
    fn_rect_set_anchor_max = (pfn_rect_set_v2)   getRealOffset(RVA_RectTransform_set_anchorMax);
    fn_rect_set_offset_min = (pfn_rect_set_v2)   getRealOffset(RVA_RectTransform_set_offsetMin);
    fn_rect_set_offset_max = (pfn_rect_set_v2)   getRealOffset(RVA_RectTransform_set_offsetMax);
    fn_il2_obj_new      = (pfn_il2_obj_new)      resolve_sym("il2cpp_object_new");
    fn_bundle_load_async= (pfn_bundle_load_async)getRealOffset(RVA_AssetBundle_LoadFromFileAsync);
    fn_bundle_load_asset= (pfn_bundle_load_asset)getRealOffset(RVA_AssetBundle_LoadAssetAsync);
    fn_bundle_get_bundle= (pfn_bundle_get_bundle)getRealOffset(RVA_ABCreateReq_get_assetBundle);
    fn_req_get_asset    = (pfn_req_get_asset)    getRealOffset(RVA_ABRequest_get_asset);
    fn_async_is_done    = (pfn_async_is_done)    getRealOffset(RVA_AsyncOp_get_isDone);

    g_text_type_obj     = make_type_obj("UnityEngine.UI", "Text");
    g_button_type_obj   = make_type_obj("UnityEngine.UI", "Button");
    g_transform_type_obj= make_type_obj("UnityEngine", "Transform");
    g_tmpugui_type_obj  = make_type_obj("TMPro", "TextMeshProUGUI");
    g_go_type_obj       = make_type_obj("UnityEngine", "GameObject");
    g_graphic_type_obj      = make_type_obj("UnityEngine.UI", "Graphic");
    g_canvas_comp_type      = make_type_obj("UnityEngine", "Canvas");
    g_meshrenderer_type_obj = make_type_obj("UnityEngine", "MeshRenderer");
    g_image_type_obj        = make_type_obj("UnityEngine.UI", "Image");
    g_recttransform_type_obj= make_type_obj("UnityEngine", "RectTransform");
    g_go_klass              = find_class_in_assemblies("UnityEngine", "GameObject");

    LOGI("late_init: done. tmpugui=%p go_type=%p bundle_load=%p image=%p go_klass=%p",
         g_tmpugui_type_obj, g_go_type_obj, (void*)fn_bundle_load_async,
         g_image_type_obj, g_go_klass);
}

/* ======================================================================
 * create_go_named — allocate + construct a managed GameObject
 * ====================================================================== */

void *create_go_named(const char *name)
{
    if (!fn_il2_obj_new || !fn_go_ctor_str || !g_go_klass) {
        LOGE("create_go_named: prerequisites not ready (obj_new=%p ctor=%p klass=%p)",
             (void*)fn_il2_obj_new, (void*)fn_go_ctor_str, g_go_klass);
        return NULL;
    }
    void *obj = fn_il2_obj_new(g_go_klass);
    if (!obj) { LOGE("create_go_named: il2cpp_object_new returned NULL"); return NULL; }
    fn_go_ctor_str(obj, make_str(name));
    return obj;
}
