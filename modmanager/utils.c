#include <string.h>
#include <stdint.h>
#include "utils.h"
#include "il2cpp.h"

int valid_ptr(void *p) { return p && (uintptr_t)p > 0x10000u; }

/* Il2CppArray header is 16 bytes on 32-bit ARM */
int arr_len(void *arr)
{
    if (!arr) return 0;
    return *(int32_t *)((uint8_t *)arr + 12);
}

void *arr_get(void *arr, int i)
{
    if (!arr) return NULL;
    return ((void **)((uint8_t *)arr + 16))[i];
}

/* C# managed string layout: [klass 4][monitor 4][length 4][chars... (UTF-16LE)] */
void cs_str_to_c(void *cs, char *out, int max_len)
{
    if (!cs || !out || max_len <= 0) { if (out && max_len > 0) out[0] = '\0'; return; }
    unsigned int len = *(unsigned int *)((uint8_t *)cs + 8);
    const char  *raw = (const char *)((uint8_t *)cs + 12);
    unsigned int copy = (len < (unsigned)max_len - 1) ? len : (unsigned)max_len - 1;
    for (unsigned i = 0; i < copy; i++) out[i] = raw[i * 2];
    out[copy] = '\0';
}

int cs_str_eq(void *cs, const char *cstr)
{
    char buf[256];
    cs_str_to_c(cs, buf, sizeof(buf));
    return strcmp(buf, cstr) == 0;
}

void *make_str(const char *s)
{
    if (!fn_str_new || !s) return NULL;
    return fn_str_new(s);
}

void *tr_find(void *tr, const char *name)
{
    if (!tr || !fn_tr_find || !fn_str_new) return NULL;
    return fn_tr_find(tr, make_str(name));
}

void *find_ancestor_named(void *start_tr, const char *name)
{
    void *tr = fn_tr_get_parent(start_tr);
    for (int d = 0; tr && d < 20; d++) {
        void *name_cs = fn_obj_get_name(fn_comp_get_go(tr));
        if (cs_str_eq(name_cs, name)) return tr;
        tr = fn_tr_get_parent(tr);
    }
    return NULL;
}

void *get_tmp_in_go(void *go)
{
    if (!go || !fn_get_comp_ch || !g_tmpugui_type_obj) return NULL;
    void *tr = fn_go_get_tr(go);
    return fn_get_comp_ch(tr, g_tmpugui_type_obj, 1);
}

void tmp_set(void *tmp_comp, const char *s)
{
    if (!tmp_comp || !fn_tmp_set_text || !fn_str_new) return;
    fn_tmp_set_text(tmp_comp, make_str(s));
}

void tmp_get(void *tmp_comp, char *buf, int max_len)
{
    if (!tmp_comp || !fn_tmp_get_text) { if (buf) buf[0] = '\0'; return; }
    cs_str_to_c(fn_tmp_get_text(tmp_comp), buf, max_len);
}

void *tr_find_deep(void *tr, const char *name)
{
    if (!tr || !fn_tr_get_cc || !fn_tr_get_child || !fn_comp_get_go || !fn_obj_get_name)
        return NULL;
    int cc = fn_tr_get_cc(tr);
    for (int i = 0; i < cc; i++) {
        void *child = fn_tr_get_child(tr, i);
        if (!child) continue;
        void *child_go = fn_comp_get_go(child);
        if (child_go && cs_str_eq(fn_obj_get_name(child_go), name))
            return child;
        void *found = tr_find_deep(child, name);
        if (found) return found;
    }
    return NULL;
}
