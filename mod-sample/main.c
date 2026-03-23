#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shared/inline-hook/inlineHook.h"
#include "../shared/utils/utils.h"
#include "../shared/modapi/modapi.h"

#define LOG_TAG "SampleMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

__attribute__((constructor)) void lib_main()
{
    LOGI("sample-plugin loaded!");

    /* Register with the mod manager — shows in UI with this display name */
    aoqmm_register("libsample.so", "Sample Mod", "1.0.0", "YourName",
                   "A minimal example mod for Attack on Quest.");

    /* Declare default config — only written on first run, user edits are preserved */
    aoqmm_ensure_config("libsample.so",
        "{\n"
        "  \"entries\": [\n"
        "    {\"key\":\"Enabled\",\"type\":\"bool\",\"value\":true,"
            "\"description\":\"Enables the mod.\"},\n"
        "    {\"key\":\"Strength\",\"type\":\"float\",\"value\":1.0,"
            "\"description\":\"Effect strength (0.0 to 2.0).\"}\n"
        "  ]\n"
        "}\n"
    );
}
