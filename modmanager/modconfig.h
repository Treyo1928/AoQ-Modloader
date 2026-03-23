#ifndef MODCONFIG_H
#define MODCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"

/* Config files live at: MODCONFIGS_DIR/<modname>.json
 * where <modname> is the .so filename without "lib" prefix and ".so" suffix.
 * e.g. "liblevygrip.so" → "levygrip.json"
 *
 * File format:
 * {
 *   "entries": [
 *     { "key": "gripOffset", "type": "float",  "value": 82.0, "description": "..." },
 *     { "key": "enabled",    "type": "bool",   "value": true,  "description": "..." },
 *     { "key": "label",      "type": "string", "value": "hi",  "description": "..." }
 *   ]
 * }
 */
#define MODCONFIGS_DIR "/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs/"

#define MODCFG_TYPE_FLOAT  "float"
#define MODCFG_TYPE_INT    "int"
#define MODCFG_TYPE_BOOL   "bool"
#define MODCFG_TYPE_STRING "string"

#define MAX_CFG_ENTRIES 32
#define MAX_CFG_KEY_LEN  64
#define MAX_CFG_TYPE_LEN 16
#define MAX_CFG_DESC_LEN 128
#define MAX_CFG_STR_LEN  256

typedef struct {
    char   key[MAX_CFG_KEY_LEN];
    char   type[MAX_CFG_TYPE_LEN];
    double value_num;    /* for float / int / bool */
    char   value_str[MAX_CFG_STR_LEN]; /* for string (also holds float/bool/int as text) */
    char   description[MAX_CFG_DESC_LEN];
} ModCfgEntry;

typedef struct {
    char        mod_name[128];   /* e.g. "levygrip" */
    ModCfgEntry entries[MAX_CFG_ENTRIES];
    int         count;
} ModConfig;

/* Convert .so filename to config basename ("liblevygrip.so" → "levygrip") */
void so_name_to_cfg_name(const char *so_name, char *cfg_name, int max_len);

/* Load config from disk into mc. Returns 0 on success, -1 if file missing/invalid. */
int load_config(const char *so_name, ModConfig *mc);

/* Write mc back to disk. Returns 0 on success. */
int save_config(const ModConfig *mc);

/* Get entry by key. Returns pointer into mc->entries[], or NULL. */
ModCfgEntry *get_entry(ModConfig *mc, const char *key);

/* Update entry value. For float/int/bool: new_num_value used.
 * For string: new_str_value used. Call save_config() after. */
void set_entry_value(ModCfgEntry *entry, double new_num_value, const char *new_str_value);

/* Check if a config file exists for the given .so name */
int config_exists(const char *so_name);

#ifdef __cplusplus
}
#endif

#endif /* MODCONFIG_H */
