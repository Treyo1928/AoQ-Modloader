# Attack on Quest — ARMv7 Modloader + Mod Manager

> Forked from [jakibaki/beatsaber-hook](https://github.com/jakibaki/beatsaber-hook). Modernized for NDK r26d and Android 12.

A lightweight C/C++ runtime function hooking framework for injecting custom code into the Unity IL2CPP engine of **Attack on Quest (AoQ)**, with a fully integrated in-game mod manager.

Modern ARM64 modloaders (MelonLoader, QuestHooks) crash on AoQ because it's an older ARMv7 title. This framework uses `Android-Inline-Hook` to safely patch 32-bit memory offsets.

---

## How It Works

A static constructor (`<clinit>`) is injected into the game's compiled Smali bytecode. When Unity boots it triggers `libmodloader.so`, which:

1. Runs the **Mod Manager** — installs UI hooks and writes default configs
2. Scans `/sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/` and `dlopen`s every `.so` found there

The Mod Manager is baked directly into `libmodloader.so` — it is not a separate file in the mods folder. It injects a **Mods** button into the game's main menu that opens an in-game panel for enabling/disabling mods and editing mod configs.

---

## Repository Layout

```
AoQ-ModLoader-For-Quest/
├── modloader/          # Builds libmodloader.so (loader + mod manager)
│   ├── main.c          # Entry point: calls modmanager_init() then load_mods()
│   ├── Android.mk
│   └── Application.mk
├── modmanager/         # Mod manager source — baked into libmodloader.so
│   ├── main.c          # Hook installation, bundle extraction, config init
│   ├── ui_inject.c     # Injects Mods button into game main menu
│   ├── ui_panel.c      # In-game panel: tab switching, list population
│   ├── il2cpp.c/.h     # IL2CPP runtime bindings (function pointers, type objects)
│   ├── modlist.c/.h    # Mod discovery: scan mods/ and disabledmods/ folders
│   ├── modconfig.c/.h  # JSON config read/write per mod
│   ├── shader_fix.c    # Unity shader compatibility fixes for the UI panel
│   ├── rva.h           # All IL2CPP RVA offsets (AoQ 0.5.0)
│   ├── cJSON.c/.h      # Embedded JSON parser
│   ├── bundle_data.h   # Embedded asset bundle (UI prefab, generated)
│   └── utils.c/.h      # IL2CPP string helpers, Unity object utilities
├── shared/
│   ├── inline-hook/    # Android-Inline-Hook (ARMv7 trampoline engine)
│   ├── utils/          # getRealOffset(), MAKE_HOOK, INSTALL_HOOK macros
│   └── modapi/
│       └── modapi.h    # Header-only mod registration API (include in your mod)
└── mod-sample/         # Minimal example mod showing modapi.h usage
```

---

## Quick Start — Sideload the Pre-Patched APK

Download the pre-patched APK from the [Releases](https://github.com/Treyo1928/AoQ-Modloader/releases) page and sideload it:
```bash
adb install -r AoQ_Patched.apk
```

Then skip to [Step 4](#step-4--install-a-mod).

---

## Step 1 — Build the Modloader

**Prerequisites:** Android NDK r26d

```bash
git clone https://github.com/Treyo1928/AoQ-Modloader.git
cd AoQ-ModLoader-For-Quest/modloader
/path/to/ndk/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=armeabi-v7a
```

Output: `libs/armeabi-v7a/libmodloader.so`

This single binary contains both the mod loader and the mod manager.

---

## Step 2 — Patch the APK

> Skip if using the pre-patched release APK.

**Prerequisites:**
- `apktool`
- `zipalign` + `apksigner` (Android SDK build-tools)
- A valid Android keystore

**1. Unpack**
```bash
apktool d AoQ-0.5.0.apk -o AoQ_Extracted
```

**2. Drop in the payload**

Copy `libmodloader.so` into `AoQ_Extracted/lib/armeabi-v7a/`.

**3. Smali injection**

Open `AoQ_Extracted/smali/com/unity3d/player/UnityPlayerActivity.smali` and paste this block at the very end of the file (after all existing `.method` blocks):
```smali
.method static constructor <clinit>()V
    .locals 1

    const-string v0, "modloader"
    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V

    return-void
.end method
```

**4. Repack and sign**
```bash
apktool b AoQ_Extracted -o AoQ_Modded.apk
zipalign -p -f 4 AoQ_Modded.apk AoQ_Patched.apk
apksigner sign --ks your_debug.keystore --ks-pass pass:android AoQ_Patched.apk
```

---

## Step 3 — Sideload the Patched APK

```bash
adb install -r AoQ_Patched.apk
```

---

## Step 4 — Install a Mod

Create the mods folder if it doesn't exist:
```bash
adb shell mkdir -p /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods
```

Push your mod:
```bash
adb push libmymod.so /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/
```

Launch the game — any `.so` files in that folder are loaded automatically. Use the **Mods** button in the main menu to manage and configure them in-game.

---

## Step 5 — Write a Mod

Mods are C shared libraries with an auto-executing constructor. See `mod-sample/` for a ready-to-build example. See [MODDING_GUIDE.md](../MODDING_GUIDE.md) for the full reference.

### Minimal mod

```c
#include <android/log.h>
#include "../shared/inline-hook/inlineHook.h"
#include "../shared/utils/utils.h"
#include "../shared/modapi/modapi.h"

#define TAG "MyMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

__attribute__((constructor)) void lib_main()
{
    LOGI("mod loaded!");

    /* Register with the in-game mod manager */
    aoqmm_register("libmymod.so", "My Mod", "1.0.0", "YourName",
                   "Short description shown in the mod manager.");
}
```

### Mod with config

```c
#include "../shared/modapi/modapi.h"

__attribute__((constructor)) void lib_main()
{
    aoqmm_register("libmymod.so", "My Mod", "1.0.0", "YourName",
                   "Does something cool.");

    /* Declare default config — only written on first run, user edits preserved */
    aoqmm_ensure_config("libmymod.so",
        "{\n"
        "  \"entries\": [\n"
        "    {\"key\":\"Enabled\",\"type\":\"bool\",\"value\":true,"
            "\"description\":\"Enables the mod.\"},\n"
        "    {\"key\":\"Strength\",\"type\":\"float\",\"value\":1.0,"
            "\"description\":\"Effect strength (0.0–2.0).\"}\n"
        "  ]\n"
        "}\n"
    );

    /* Read back values at runtime */
    /* (Use modconfig.h load_config / get_entry if you want to read them in C) */
}
```

Config files are stored at:
```
/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs/<modname>.json
```

They are editable in-game via the **Mods → Configure Mods** panel.

---

## On-Device File Layout

```
/sdcard/Android/data/com.AoQ.AttackOnQuest/files/
├── mods/
│   ├── libmymod.so          # enabled mods
│   └── disabledmods/        # disabled mods (folder name configurable)
├── modconfigs/
│   ├── aoqmodmanager.json   # mod manager settings
│   ├── mymod.json           # your mod's config (auto-created by aoqmm_ensure_config)
│   └── mymod.meta.json      # display name / version / author (written by aoqmm_register)
```

---

## Mod Manager Settings

The mod manager exposes its own settings through the Configure Mods panel under **Mod Manager**:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ShowModdedText` | bool | true | Show "MODDED (N mods)" in the main menu version text |
| `ShowAllMods` | bool | false | Show mods with no config in the Configure Mods list |
| `DisabledModsFolder` | string | `disabledmods` | Subfolder inside `mods/` for disabled `.so` files. Renaming this migrates files automatically. |
| `ModRefreshInterval` | int | 0 | Auto-refresh interval in seconds (0 = never) |

---

## Verify It's Working

```bash
adb logcat -s AoQModManager,QuestHook
```

You should see:
```
QuestHook: Welcome!
AoQModManager: aoqmodmanager: loading...
AoQModManager: Hooks installed.
QuestHook: Loading mods!
QuestHook: Done loading mods!
```
