/*
 * libaoqcore — AoQ ARMv7 Modding Utility Library (shared)
 *
 * Wraps common Unity engine, TextMeshPro, Photon and config operations so
 * mod authors don't need to manage IL2CPP internals directly.
 *
 * USAGE
 *   1. Call aoq_init() once inside your lib_main() before installing hooks.
 *   2. Call creation helpers (aoq_tmp_create etc.) from hook context only —
 *      everything lazily resolves on first use from a hook, never earlier.
 *   3. Per-frame helpers (aoq_tmp_set_text, aoq_billboard, ...) are null-safe.
 *
 * PORTING FROM BEPINEX / HARMONY
 *   BepInEx concept            →  libaoqcore equivalent
 *   ─────────────────────────────────────────────────────
 *   new GameObject("name")     →  (internal to aoq_tmp_create)
 *   AddComponent<TextMeshPro>  →  (internal to aoq_tmp_create)
 *   tmp.text = "6"             →  aoq_tmp_set_text(tmp, "6")
 *   tmp.color = Color.green    →  aoq_tmp_set_color(tmp, 0,1,0,1)
 *   t.LookAt(Camera.main...)   →  aoq_billboard(transform, aoq_get_transform(aoq_camera_main()))
 *   photonView.IsMine          →  aoq_is_mine(self)
 *   obj == null (destroyed)    →  !aoq_alive(obj)
 *   Object.Destroy(obj)        →  aoq_destroy(obj)
 *   GetComponent<T>()          →  aoq_go_get_component_named(go, "", "T")
 *
 * All addresses are relative to libil2cpp.so (RVAs from Il2CppDumper on
 * AoQ 0.5.0).  getRealOffset() in shared/utils/utils.c adds the ASLR base.
 *
 * CALLING CONVENTION NOTE (ARMv7 softfp)
 *   Floats are passed in integer registers r0-r3.  A Vector3 (3 floats,
 *   12 bytes) is passed as 3 consecutive word arguments.  Functions that
 *   RETURN a Vector3 or larger struct use a hidden output pointer in r0,
 *   shifting 'self' to r1 — we avoid those returns by using pointer-based
 *   overloads (e.g. LookAt(Transform) instead of LookAt(Vector3)).
 */

#ifndef AOQ_H
#define AOQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../../modmanager/modconfig.h"

/* ====================================================================
 * COMMON TYPES
 * ==================================================================== */

/* Unity Vector3 — passed as 3 consecutive float arguments on ARMv7 softfp */
typedef struct { float x, y, z; } AoqVec3;

/* Unity Color — passed as 4 consecutive float arguments (4th spills to stack) */
typedef struct { float r, g, b, a; } AoqColor;

/* TextAlignmentOptions values from TMP (TMPro.TextAlignmentOptions enum) */
#define TMP_ALIGN_TOP_LEFT  257
#define TMP_ALIGN_TOP       258
#define TMP_ALIGN_CENTER    514   /* TextAlignmentOptions.Center */
#define TMP_ALIGN_BOTTOM    1028

/* Common color helpers */
#define AOQ_COLOR_GREEN   0.0f, 1.0f, 0.0f, 1.0f
#define AOQ_COLOR_YELLOW  1.0f, 0.92f, 0.016f, 1.0f
#define AOQ_COLOR_RED     1.0f, 0.0f, 0.0f, 1.0f
#define AOQ_COLOR_WHITE   1.0f, 1.0f, 1.0f, 1.0f

/* Read a field straight out of an il2cpp object by byte offset (from dump.cs).
 * Example: void *rightSword = AOQ_FIELD(self, 0x10, void *); */
#define AOQ_FIELD(obj, off, type)  (*(type *)((uint8_t *)(obj) + (off)))


/* ====================================================================
 * INITIALISATION
 *
 * Marks init requested; all real resolution is deferred until the first
 * helper call from a hook (when Unity + il2cpp are fully up).
 * Safe to call multiple times.
 * ==================================================================== */
void aoq_init(void);


/* ====================================================================
 * OBJECT LIFETIME
 * ==================================================================== */

/* 1 while the underlying native Unity object still exists, 0 after Unity
 * has destroyed it (or if obj is NULL).  This is the C equivalent of the
 * C# "obj == null" check on a destroyed UnityEngine.Object.
 * NEVER call engine methods on a pointer that fails this check. */
int aoq_alive(void *unity_obj);

/* UnityEngine.Object.Destroy(obj). Null-safe. */
void aoq_destroy(void *unity_obj);

/* GameObject.SetActive(value). Null-safe, and skips destroyed objects. */
void aoq_go_set_active(void *go, int value);


/* ====================================================================
 * PHOTON (PUN2) HELPERS
 * ==================================================================== */

/* MonoBehaviourPun.photonView for any component deriving MonoBehaviourPun
 * (NetworkBlades, NetworkWeaponSwap, TitanAI_Multi, ...). NULL-safe. */
void *aoq_photon_view(void *mono_behaviour_pun);

/* photonView.IsMine — 1 when this instance belongs to the local player.
 * Returns 0 when the view can't be resolved (safe default: treat objects
 * we can't identify as NOT ours so we never touch other players' stuff). */
int aoq_is_mine(void *mono_behaviour_pun);

/* PhotonNetwork.IsMasterClient (0 if unresolved). */
int aoq_is_master(void);


/* ====================================================================
 * CLASS / COMPONENT LOOKUP
 * ==================================================================== */

/* Find an Il2CppClass* by namespace + name across all loaded assemblies.
 * ns may be "" for the global namespace. Returns NULL if not found. */
void *aoq_find_class(const char *ns, const char *name);

/* System.Type object for a class (needed by GetComponent/AddComponent). */
void *aoq_type_object(void *klass);

/* GameObject.GetComponent(Type). */
void *aoq_go_get_component(void *go, void *type_obj);

/* Convenience: GetComponent by class name. Does a full class lookup each
 * call — prefer resolving the Type once yourself in hot paths. */
void *aoq_go_get_component_named(void *go, const char *ns, const char *name);

/* static_fields block of a class whose TypeInfo global lives at rva
 * (from Il2CppDumper script.json "<Class>_TypeInfo"). NULL until the
 * class has been initialised by il2cpp. */
void *aoq_static_fields(long typeinfo_rva);


/* ====================================================================
 * CONFIG CACHE
 *
 * Wraps modconfig.h with an in-memory cache so hooks never do disk I/O
 * per frame. Call aoq_cfg_refresh() at natural boundaries (Start hooks,
 * button presses); read values with the typed getters anywhere.
 * On a failed refresh the previous values are kept.
 * ==================================================================== */

typedef struct {
    ModConfig cfg;
    int       loaded;
} AoqCfgCache;

/* Reload from disk. Returns 0 on success. Keeps old values on failure. */
int aoq_cfg_refresh(AoqCfgCache *c, const char *so_name);

double aoq_cfg_num (AoqCfgCache *c, const char *key, double fallback);
float  aoq_cfg_flt (AoqCfgCache *c, const char *key, float  fallback);
int    aoq_cfg_int (AoqCfgCache *c, const char *key, int    fallback);
int    aoq_cfg_bool(AoqCfgCache *c, const char *key, int    fallback);


/* ====================================================================
 * INPUT: TAP / HOLD DETECTOR
 *
 * Feed it the button's GetDown + Get states once per frame; it returns
 * what happened this frame.
 * ==================================================================== */

typedef struct { float press_start; int pressing; } AoqTapHold;

#define AOQ_INPUT_NONE 0
#define AOQ_INPUT_TAP  1   /* released before hold_secs elapsed */
#define AOQ_INPUT_HOLD 2   /* held longer than hold_secs (fires once) */

int aoq_tap_hold(AoqTapHold *st, int down_this_frame, int held_this_frame,
                 float now, float hold_secs);


/* ====================================================================
 * UNITY CAMERA
 * ==================================================================== */

/* Returns Camera.main (equivalent to Camera.get_main() in C#).
 * Returns NULL if no MainCamera is tagged in the scene. */
void *aoq_camera_main(void);


/* ====================================================================
 * UNITY TRANSFORM HELPERS
 * ==================================================================== */

/* Get the Transform from a Component (MonoBehaviour, AudioSource, etc.).
 * Equivalent to component.transform in C#. */
void *aoq_get_transform(void *component);

/* Get the Transform from a GameObject.
 * Equivalent to go.transform in C#. */
void *aoq_get_transform_go(void *go);

/* Set the world-space parent of a transform.
 * worldPositionStays: 1 = keep world position, 0 = use local position. */
void aoq_set_parent(void *transform, void *parent, int worldPositionStays);

/* Equivalent to transform.localPosition = new Vector3(x, y, z) */
void aoq_set_local_position(void *transform, float x, float y, float z);

/* Equivalent to transform.localScale = new Vector3(x, y, z) */
void aoq_set_local_scale(void *transform, float x, float y, float z);

/* Make the transform face target_transform, then flip 180° on Y so the
 * front face points toward the target (standard billboard pattern). */
void aoq_billboard(void *transform, void *target_transform);


/* ====================================================================
 * TEXTMESHPRO HELPERS
 * ==================================================================== */

/* Create a new GameObject named go_name with a TextMeshPro component,
 * parented to parent_transform at local offset (lx, ly, lz).
 * Returns the TextMeshPro component, or NULL on failure. */
void *aoq_tmp_create(const char *go_name, void *parent_transform,
                     float font_size, float lx, float ly, float lz);

/* Equivalent to tmp.text = text; (null-safe) */
void aoq_tmp_set_text(void *tmp_comp, const char *text);

/* Equivalent to tmp.color = new Color(r,g,b,a); (null-safe) */
void aoq_tmp_set_color(void *tmp_comp, float r, float g, float b, float a);

/* Use TMP_ALIGN_CENTER or another TMP_ALIGN_* constant above. */
void aoq_tmp_set_alignment(void *tmp_comp, int alignment);

void aoq_tmp_set_font_size(void *tmp_comp, float size);

#ifdef __cplusplus
}
#endif

#endif /* AOQ_H */
