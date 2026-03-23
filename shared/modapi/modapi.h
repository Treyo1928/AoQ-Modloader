#pragma once
/*
 * AoQ Mod Manager API — include this in your mod to integrate with the mod manager.
 *
 * Usage in your mod's __attribute__((constructor)):
 *
 *   #include "../../shared/modapi/modapi.h"
 *
 *   __attribute__((constructor)) void lib_main() {
 *       // Register display metadata (always call this)
 *       aoqmm_register("libmymod.so", "My Mod Name", "1.0.0", "AuthorName",
 *                      "Short description of what the mod does.");
 *
 *       // Declare config with defaults (only written if config doesn't exist yet)
 *       aoqmm_ensure_config("libmymod.so",
 *           "{\n"
 *           "  \"entries\": [\n"
 *           "    {\"key\":\"EnableFeature\",\"type\":\"bool\",\"value\":true,"
 *               "\"description\":\"Enables the main feature.\"},\n"
 *           "    {\"key\":\"Strength\",\"type\":\"float\",\"value\":1.0,"
 *               "\"description\":\"Strength multiplier (0.0 - 2.0).\"}\n"
 *           "  ]\n"
 *           "}\n"
 *       );
 *
 *       // ... rest of mod setup
 *   }
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define AOQMM_FILES_DIR "/sdcard/DCIM/AoQMods/"
#define AOQMM_CFG_DIR   AOQMM_FILES_DIR "modconfigs/"

/* Internal: convert "libfoo.so" -> "foo" */
static inline void _aoqmm_so_to_name(const char *so, char *out, int max)
{
    const char *s = so;
    if (strncmp(s, "lib", 3) == 0) s += 3;
    strncpy(out, s, max - 1);
    out[max - 1] = '\0';
    int len = (int)strlen(out);
    if (len > 3 && strcmp(out + len - 3, ".so") == 0)
        out[len - 3] = '\0';
}

/*
 * aoqmm_register — register display metadata for your mod.
 * Writes modconfigs/<name>.meta.json. Called every load (overwrites meta to
 * stay current). The mod manager uses this to show a human-readable name,
 * version, and author instead of the raw .so filename.
 */
static inline void aoqmm_register(const char *so_file, const char *display_name,
                                   const char *version,  const char *author,
                                   const char *description)
{
    mkdir(AOQMM_CFG_DIR, 0755);
    char name[64];
    _aoqmm_so_to_name(so_file, name, sizeof(name));
    char path[512];
    snprintf(path, sizeof(path), "%s%s.meta.json", AOQMM_CFG_DIR, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f,
        "{\n"
        "  \"display_name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"author\": \"%s\",\n"
        "  \"description\": \"%s\"\n"
        "}\n",
        display_name  ? display_name  : so_file,
        version       ? version       : "unknown",
        author        ? author        : "unknown",
        description   ? description   : "");
    fclose(f);
}

/*
 * aoqmm_ensure_config — write a default config JSON for your mod.
 * Only writes if the config file does not already exist, so user edits
 * are preserved across reloads. json_content must be a valid config JSON
 * in the format used by the mod manager (see modconfig.h for the schema).
 */
static inline void aoqmm_ensure_config(const char *so_file, const char *json_content)
{
    mkdir(AOQMM_CFG_DIR, 0755);
    char name[64];
    _aoqmm_so_to_name(so_file, name, sizeof(name));
    char path[512];
    snprintf(path, sizeof(path), "%s%s.json", AOQMM_CFG_DIR, name);
    struct stat st;
    if (stat(path, &st) == 0) return;  /* already exists — preserve user edits */
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(json_content, f);
    fclose(f);
}
