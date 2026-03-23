#ifndef MODLIST_H
#define MODLIST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Filesystem locations on device */
#define MODS_BASE_DIR     "/sdcard/Android/data/com.AoQ.AttackOnQuest/files"
#define MODS_DIR          MODS_BASE_DIR "/mods/"
#define MODS_DISABLED_DIR MODS_DIR "disabledmods/"

/* Name of this mod's own .so — never moved */
#define SELF_MOD_NAME "libaoqmodmanager.so"

#define MAX_MODS 64

typedef struct {
    char name[128];         /* filename, e.g. "liblevygrip.so" */
    int  enabled;           /* 1 = in mods/, 0 = in mods_disabled/ */
    char display_name[128]; /* human-readable name from meta.json, or same as name */
    int  builtin;           /* 1 = built-in (not a file on disk), e.g. mod manager itself */
} ModEntry;

/* Scan both mods/ and mods_disabled/ directories.
 * Fills entries[], returns count (0 on error). */
int scan_mods(ModEntry *entries, int max_entries);

/* Move mod from mods_disabled/ to mods/. Returns 0 on success, -1 on error. */
int enable_mod(const char *name);

/* Move mod from mods/ to mods_disabled/. Returns 0 on success, -1 on error.
 * Refuses to disable SELF_MOD_NAME (returns -2). */
int disable_mod(const char *name);

/* Count .so files in mods/ (excluding self). */
int count_enabled_mods(void);

/* Read DisabledModsFolder from own config and apply it.
 * Call once after config system is ready (inject time). */
void init_disabled_dir(void);

/* Change the disabled-mods subfolder: creates new dir, moves all disabled
 * .so files from the old dir to the new one, removes the old dir.
 * new_subfolder is the bare name (e.g. "disabledmods"), not a full path.
 * Returns 0 on success, -1 on error. */
int set_disabled_folder(const char *new_subfolder);

#ifdef __cplusplus
}
#endif

#endif /* MODLIST_H */
