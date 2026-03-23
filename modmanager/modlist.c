#include "modlist.h"
#include "modconfig.h"
#include <android/log.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define TAG "AoQModManager"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/* Runtime disabled-mods directory path — updated by set_disabled_folder() */
static char g_disabled_dir[512] = MODS_DIR "disabledmods/";

/* Load the display_name field from <cfg_name>.meta.json.
 * Uses simple string search — no full JSON parser needed.
 * Falls back to so_name if not found. */
static void load_meta_display_name(const char *so_name, char *out, int max)
{
    char cfg_name[128];
    so_name_to_cfg_name(so_name, cfg_name, sizeof(cfg_name));

    char path[512];
    snprintf(path, sizeof(path), "%s%s.meta.json", MODCONFIGS_DIR, cfg_name);

    FILE *f = fopen(path, "r");
    if (!f) {
        strncpy(out, so_name, max - 1);
        out[max - 1] = '\0';
        return;
    }

    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';

    /* Look for "display_name": "..." */
    const char *key = "\"display_name\"";
    char *p = strstr(buf, key);
    if (p) {
        p += strlen(key);
        /* skip whitespace and colon */
        while (*p == ' ' || *p == '\t' || *p == ':') p++;
        if (*p == '"') {
            p++; /* skip opening quote */
            int i = 0;
            while (*p && *p != '"' && i < max - 1)
                out[i++] = *p++;
            out[i] = '\0';
            if (i > 0) return;
        }
    }

    /* Fallback */
    strncpy(out, so_name, max - 1);
    out[max - 1] = '\0';
}

static int ends_with_so(const char *name)
{
    size_t len = strlen(name);
    return len > 3 && strcmp(name + len - 3, ".so") == 0;
}

int scan_mods(ModEntry *entries, int max_entries)
{
    int count = 0;
    DIR *d;
    struct dirent *ent;

    /* Always add the mod manager as a built-in first entry */
    if (max_entries > 0) {
        strncpy(entries[0].name, SELF_MOD_NAME, sizeof(entries[0].name) - 1);
        entries[0].name[sizeof(entries[0].name) - 1] = '\0';
        entries[0].enabled = 1;
        entries[0].builtin = 1;
        load_meta_display_name(entries[0].name, entries[0].display_name, sizeof(entries[0].display_name));
        count = 1;
    }

    /* Scan enabled mods */
    d = opendir(MODS_DIR);
    if (d) {
        while ((ent = readdir(d)) != NULL && count < max_entries) {
            if (!ends_with_so(ent->d_name)) continue;
            /* Skip self — already added as builtin above */
            if (strcmp(ent->d_name, SELF_MOD_NAME) == 0) continue;
            strncpy(entries[count].name, ent->d_name, sizeof(entries[count].name) - 1);
            entries[count].name[sizeof(entries[count].name) - 1] = '\0';
            entries[count].enabled = 1;
            entries[count].builtin = 0;
            load_meta_display_name(entries[count].name, entries[count].display_name, sizeof(entries[count].display_name));
            count++;
        }
        closedir(d);
    } else {
        LOGE("scan_mods: can't open %s: %s", MODS_DIR, strerror(errno));
    }

    /* Scan disabled mods */
    d = opendir(g_disabled_dir);
    if (d) {
        while ((ent = readdir(d)) != NULL && count < max_entries) {
            if (!ends_with_so(ent->d_name)) continue;
            strncpy(entries[count].name, ent->d_name, sizeof(entries[count].name) - 1);
            entries[count].name[sizeof(entries[count].name) - 1] = '\0';
            entries[count].enabled = 0;
            entries[count].builtin = 0;
            load_meta_display_name(entries[count].name, entries[count].display_name, sizeof(entries[count].display_name));
            count++;
        }
        closedir(d);
    } else {
        LOGI("scan_mods: no disabled dir (normal if none disabled)");
    }

    return count;
}

int enable_mod(const char *name)
{
    if (!name || !*name) return -1;

    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s%s", g_disabled_dir, name);
    snprintf(dst, sizeof(dst), "%s%s", MODS_DIR, name);

    /* Ensure mods/ dir exists */
    mkdir(MODS_DIR, 0755);

    if (rename(src, dst) != 0) {
        LOGE("enable_mod: rename %s -> %s failed: %s", src, dst, strerror(errno));
        return -1;
    }
    LOGI("enable_mod: enabled %s", name);
    return 0;
}

int disable_mod(const char *name)
{
    if (!name || !*name) return -1;
    if (strcmp(name, SELF_MOD_NAME) == 0) {
        LOGI("disable_mod: refusing to disable self (%s)", name);
        return -2;
    }

    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s%s", MODS_DIR, name);
    snprintf(dst, sizeof(dst), "%s%s", g_disabled_dir, name);

    /* Ensure mods_disabled/ dir exists */
    mkdir(g_disabled_dir, 0755);

    if (rename(src, dst) != 0) {
        LOGE("disable_mod: rename %s -> %s failed: %s", src, dst, strerror(errno));
        return -1;
    }
    LOGI("disable_mod: disabled %s", name);
    return 0;
}

int count_enabled_mods(void)
{
    int count = 0;
    DIR *d = opendir(MODS_DIR);
    if (!d) return 0;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ends_with_so(ent->d_name))
            count++;
    }
    closedir(d);
    return count;
}

int set_disabled_folder(const char *new_subfolder)
{
    if (!new_subfolder || !*new_subfolder) return -1;

    char new_path[512];
    snprintf(new_path, sizeof(new_path), "%s%s/", MODS_DIR, new_subfolder);

    if (strcmp(new_path, g_disabled_dir) == 0) return 0;  /* no change */

    LOGI("set_disabled_folder: migrating '%s' -> '%s'", g_disabled_dir, new_path);

    /* Create the new directory */
    mkdir(new_path, 0755);

    /* Move all disabled .so files from old dir to new dir */
    DIR *d = opendir(g_disabled_dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (!ends_with_so(ent->d_name)) continue;
            char src[512], dst[512];
            snprintf(src, sizeof(src), "%s%s", g_disabled_dir, ent->d_name);
            snprintf(dst, sizeof(dst), "%s%s", new_path,       ent->d_name);
            if (rename(src, dst) != 0)
                LOGE("set_disabled_folder: move '%s' failed: %s", ent->d_name, strerror(errno));
            else
                LOGI("set_disabled_folder: moved '%s'", ent->d_name);
        }
        closedir(d);
        /* Remove old dir — only succeeds if empty after migration */
        if (rmdir(g_disabled_dir) != 0)
            LOGI("set_disabled_folder: rmdir '%s': %s", g_disabled_dir, strerror(errno));
    } else {
        LOGI("set_disabled_folder: old dir '%s' not found, nothing to migrate", g_disabled_dir);
    }

    strncpy(g_disabled_dir, new_path, sizeof(g_disabled_dir) - 1);
    g_disabled_dir[sizeof(g_disabled_dir) - 1] = '\0';
    LOGI("set_disabled_folder: active dir now '%s'", g_disabled_dir);
    return 0;
}

void init_disabled_dir(void)
{
    ModConfig cfg;
    if (load_config(SELF_MOD_NAME, &cfg) != 0) return;
    ModCfgEntry *e = get_entry(&cfg, "DisabledModsFolder");
    if (!e || !e->value_str[0]) return;
    set_disabled_folder(e->value_str);
}
