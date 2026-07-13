/*
 * libaoqcore — implementation
 *
 * All RVAs are relative to libil2cpp.so (verified with Il2CppDumper against
 * AoQ 0.5.0 / AoQ-0.5.0.apk).  getRealOffset() adds the ASLR base at runtime.
 *
 * IL2CPP C API functions (il2cpp_*) are resolved at runtime via dlsym so
 * the library works regardless of how the runtime is loaded.
 */

#include "aoq.h"
#include "../utils/utils.h"

#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

#define AOQ_TAG "libaoqcore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  AOQ_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, AOQ_TAG, __VA_ARGS__)


/* ====================================================================
 * GAME / ENGINE RVAs  (all from Il2CppDumper on AoQ 0.5.0)
 * ==================================================================== */

/* UnityEngine.GameObject */
#define RVA_GO_CTOR_STR          0xC74738   /* .ctor(string name)          */
#define RVA_GO_GET_TRANSFORM     0xC744F8   /* get_transform()             */
#define RVA_GO_ADD_COMPONENT     0xC744A0   /* AddComponent(Type type)     */
#define RVA_GO_GET_COMPONENT     0xC6A3E4   /* GetComponent(Type type)     */

/* UnityEngine.Component */
#define RVA_COMP_GET_TRANSFORM   0xC6A29C   /* get_transform()             */

/* UnityEngine.Object */
#define RVA_OBJ_DESTROY          0xCEBE04   /* static Destroy(Object)      */
#define RVA_GO_SET_ACTIVE        0xC745F0   /* GameObject.SetActive(bool)  */

/* UnityEngine.Transform */
#define RVA_TR_SET_PARENT        0xFD6444   /* SetParent(Transform, bool)  */
#define RVA_TR_SET_LOCAL_POS     0xFD51E8   /* set_localPosition(Vector3)  */
#define RVA_TR_SET_LOCAL_SCALE   0xFD60E8   /* set_localScale(Vector3)     */
#define RVA_TR_LOOKAT_TR         0xFD6E44   /* LookAt(Transform target)    */
#define RVA_TR_ROTATE_FFF        0xFD6E1C   /* Rotate(float,float,float)   */

/* UnityEngine.Camera */
#define RVA_CAM_GET_MAIN         0xC68014   /* static Camera get_main()    */

/* Photon.Pun */
#define RVA_MBP_GET_PHOTONVIEW   0x9A0574   /* MonoBehaviourPun.get_photonView */
#define RVA_PV_GET_ISMINE        0x1089D98  /* PhotonView.get_IsMine       */
#define RVA_PN_GET_ISMASTER      0xA62940   /* PhotonNetwork.get_IsMasterClient */

/* TMPro.TMP_Text  (base class — TextMeshPro inherits these) */
#define RVA_TMP_SET_TEXT         0x43aac8   /* set_text(Il2CppString*)     */
#define RVA_TMP_SET_COLOR        0x43afb8   /* set_color(Color)            */
#define RVA_TMP_SET_FONT_SIZE    0x43b8dc   /* set_fontSize(float)         */
#define RVA_TMP_SET_ALIGNMENT    0x43bd50   /* set_alignment(int)          */

/* TypeInfo globals (double-deref to get Il2CppClass*) */
#define RVA_TI_GAMEOBJECT        0x15b4b64  /* UnityEngine.GameObject      */
#define RVA_TI_TMP_TEXT          0x15bae84  /* TMPro.TMP_Text (for image)  */

/* Byte offset of static_fields inside Il2CppClass (from il2cpp.h layout) */
#define IL2CPP_CLASS_IMAGE_OFF         0x00   /* Il2CppClass_1.image        */
#define IL2CPP_CLASS_STATIC_FIELDS_OFF 0x5C   /* Il2CppClass.static_fields  */

/* Byte offset of m_CachedPtr inside UnityEngine.Object (from dump.cs).
 * Unity zeroes it when the native object is destroyed. */
#define UNITY_OBJ_CACHED_PTR_OFF       0x8


/* ====================================================================
 * FUNCTION POINTER TYPES
 * ==================================================================== */

/* IL2CPP C API (resolved via dlsym) */
typedef void  *(*pfn_obj_new)         (void *klass);
typedef void  *(*pfn_str_new)         (const char *str);
typedef void  *(*pfn_domain_get)      (void);
typedef void  **(*pfn_domain_assemblies)(void *domain, size_t *count);
typedef void  *(*pfn_assembly_get_image)(void *assembly);
typedef void  *(*pfn_class_from_name) (void *image, const char *ns, const char *name);
typedef const void *(*pfn_class_get_type)(void *klass);
typedef void  *(*pfn_type_get_object) (const void *type);

/* Unity engine functions (resolved via getRealOffset) */
typedef void   (*pfn_go_ctor_str)     (void *self, void *name_str);
typedef void  *(*pfn_go_get_tr)       (void *self);
typedef void  *(*pfn_go_add_comp)     (void *self, void *type_obj);
typedef void  *(*pfn_go_get_comp)     (void *self, void *type_obj);
typedef void  *(*pfn_comp_get_tr)     (void *self);
typedef void   (*pfn_obj_destroy)     (void *obj);
typedef void   (*pfn_go_set_active)   (void *self, int value);
typedef void   (*pfn_tr_set_parent)   (void *self, void *parent, int worldStays);
typedef void   (*pfn_tr_set_local_pos)(void *self, float x, float y, float z);
typedef void   (*pfn_tr_set_local_scl)(void *self, float x, float y, float z);
typedef void   (*pfn_tr_lookat_tr)    (void *self, void *target);
typedef void   (*pfn_tr_rotate_fff)   (void *self, float x, float y, float z);
typedef void  *(*pfn_cam_get_main)    (void);
typedef void  *(*pfn_get_photon_view) (void *self);
typedef int    (*pfn_get_is_mine)     (void *self);
typedef int    (*pfn_get_is_master)   (void);
typedef void   (*pfn_tmp_set_text)    (void *self, void *str);
typedef void   (*pfn_tmp_set_color)   (void *self, float r, float g, float b, float a);
typedef void   (*pfn_tmp_set_size)    (void *self, float size);
typedef void   (*pfn_tmp_set_align)   (void *self, int align);


/* ====================================================================
 * STATIC STATE
 * ==================================================================== */

static int g_init_done = 0;
static int g_classes_done = 0;

static void aoq_late_init(void);

/* Handle to libil2cpp.so — Unity loads it RTLD_LOCAL so RTLD_DEFAULT won't see it */
static void *g_il2cpp_handle = NULL;

/* IL2CPP C API */
static pfn_obj_new          fn_obj_new          = NULL;
static pfn_str_new          fn_str_new          = NULL;
static pfn_domain_get       fn_domain_get       = NULL;
static pfn_domain_assemblies fn_domain_assemblies = NULL;
static pfn_assembly_get_image fn_assembly_image = NULL;
static pfn_class_from_name  fn_class_from_name  = NULL;
static pfn_class_get_type   fn_class_get_type   = NULL;
static pfn_type_get_object  fn_type_get_object  = NULL;

/* Unity engine */
static pfn_go_ctor_str      fn_go_ctor_str      = NULL;
static pfn_go_get_tr        fn_go_get_tr        = NULL;
static pfn_go_add_comp      fn_go_add_comp      = NULL;
static pfn_go_get_comp      fn_go_get_comp      = NULL;
static pfn_comp_get_tr      fn_comp_get_tr      = NULL;
static pfn_obj_destroy      fn_obj_destroy      = NULL;
static pfn_go_set_active    fn_go_set_active    = NULL;
static pfn_tr_set_parent    fn_tr_set_parent    = NULL;
static pfn_tr_set_local_pos fn_tr_set_local_pos = NULL;
static pfn_tr_set_local_scl fn_tr_set_local_scl = NULL;
static pfn_tr_lookat_tr     fn_tr_lookat_tr     = NULL;
static pfn_tr_rotate_fff    fn_tr_rotate_fff    = NULL;
static pfn_cam_get_main     fn_cam_get_main     = NULL;
static pfn_get_photon_view  fn_get_photon_view  = NULL;
static pfn_get_is_mine      fn_get_is_mine      = NULL;
static pfn_get_is_master    fn_get_is_master    = NULL;
static pfn_tmp_set_text     fn_tmp_set_text     = NULL;
static pfn_tmp_set_color    fn_tmp_set_color    = NULL;
static pfn_tmp_set_size     fn_tmp_set_size     = NULL;
static pfn_tmp_set_align    fn_tmp_set_align    = NULL;

/* Cached Il2CppClass* for GameObject (needed for il2cpp_object_new) */
static void *g_go_klass     = NULL;

/* Cached System.Type* for TextMeshPro (needed for AddComponent) */
static void *g_tmp_type_obj = NULL;


/* ====================================================================
 * HELPER: resolve dlsym pointer, log on failure
 * ==================================================================== */
static void *resolve_sym(const char *name)
{
    if (!g_il2cpp_handle) {
        /* RTLD_NOLOAD: get handle to already-loaded library without loading it again */
        g_il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!g_il2cpp_handle)
            LOGE("dlopen(libil2cpp.so, RTLD_NOLOAD) failed: %s", dlerror());
    }
    void *p = g_il2cpp_handle ? dlsym(g_il2cpp_handle, name) : NULL;
    if (p == NULL)
        LOGE("dlsym(\"%s\") failed", name);
    return p;
}

/* ====================================================================
 * HELPER: find an Il2CppClass* by iterating assemblies
 * ==================================================================== */
static void *find_class_in_all_assemblies(const char *ns, const char *name)
{
    if (!fn_domain_get || !fn_domain_assemblies ||
        !fn_assembly_image || !fn_class_from_name)
        return NULL;

    void *domain = fn_domain_get();
    if (!domain) { LOGE("il2cpp_domain_get returned NULL"); return NULL; }

    size_t count = 0;
    void **assemblies = fn_domain_assemblies(domain, &count);
    if (!assemblies) { LOGE("il2cpp_domain_get_assemblies returned NULL"); return NULL; }

    for (size_t i = 0; i < count; i++) {
        void *image = fn_assembly_image(assemblies[i]);
        if (!image) continue;
        void *klass = fn_class_from_name(image, ns, name);
        if (klass) return klass;
    }

    LOGE("class %s.%s not found in any assembly", ns, name);
    return NULL;
}


/* ====================================================================
 * aoq_init  — called from lib_main() constructor
 *
 * Intentionally does nothing beyond marking that init was requested.
 * All actual resolution (getRealOffset + dlsym + TypeInfo) is deferred
 * to aoq_late_init(), which runs from inside a hook when Unity is fully
 * initialised.  Any work done here at constructor time risks interfering
 * with the engine's own early startup.
 * ==================================================================== */
void aoq_init(void)
{
    if (g_init_done) return;
    g_init_done = 1;
    LOGI("aoq_init called — all resolution deferred to first hook");
}


/* ====================================================================
 * OBJECT LIFETIME
 * ==================================================================== */

int aoq_alive(void *unity_obj)
{
    if (!unity_obj) return 0;
    return AOQ_FIELD(unity_obj, UNITY_OBJ_CACHED_PTR_OFF, intptr_t) != 0;
}

void aoq_destroy(void *unity_obj)
{
    aoq_late_init();
    if (!unity_obj || !fn_obj_destroy) return;
    if (!aoq_alive(unity_obj)) return;   /* already gone */
    fn_obj_destroy(unity_obj);
}

void aoq_go_set_active(void *go, int value)
{
    aoq_late_init();
    if (!fn_go_set_active || !aoq_alive(go)) return;
    fn_go_set_active(go, value);
}


/* ====================================================================
 * PHOTON HELPERS
 * ==================================================================== */

void *aoq_photon_view(void *mbp)
{
    aoq_late_init();
    if (!mbp || !fn_get_photon_view) return NULL;
    return fn_get_photon_view(mbp);
}

int aoq_is_mine(void *mbp)
{
    aoq_late_init();
    if (!fn_get_is_mine) return 0;
    void *pv = aoq_photon_view(mbp);
    if (!pv) return 0;
    return fn_get_is_mine(pv) ? 1 : 0;
}

int aoq_is_master(void)
{
    aoq_late_init();
    if (!fn_get_is_master) return 0;
    return fn_get_is_master() ? 1 : 0;
}


/* ====================================================================
 * CLASS / COMPONENT LOOKUP
 * ==================================================================== */

void *aoq_find_class(const char *ns, const char *name)
{
    aoq_late_init();
    return find_class_in_all_assemblies(ns, name);
}

void *aoq_type_object(void *klass)
{
    aoq_late_init();
    if (!klass || !fn_class_get_type || !fn_type_get_object) return NULL;
    const void *type = fn_class_get_type(klass);
    return type ? fn_type_get_object(type) : NULL;
}

void *aoq_go_get_component(void *go, void *type_obj)
{
    aoq_late_init();
    if (!go || !type_obj || !fn_go_get_comp) return NULL;
    return fn_go_get_comp(go, type_obj);
}

void *aoq_go_get_component_named(void *go, const char *ns, const char *name)
{
    void *klass = aoq_find_class(ns, name);
    void *type  = klass ? aoq_type_object(klass) : NULL;
    return type ? aoq_go_get_component(go, type) : NULL;
}

void *aoq_static_fields(long typeinfo_rva)
{
    void **ppClass = (void **)getRealOffset(typeinfo_rva);
    if (!ppClass || !*ppClass) return NULL;
    return AOQ_FIELD(*ppClass, IL2CPP_CLASS_STATIC_FIELDS_OFF, void *);
}


/* ====================================================================
 * CONFIG CACHE
 * ==================================================================== */

int aoq_cfg_refresh(AoqCfgCache *c, const char *so_name)
{
    if (!c || !so_name) return -1;
    ModConfig fresh;
    if (load_config(so_name, &fresh) != 0)
        return -1;                 /* keep previous values on failure */
    c->cfg    = fresh;
    c->loaded = 1;
    return 0;
}

double aoq_cfg_num(AoqCfgCache *c, const char *key, double fallback)
{
    if (!c || !c->loaded) return fallback;
    ModCfgEntry *e = get_entry(&c->cfg, key);
    return e ? e->value_num : fallback;
}

float aoq_cfg_flt(AoqCfgCache *c, const char *key, float fallback)
{
    return (float)aoq_cfg_num(c, key, (double)fallback);
}

int aoq_cfg_int(AoqCfgCache *c, const char *key, int fallback)
{
    return (int)aoq_cfg_num(c, key, (double)fallback);
}

int aoq_cfg_bool(AoqCfgCache *c, const char *key, int fallback)
{
    return aoq_cfg_num(c, key, (double)fallback) != 0.0;
}


/* ====================================================================
 * INPUT: TAP / HOLD
 * ==================================================================== */

int aoq_tap_hold(AoqTapHold *st, int down, int held, float now, float hold_secs)
{
    if (!st) return AOQ_INPUT_NONE;

    if (down) {
        st->press_start = now;
        st->pressing    = 1;
    }

    if (held) {
        if (st->pressing && (now - st->press_start) > hold_secs) {
            st->pressing = 0;
            return AOQ_INPUT_HOLD;
        }
        return AOQ_INPUT_NONE;
    }

    if (st->pressing) {
        st->pressing = 0;
        return AOQ_INPUT_TAP;
    }
    return AOQ_INPUT_NONE;
}


/* ====================================================================
 * aoq_camera_main
 * ==================================================================== */
void *aoq_camera_main(void)
{
    aoq_late_init();
    if (!fn_cam_get_main) return NULL;
    return fn_cam_get_main();
}


/* ====================================================================
 * TRANSFORM HELPERS
 * ==================================================================== */

void *aoq_get_transform(void *component)
{
    aoq_late_init();
    if (!component || !fn_comp_get_tr) return NULL;
    return fn_comp_get_tr(component);
}

void *aoq_get_transform_go(void *go)
{
    aoq_late_init();
    if (!go || !fn_go_get_tr) return NULL;
    return fn_go_get_tr(go);
}

void aoq_set_parent(void *transform, void *parent, int worldPositionStays)
{
    aoq_late_init();
    if (!transform || !fn_tr_set_parent) return;
    fn_tr_set_parent(transform, parent, worldPositionStays);
}

void aoq_set_local_position(void *transform, float x, float y, float z)
{
    aoq_late_init();
    if (!transform || !fn_tr_set_local_pos) return;
    fn_tr_set_local_pos(transform, x, y, z);
}

void aoq_set_local_scale(void *transform, float x, float y, float z)
{
    aoq_late_init();
    if (!transform || !fn_tr_set_local_scl) return;
    fn_tr_set_local_scl(transform, x, y, z);
}

void aoq_billboard(void *transform, void *target_transform)
{
    aoq_late_init();
    if (!transform || !target_transform) return;
    if (!fn_tr_lookat_tr || !fn_tr_rotate_fff) return;
    fn_tr_lookat_tr(transform, target_transform);
    fn_tr_rotate_fff(transform, 0.0f, 180.0f, 0.0f);
}


/* ====================================================================
 * aoq_late_init  (lazy — called from hook context, not lib_main)
 *
 * Resolves everything that requires libil2cpp.so to be loaded:
 *   1. IL2CPP C API symbols via dlsym (libil2cpp.so is RTLD_LOCAL so
 *      we need an explicit handle — not available at constructor time)
 *   2. Il2CppClass* TypeInfo globals (lazily initialised by IL2CPP,
 *      always NULL until Unity has started running)
 * Both are safe only once a hook has fired.
 * ==================================================================== */
static void aoq_late_init(void)
{
    if (g_classes_done) return;

    /* ── Open explicit handle to libil2cpp.so ────────────────────────── */
    if (!g_il2cpp_handle) {
        /* Try RTLD_NOLOAD first; if it fails the lib isn't loaded yet and
         * we fall back to a plain RTLD_LAZY open (safe — already in mem). */
        g_il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!g_il2cpp_handle)
            g_il2cpp_handle = dlopen("libil2cpp.so", RTLD_LAZY);
        if (!g_il2cpp_handle)
            LOGE("dlopen(libil2cpp.so) failed: %s", dlerror());
        else
            LOGI("libil2cpp.so handle: %p", g_il2cpp_handle);
    }

    /* ── IL2CPP C API via dlsym ──────────────────────────────────────── */
    fn_obj_new           = (pfn_obj_new)          resolve_sym("il2cpp_object_new");
    fn_str_new           = (pfn_str_new)          resolve_sym("il2cpp_string_new");
    fn_domain_get        = (pfn_domain_get)       resolve_sym("il2cpp_domain_get");
    fn_domain_assemblies = (pfn_domain_assemblies)resolve_sym("il2cpp_domain_get_assemblies");
    fn_assembly_image    = (pfn_assembly_get_image)resolve_sym("il2cpp_assembly_get_image");
    fn_class_from_name   = (pfn_class_from_name)  resolve_sym("il2cpp_class_from_name");
    fn_class_get_type    = (pfn_class_get_type)   resolve_sym("il2cpp_class_get_type");
    fn_type_get_object   = (pfn_type_get_object)  resolve_sym("il2cpp_type_get_object");

    /* Unity / Photon / TMP engine function pointers (all RVAs resolved here) */
    fn_go_ctor_str      = (pfn_go_ctor_str)     getRealOffset(RVA_GO_CTOR_STR);
    fn_go_get_tr        = (pfn_go_get_tr)       getRealOffset(RVA_GO_GET_TRANSFORM);
    fn_go_add_comp      = (pfn_go_add_comp)     getRealOffset(RVA_GO_ADD_COMPONENT);
    fn_go_get_comp      = (pfn_go_get_comp)     getRealOffset(RVA_GO_GET_COMPONENT);
    fn_comp_get_tr      = (pfn_comp_get_tr)     getRealOffset(RVA_COMP_GET_TRANSFORM);
    fn_obj_destroy      = (pfn_obj_destroy)     getRealOffset(RVA_OBJ_DESTROY);
    fn_go_set_active    = (pfn_go_set_active)   getRealOffset(RVA_GO_SET_ACTIVE);
    fn_tr_set_parent    = (pfn_tr_set_parent)   getRealOffset(RVA_TR_SET_PARENT);
    fn_tr_set_local_pos = (pfn_tr_set_local_pos)getRealOffset(RVA_TR_SET_LOCAL_POS);
    fn_tr_set_local_scl = (pfn_tr_set_local_scl)getRealOffset(RVA_TR_SET_LOCAL_SCALE);
    fn_tr_lookat_tr     = (pfn_tr_lookat_tr)    getRealOffset(RVA_TR_LOOKAT_TR);
    fn_tr_rotate_fff    = (pfn_tr_rotate_fff)   getRealOffset(RVA_TR_ROTATE_FFF);
    fn_cam_get_main     = (pfn_cam_get_main)    getRealOffset(RVA_CAM_GET_MAIN);
    fn_get_photon_view  = (pfn_get_photon_view) getRealOffset(RVA_MBP_GET_PHOTONVIEW);
    fn_get_is_mine      = (pfn_get_is_mine)     getRealOffset(RVA_PV_GET_ISMINE);
    fn_get_is_master    = (pfn_get_is_master)   getRealOffset(RVA_PN_GET_ISMASTER);
    fn_tmp_set_text     = (pfn_tmp_set_text)    getRealOffset(RVA_TMP_SET_TEXT);
    fn_tmp_set_color    = (pfn_tmp_set_color)   getRealOffset(RVA_TMP_SET_COLOR);
    fn_tmp_set_size     = (pfn_tmp_set_size)    getRealOffset(RVA_TMP_SET_FONT_SIZE);
    fn_tmp_set_align    = (pfn_tmp_set_align)   getRealOffset(RVA_TMP_SET_ALIGNMENT);

    /* ── Il2CppClass* for GameObject ─────────────────────────────────── */
    void **ppGoClass = (void **)getRealOffset(RVA_TI_GAMEOBJECT);
    if (ppGoClass && *ppGoClass) {
        g_go_klass = *ppGoClass;
        LOGI("go_klass resolved: %p", g_go_klass);
    } else {
        LOGE("Failed to resolve GameObject TypeInfo (ppGoClass=%p)", ppGoClass);
    }

    /* ── System.Type* for TextMeshPro ────────────────────────────────── */
    void **ppTmpTextClass = (void **)getRealOffset(RVA_TI_TMP_TEXT);
    if (ppTmpTextClass && *ppTmpTextClass) {
        void *image = AOQ_FIELD(*ppTmpTextClass, IL2CPP_CLASS_IMAGE_OFF, void *);
        void *tmpKlass = NULL;

        if (image && fn_class_from_name)
            tmpKlass = fn_class_from_name(image, "TMPro", "TextMeshPro");

        if (!tmpKlass)
            tmpKlass = find_class_in_all_assemblies("TMPro", "TextMeshPro");

        if (tmpKlass && fn_class_get_type && fn_type_get_object) {
            const void *type = fn_class_get_type(tmpKlass);
            if (type) g_tmp_type_obj = fn_type_get_object(type);
        }

        if (g_tmp_type_obj)
            LOGI("tmp_type_obj resolved: %p", g_tmp_type_obj);
        else
            LOGE("Failed to get System.Type for TextMeshPro");
    } else {
        LOGE("Failed to resolve TMP_Text TypeInfo (ppTmpTextClass=%p)", ppTmpTextClass);
    }

    g_classes_done = 1;
}


/* ====================================================================
 * aoq_tmp_create
 * ==================================================================== */
void *aoq_tmp_create(const char *go_name, void *parent_transform,
                     float font_size, float lx, float ly, float lz)
{
    if (!g_init_done)  { LOGE("aoq_tmp_create: aoq_init() not called"); return NULL; }

    aoq_late_init();

    if (!g_go_klass)   { LOGE("aoq_tmp_create: g_go_klass is null");    return NULL; }
    if (!g_tmp_type_obj){ LOGE("aoq_tmp_create: g_tmp_type_obj is null"); return NULL; }

    /* 1. Allocate a new managed GameObject and call its string constructor */
    void *go = fn_obj_new(g_go_klass);
    if (!go) { LOGE("aoq_tmp_create: il2cpp_object_new returned null"); return NULL; }

    void *name_str = fn_str_new(go_name);
    fn_go_ctor_str(go, name_str);

    /* 2. Get the auto-created Transform and parent it */
    void *go_tr = fn_go_get_tr(go);
    if (parent_transform)
        fn_tr_set_parent(go_tr, parent_transform, 0 /* worldPositionStays=false */);

    /* 3. Set position and scale */
    fn_tr_set_local_scl(go_tr, 1.0f, 1.0f, 1.0f);
    fn_tr_set_local_pos(go_tr, lx, ly, lz);

    /* 4. AddComponent<TextMeshPro>() — using the non-generic Type overload */
    void *tmp_comp = fn_go_add_comp(go, g_tmp_type_obj);
    if (!tmp_comp) {
        LOGE("aoq_tmp_create: AddComponent returned null for '%s'", go_name);
        return NULL;
    }

    /* 5. Configure the TMP component */
    fn_tmp_set_size(tmp_comp, font_size);
    fn_tmp_set_align(tmp_comp, TMP_ALIGN_CENTER);

    LOGI("aoq_tmp_create: created '%s' -> tmp_comp=%p", go_name, tmp_comp);
    return tmp_comp;
}


/* ====================================================================
 * TMP setters (all null-safe)
 * ==================================================================== */
void aoq_tmp_set_text(void *tmp_comp, const char *text)
{
    if (!tmp_comp || !text || !fn_tmp_set_text || !fn_str_new) return;
    fn_tmp_set_text(tmp_comp, fn_str_new(text));
}

void aoq_tmp_set_color(void *tmp_comp, float r, float g, float b, float a)
{
    if (!tmp_comp || !fn_tmp_set_color) return;
    fn_tmp_set_color(tmp_comp, r, g, b, a);
}

void aoq_tmp_set_alignment(void *tmp_comp, int alignment)
{
    if (!tmp_comp || !fn_tmp_set_align) return;
    fn_tmp_set_align(tmp_comp, alignment);
}

void aoq_tmp_set_font_size(void *tmp_comp, float size)
{
    if (!tmp_comp || !fn_tmp_set_size) return;
    fn_tmp_set_size(tmp_comp, size);
}
