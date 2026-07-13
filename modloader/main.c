#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "../shared/inline-hook/inlineHook.h"
#include "../shared/utils/utils.h"

#define FILES_DIR     "/sdcard/DCIM/AoQMods"
#define MOD_PATH      FILES_DIR "/mods/"
#define MOD_TEMP_DIR  "/data/data/com.AoQ.AttackOnQuest/cache/mods/"

static void setup_directories(void)
{
    /* Create all on-device directories the framework needs.
     * mkdir is a no-op if the directory already exists. */
    mkdir(FILES_DIR,                        0755);
    mkdir(FILES_DIR "/mods",                0755);
    mkdir(FILES_DIR "/mods/disabledmods",   0755);
    mkdir(FILES_DIR "/modconfigs",          0755);
    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Directories ready.");
}

/* Returns 1 once mods have been loaded (or the mods dir was reachable and
 * simply empty), 0 if external storage isn't accessible yet so the caller
 * should retry on a later frame.
 *
 * WHY THIS IS DEFERRED: on a fresh install the user grants the storage
 * permission a few seconds INTO the first session (via the runtime prompt).
 * This function used to run from the native constructor (System.loadLibrary
 * time) — far too early, when /sdcard is still "Permission denied", so zero
 * mods loaded and no mod ever ran its constructor. We now call it every frame
 * from a Unity hook until storage becomes reachable, then load exactly once. */
int load_mods(void)
{
    static int s_loaded = 0;
    if (s_loaded) return 1;

    /* cache/ itself may not exist yet on a fresh install — mkdir doesn't
     * create parents, so make both levels (internal storage, always allowed) */
    mkdir("/data/data/com.AoQ.AttackOnQuest/cache", 0771);
    mkdir(MOD_TEMP_DIR, 0755);

    /* (Re)create the on-sdcard dirs now that storage may be reachable. */
    setup_directories();

    struct dirent **file_list;
    errno = 0;
    int no_files = scandir(MOD_PATH, &file_list, NULL, alphasort);
    if (no_files < 0 && (errno == EACCES || errno == EPERM)) {
        /* Storage permission not active yet — stay silent and let the caller
         * retry next frame (logging every frame would spam logcat). */
        return 0;
    }

    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Loading mods!");
    __android_log_print(ANDROID_LOG_INFO, "QuestHook", "scandir(%s) = %d  errno=%d (%s)",
                        MOD_PATH, no_files, errno, strerror(errno));
    s_loaded = 1;

    // Goes through all files in the mods folder alphabetically
    for (int i = 0; i < no_files; i++)
    {
        // Only attempts to load .so files
        if (strlen(file_list[i]->d_name) > 3 && !strcmp(file_list[i]->d_name + strlen(file_list[i]->d_name) - 3, ".so"))
        {
            char full_path[PATH_MAX] = MOD_PATH;
            strcat(full_path, file_list[i]->d_name);

            __android_log_print(ANDROID_LOG_INFO, "QuestHook", "Loading mod: %s", full_path);

            // Get filesize of mod
            int infile = open(full_path, O_RDONLY);
            if (infile < 0) {
                __android_log_print(ANDROID_LOG_ERROR, "QuestHook",
                                    "Can't open %s: %s", full_path, strerror(errno));
                free(file_list[i]);
                continue;
            }
            off_t filesize = lseek(infile, 0, SEEK_END);
            lseek(infile, 0, SEEK_SET);

            // Per-mod temp name so crash backtraces name the real mod
            // instead of every mod appearing as "curmod.so"
            char temp_path[PATH_MAX] = MOD_TEMP_DIR;
            strcat(temp_path, file_list[i]->d_name);

            // Unlink old file
            unlink(temp_path);

            // Creates temporary copy (we can't execute stuff in /sdcard so we need to copy it over)
            int outfile = open(temp_path, O_CREAT | O_WRONLY, 0644);
            sendfile(outfile, infile, 0, filesize);
            close(infile);
            close(outfile);

            // Mark copy as executable
            chmod(temp_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);
            // and load it
            if (!dlopen(temp_path, RTLD_NOW))
                __android_log_print(ANDROID_LOG_ERROR, "QuestHook",
                                    "dlopen(%s) failed: %s", temp_path, dlerror());
        }
        free(file_list[i]);
    }
    free(file_list);

    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Done loading mods!");
}

/* Forward declaration — implemented in ../modmanager/main.c */
void modmanager_init(void);

__attribute__((constructor)) void lib_main()
{
    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Welcome!");
    /* NOTE: mods are NOT loaded here. External storage isn't reachable this
     * early on a fresh install (the user hasn't granted the runtime storage
     * permission yet). The mod manager calls load_mods() every frame from its
     * Unity hook until storage is available — see modmanager/main.c. */
    modmanager_init();
}