#include "modconfig.h"
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define TAG "AoQModManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

void so_name_to_cfg_name(const char *so_name, char *cfg_name, int max_len)
{
    /* "liblevygrip.so" → "levygrip" */
    const char *start = so_name;
    if (strncmp(start, "lib", 3) == 0) start += 3;

    strncpy(cfg_name, start, max_len - 1);
    cfg_name[max_len - 1] = '\0';

    /* Strip .so suffix */
    size_t len = strlen(cfg_name);
    if (len > 3 && strcmp(cfg_name + len - 3, ".so") == 0)
        cfg_name[len - 3] = '\0';
}

int config_exists(const char *so_name)
{
    char cfg_name[128];
    so_name_to_cfg_name(so_name, cfg_name, sizeof(cfg_name));

    char path[512];
    snprintf(path, sizeof(path), "%s%s.json", MODCONFIGS_DIR, cfg_name);

    struct stat st;
    return stat(path, &st) == 0;
}

int load_config(const char *so_name, ModConfig *mc)
{
    if (!so_name || !mc) return -1;
    memset(mc, 0, sizeof(*mc));

    so_name_to_cfg_name(so_name, mc->mod_name, sizeof(mc->mod_name));

    char path[512];
    snprintf(path, sizeof(path), "%s%s.json", MODCONFIGS_DIR, mc->mod_name);

    FILE *f = fopen(path, "r");
    if (!f) {
        LOGI("load_config: no config at %s", path);
        return -1;
    }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return -1; }

    char *buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        LOGE("load_config: JSON parse error for %s", path);
        return -1;
    }

    cJSON *entries_arr = cJSON_GetObjectItem(root, "entries");
    if (!entries_arr || entries_arr->type != cJSON_Array) {
        LOGE("load_config: no 'entries' array in %s", path);
        cJSON_Delete(root);
        return -1;
    }

    int n = cJSON_GetArraySize(entries_arr);
    if (n > MAX_CFG_ENTRIES) n = MAX_CFG_ENTRIES;

    for (int i = 0; i < n; i++) {
        cJSON *entry = cJSON_GetArrayItem(entries_arr, i);
        if (!entry || entry->type != cJSON_Object) continue;

        ModCfgEntry *e = &mc->entries[mc->count];

        cJSON *key  = cJSON_GetObjectItem(entry, "key");
        cJSON *type = cJSON_GetObjectItem(entry, "type");
        cJSON *val  = cJSON_GetObjectItem(entry, "value");
        cJSON *desc = cJSON_GetObjectItem(entry, "description");

        if (!key || key->type != cJSON_String) continue;

        strncpy(e->key, key->valuestring, MAX_CFG_KEY_LEN - 1);

        if (type && type->type == cJSON_String)
            strncpy(e->type, type->valuestring, MAX_CFG_TYPE_LEN - 1);
        else
            strncpy(e->type, MODCFG_TYPE_STRING, MAX_CFG_TYPE_LEN - 1);

        if (desc && desc->type == cJSON_String)
            strncpy(e->description, desc->valuestring, MAX_CFG_DESC_LEN - 1);

        /* Parse value based on type */
        if (strcmp(e->type, MODCFG_TYPE_BOOL) == 0) {
            if (val) {
                e->value_num = (val->type == cJSON_True) ? 1.0 : 0.0;
                snprintf(e->value_str, MAX_CFG_STR_LEN, "%s",
                         e->value_num ? "true" : "false");
            }
        } else if (strcmp(e->type, MODCFG_TYPE_FLOAT) == 0 ||
                   strcmp(e->type, MODCFG_TYPE_INT)   == 0) {
            if (val && val->type == cJSON_Number) {
                e->value_num = val->valuedouble;
                snprintf(e->value_str, MAX_CFG_STR_LEN, "%g", e->value_num);
            }
        } else {
            /* string */
            if (val && val->type == cJSON_String) {
                strncpy(e->value_str, val->valuestring, MAX_CFG_STR_LEN - 1);
                e->value_num = 0;
            }
        }

        mc->count++;
    }

    cJSON_Delete(root);
    LOGI("load_config: loaded %d entries for %s", mc->count, mc->mod_name);
    return 0;
}

int save_config(const ModConfig *mc)
{
    if (!mc) return -1;

    /* Ensure modconfigs dir exists */
    mkdir(MODCONFIGS_DIR, 0755);

    char path[512];
    snprintf(path, sizeof(path), "%s%s.json", MODCONFIGS_DIR, mc->mod_name);

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON *entries_arr = cJSON_CreateArray();
    if (!entries_arr) { cJSON_Delete(root); return -1; }
    cJSON_AddItemToObject(root, "entries", entries_arr);

    for (int i = 0; i < mc->count; i++) {
        const ModCfgEntry *e = &mc->entries[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry) continue;

        cJSON_AddStringToObject(entry, "key",         e->key);
        cJSON_AddStringToObject(entry, "type",        e->type);
        cJSON_AddStringToObject(entry, "description", e->description);

        if (strcmp(e->type, MODCFG_TYPE_BOOL) == 0) {
            cJSON_AddItemToObject(entry, "value", cJSON_CreateBool((int)e->value_num));
        } else if (strcmp(e->type, MODCFG_TYPE_FLOAT) == 0 ||
                   strcmp(e->type, MODCFG_TYPE_INT)   == 0) {
            cJSON_AddNumberToObject(entry, "value", e->value_num);
        } else {
            cJSON_AddStringToObject(entry, "value", e->value_str);
        }

        cJSON_AddItemToArray(entries_arr, entry);
    }

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return -1;

    FILE *f = fopen(path, "w");
    if (!f) {
        LOGE("save_config: can't open %s for writing: %s", path, strerror(errno));
        free(text);
        return -1;
    }
    fputs(text, f);
    fclose(f);
    free(text);

    LOGI("save_config: saved %s", path);
    return 0;
}

ModCfgEntry *get_entry(ModConfig *mc, const char *key)
{
    if (!mc || !key) return NULL;
    for (int i = 0; i < mc->count; i++) {
        if (strcmp(mc->entries[i].key, key) == 0)
            return &mc->entries[i];
    }
    return NULL;
}

void set_entry_value(ModCfgEntry *entry, double new_num_value, const char *new_str_value)
{
    if (!entry) return;
    if (strcmp(entry->type, MODCFG_TYPE_STRING) == 0) {
        if (new_str_value)
            strncpy(entry->value_str, new_str_value, MAX_CFG_STR_LEN - 1);
    } else {
        entry->value_num = new_num_value;
        if (strcmp(entry->type, MODCFG_TYPE_BOOL) == 0)
            snprintf(entry->value_str, MAX_CFG_STR_LEN, "%s",
                     new_num_value ? "true" : "false");
        else
            snprintf(entry->value_str, MAX_CFG_STR_LEN, "%g", new_num_value);
    }
}
