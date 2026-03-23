#pragma once
#include "il2cpp.h"

/* Is p a plausibly valid heap pointer? */
int   valid_ptr(void *p);

/* Check whether a Unity object is still alive.
 * In IL2CPP 32-bit, m_cachedPtr (the native C++ backing pointer) lives at
 * offset 0x8 of the C# wrapper.  Object.Destroy() zeroes it — so a non-zero
 * value means the native object still exists. */
static inline int unity_alive(void *obj)
{
    if (!valid_ptr(obj)) return 0;
    void *cached = *(void **)((uint8_t *)obj + 0x8);
    return valid_ptr(cached);
}

/* IL2CPP array helpers (Il2CppArray header = 16 bytes on 32-bit) */
int   arr_len(void *arr);
void *arr_get(void *arr, int i);

/* C# managed string helpers */
void  cs_str_to_c(void *cs, char *out, int max_len);
int   cs_str_eq(void *cs, const char *cstr);

/* Create a new managed string from a C string */
void *make_str(const char *s);

/* IL2CPP type object helper (looks up class across all assemblies) */
void *make_type_obj(const char *ns, const char *name);

/* Transform.Find with a C string name (supports '/' path separators) */
void *tr_find(void *tr, const char *name);

/* Recursive depth-first search by exact GO name — use when path is unknown */
void *tr_find_deep(void *tr, const char *name);

/* Walk up transform parent chain looking for a GO with the given name */
void *find_ancestor_named(void *start_tr, const char *name);

/* Get the first TextMeshProUGUI in a GameObject's hierarchy */
void *get_tmp_in_go(void *go);

/* Set / get text on a TMP_Text component */
void  tmp_set(void *tmp_comp, const char *s);
void  tmp_get(void *tmp_comp, char *buf, int max_len);
