# Attack on Quest — ARMv7 Modloader Framework

> Forked from [jakibaki/beatsaber-hook](https://github.com/jakibaki/beatsaber-hook). Modernized for NDK r26d and Android 12.

A lightweight C/C++ runtime function hooking framework for injecting custom code into the Unity IL2CPP engine of **Attack on Quest (AoQ)**.

Modern ARM64 modloaders (MelonLoader, QuestHooks) crash on AoQ because it's an older ARMv7 title. This framework uses `Android-Inline-Hook` to safely patch 32-bit memory offsets.

## How It Works

A static constructor (`<clinit>`) is injected into the game's compiled Smali bytecode. When Unity boots, it triggers `libmodloader.so`, which scans `/sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/` and loads any `.so` mods found there.

---

## Quick Start — Sideload the Pre-Patched APK

If you just want to play with mods and don't need to build anything yourself, download the pre-patched APK from the [Releases](https://github.com/Treyo1928/AoQ-Modloader/releases) page and sideload it via SideQuest or `adb`:
```bash
adb install -r AoQ_Modded_Aligned.apk
```

Then skip to [Step 4](#step-4--install-a-mod) to start loading mods.

---

## Step 1 — Build the Modloader

> Skip this step if you're using the pre-patched release APK above.

**Prerequisites:** Android NDK r26d (recommended)
```bash
git clone https://github.com/Treyo1928/AoQ-Modloader.git
cd beatsaber-hook/modloader
/path/to/ndk/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=armeabi-v7a
```

Output: `libs/armeabi-v7a/libmodloader.so`

---

## Step 2 — Patch the APK

> Skip this step if you're using the pre-patched release APK above.

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

Open `AoQ_Extracted/smali/com/unity3d/player/UnityPlayerActivity.smali` and paste the following block at the very end of the file:
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
Install the patched APK (either prepatched by me or patched by you) via SideQuest or `adb`:
```bash
adb install -r AoQ_Patched.apk
```

---

## Step 4 — Install a Mod

Create the mods folder if it doesn't exist yet (via SideQuest's file browser, or adb):
```bash
adb shell mkdir -p /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods
```

Then push your mod:
```bash
adb push yourmod.so /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/
```

Launch the game — any `.so` files in that folder will be loaded automatically.

---

## Step 5 (Optional) — Write a Mod

Mods are C++ shared libraries with an auto-executing constructor. Here's a minimal example to verify everything is working:

**`testmod.cpp`**
```cpp
#include <android/log.h>

#define LOG_TAG "AoQ_Modding"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

__attribute__((constructor)) void lib_main() {
    LOGI("SUCCESS: mod loaded and executed.");
}
```

Compile it to `libtestmod.so` (armeabi-v7a), push it to the mods folder per Step 4, then verify:
```bash
adb logcat | grep AoQ_Modding
```