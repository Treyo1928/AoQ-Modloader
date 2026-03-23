#pragma once
#include <stdint.h>
#include <stddef.h>

/* ======================================================================
 * FUNCTION POINTER TYPES
 * ====================================================================== */

typedef void  *(*pfn_comp_get_tr)        (void *self);
typedef void  *(*pfn_comp_get_go)        (void *self);
typedef void  *(*pfn_comp_get_comp)      (void *self, void *type);
typedef void  *(*pfn_comp_get_comp_ch)   (void *self, void *type, int include_inactive);
typedef void  *(*pfn_go_get_tr)          (void *self);
typedef void   (*pfn_go_set_active)      (void *self, int value);
typedef void  *(*pfn_obj_get_name)       (void *self);
typedef void   (*pfn_obj_set_name)       (void *self, void *name_str);
typedef void  *(*pfn_obj_instantiate)    (void *original);
typedef void  *(*pfn_obj_inst_parent)    (void *original, void *parent, int worldStays);
typedef void   (*pfn_obj_dontdestroy)    (void *obj);
typedef void  *(*pfn_obj_find_type)      (void *type);
typedef void   (*pfn_tr_set_parent)      (void *self, void *parent, int worldStays);
typedef void   (*pfn_tr_set_lpos)        (void *self, float x, float y, float z);
typedef void   (*pfn_tr_set_lscale)      (void *self, float x, float y, float z);
typedef void  *(*pfn_tr_get_parent)      (void *self);
typedef int    (*pfn_tr_get_cc)          (void *self);
typedef void  *(*pfn_tr_get_child)       (void *self, int index);
typedef void  *(*pfn_tr_find)            (void *self, void *name_str);
typedef void  *(*pfn_get_comps_ch_arr)   (void *self, void *type, int includeInactive);
typedef int    (*pfn_canvas_get_mode)    (void *self);
typedef void   (*pfn_canvas_set_mode)    (void *self, int mode);
typedef void  *(*pfn_canvas_get_cam)     (void *self);
typedef void   (*pfn_canvas_set_cam)     (void *self, void *camera);
typedef void   (*pfn_graphic_set_dirty)  (void *self);
typedef void   (*pfn_graphic_set_color)  (void *self, float r, float g, float b, float a);
typedef void   (*pfn_tmp_set_all_dirty)  (void *self);
typedef void   (*pfn_tmp_set_autosize)   (void *self, int value);
typedef void   (*pfn_tmp_set_fsmin)      (void *self, float value);
typedef void   (*pfn_tmp_set_overflow)   (void *self, int mode);
typedef void  *(*pfn_renderer_get_smat)  (void *self);
typedef void   (*pfn_renderer_set_smat)  (void *self, void *mat);
typedef void  *(*pfn_shader_find)        (void *name_str);
typedef void  *(*pfn_mat_get_shader)     (void *self);
typedef void   (*pfn_mat_set_shader)     (void *self, void *shader);
typedef void   (*pfn_mat_enable_kw)      (void *self, void *keyword_str);
typedef void  *(*pfn_tmp_get_smat)       (void *self);
typedef void   (*pfn_tmp_set_smat)       (void *self, void *mat);
typedef void  *(*pfn_tmp_get_text)       (void *self);
typedef void   (*pfn_tmp_set_text)       (void *self, void *str);
typedef int    (*pfn_ovr_getdown)        (int button, int controller);
typedef int    (*pfn_ovr_getup)          (int button, int controller);
typedef void   (*pfn_go_ctor_str)        (void *self, void *name_str);
typedef void  *(*pfn_go_add_comp)        (void *self, void *type);
typedef void   (*pfn_rect_set_v2)        (void *self, float x, float y);
typedef void  *(*pfn_il2_obj_new)        (void *klass);
typedef void  *(*pfn_bundle_load_async)  (void *path_str);
typedef void  *(*pfn_bundle_load_asset)  (void *self, void *name_str, void *type);
typedef void  *(*pfn_bundle_get_bundle)  (void *req);
typedef void  *(*pfn_req_get_asset)      (void *req);
typedef int    (*pfn_async_is_done)      (void *op);
typedef void   (*pfn_set_interactable)   (void *self, int value);

/* Vec2 returned by value (8 bytes, ARM AAPCS r0:r1) */
typedef struct { float x; float y; }          Vec2;
/* Vec3 returned via hidden first-arg pointer (>8 bytes) */
typedef struct { float x; float y; float z; } Vec3;
typedef void   (*pfn_tr_get_lscale)      (Vec3 *out, void *self);
typedef Vec2   (*pfn_rect_get_apos)      (void *self);
typedef void   (*pfn_rect_set_apos)      (void *self, float x, float y);

/* IL2CPP runtime C API */
typedef void  *(*pfn_il2_str_new)    (const char *str);
typedef void  *(*pfn_il2_domain_get) (void);
typedef void **(*pfn_il2_dom_asms)   (void *domain, size_t *count);
typedef void  *(*pfn_il2_asm_image)  (void *assembly);
typedef void  *(*pfn_il2_cls_name)   (void *image, const char *ns, const char *name);
typedef const void *(*pfn_il2_cls_type)(void *klass);
typedef void  *(*pfn_il2_type_obj)   (const void *type);

/* ======================================================================
 * EXTERN DECLARATIONS — function pointers (defined in il2cpp.c)
 * ====================================================================== */

extern pfn_comp_get_tr      fn_comp_get_tr;
extern pfn_comp_get_go      fn_comp_get_go;
extern pfn_comp_get_comp    fn_comp_get_comp;
extern pfn_comp_get_comp_ch fn_get_comp_ch;
extern pfn_go_get_tr        fn_go_get_tr;
extern pfn_go_set_active    fn_go_set_active;
extern pfn_obj_get_name     fn_obj_get_name;
extern pfn_obj_set_name     fn_obj_set_name;
extern pfn_obj_instantiate  fn_obj_instantiate;
extern pfn_obj_inst_parent  fn_obj_inst_parent;
extern pfn_obj_dontdestroy  fn_obj_dontdestroy;
extern pfn_obj_find_type    fn_obj_find_type;
extern pfn_tr_set_parent    fn_tr_set_parent;
extern pfn_tr_set_lpos      fn_tr_set_lpos;
extern pfn_tr_get_lscale    fn_tr_get_lscale;
extern pfn_tr_set_lscale    fn_tr_set_lscale;
extern pfn_tr_get_parent    fn_tr_get_parent;
extern pfn_tr_get_cc        fn_tr_get_cc;
extern pfn_tr_get_child     fn_tr_get_child;
extern pfn_tr_find          fn_tr_find;
extern pfn_rect_get_apos    fn_rect_get_apos;
extern pfn_rect_set_apos    fn_rect_set_apos;
extern pfn_set_interactable fn_set_interactable;
extern pfn_get_comps_ch_arr fn_get_comps_ch_arr;
extern pfn_canvas_get_mode  fn_canvas_get_mode;
extern pfn_canvas_set_mode  fn_canvas_set_mode;
extern pfn_canvas_get_cam   fn_canvas_get_cam;
extern pfn_canvas_set_cam   fn_canvas_set_cam;
extern pfn_graphic_set_dirty  fn_graphic_set_dirty;
extern pfn_graphic_set_color  fn_graphic_set_color;
extern pfn_tmp_set_all_dirty  fn_tmp_set_all_dirty;
extern pfn_tmp_set_autosize   fn_tmp_set_autosize;
extern pfn_tmp_set_fsmin      fn_tmp_set_fsmin;
extern pfn_tmp_set_overflow   fn_tmp_set_overflow;
extern pfn_renderer_get_smat  fn_renderer_get_smat;
extern pfn_renderer_set_smat  fn_renderer_set_smat;
extern pfn_shader_find      fn_shader_find;
extern pfn_mat_get_shader   fn_mat_get_shader;
extern pfn_mat_set_shader   fn_mat_set_shader;
extern pfn_mat_enable_kw    fn_mat_enable_kw;
extern pfn_tmp_get_smat     fn_tmp_get_smat;
extern pfn_tmp_set_smat     fn_tmp_set_smat;
extern pfn_tmp_get_text     fn_tmp_get_text;
extern pfn_tmp_set_text     fn_tmp_set_text;
extern pfn_ovr_getdown      fn_ovr_getdown;
extern pfn_ovr_getup        fn_ovr_getup;
extern pfn_go_ctor_str      fn_go_ctor_str;
extern pfn_go_add_comp      fn_go_add_comp;
extern pfn_rect_set_v2      fn_rect_set_anchor_min;
extern pfn_rect_set_v2      fn_rect_set_anchor_max;
extern pfn_rect_set_v2      fn_rect_set_offset_min;
extern pfn_rect_set_v2      fn_rect_set_offset_max;
extern pfn_il2_obj_new      fn_il2_obj_new;
extern pfn_bundle_load_async fn_bundle_load_async;
extern pfn_bundle_load_asset fn_bundle_load_asset;
extern pfn_bundle_get_bundle fn_bundle_get_bundle;
extern pfn_req_get_asset    fn_req_get_asset;
extern pfn_async_is_done    fn_async_is_done;
extern pfn_il2_str_new      fn_str_new;

/* ======================================================================
 * EXTERN DECLARATIONS — IL2CPP type objects (defined in il2cpp.c)
 * ====================================================================== */

extern void *g_text_type_obj;
extern void *g_button_type_obj;
extern void *g_transform_type_obj;
extern void *g_tmpugui_type_obj;
extern void *g_go_type_obj;
extern void *g_graphic_type_obj;
extern void *g_canvas_comp_type;
extern void *g_meshrenderer_type_obj;
extern void *g_image_type_obj;
extern void *g_recttransform_type_obj;
extern void *g_go_klass;
extern void *g_il2cpp_handle;
extern int   g_late_init_done;

/* Called from hook context on first frame — resolves all pointers above */
void late_init(void);

/* Allocate and construct a new managed GameObject with the given name */
void *create_go_named(const char *name);
