# AoQ Quest Mod Writing Guide
**For: AttackOnQuest (com.AoQ.AttackOnQuest) — version 0.5.0**
**Framework: AoQ-ModLoader-For-Quest (ARMv7 C hooking)**

---

## Table of Contents
1. [How Mods Are Loaded](#1-how-mods-are-loaded)
2. [Registering with the Mod Manager](#2-registering-with-the-mod-manager)
3. [Adding a Config](#3-adding-a-config)
4. [Reading Config at Runtime](#4-reading-config-at-runtime)
5. [Finding Methods in dump.cs](#5-finding-methods-in-dumpcs)
6. [IL2CPP ABI Basics](#6-il2cpp-abi-basics)
7. [The MAKE_HOOK Pattern](#7-the-make_hook-pattern)
8. [Prefix / Postfix Equivalents](#8-prefix--postfix-equivalents)
9. [Accessing Struct Fields](#9-accessing-struct-fields)
10. [Calling Game Functions From a Hook](#10-calling-game-functions-from-a-hook)
11. [Calling OVRInput](#11-calling-ovrinput)
12. [Building and Deploying](#12-building-and-deploying)
13. [Case Study: Porting LeviGrip to Quest](#13-case-study-porting-levygrip-to-quest)
14. [Concept Cheat Sheet](#14-concept-cheat-sheet)

---

## 1. How Mods Are Loaded

`libmodloader.so` is baked into the game APK and loaded by a Smali `<clinit>` hook on startup. It:

1. Initialises the **Mod Manager** (installs UI hooks, writes default configs)
2. Scans `/sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/` alphabetically and `dlopen`s every `.so` found there

Your entry point is a constructor function that runs automatically when your `.so` is loaded:

```c
__attribute__((constructor)) void lib_main()
{
    // runs automatically — install hooks, register with mod manager
}
```

Watch your log output:
```bash
adb logcat -s MyModTag,QuestHook,AoQModManager
```

---

## 2. Registering with the Mod Manager

Include `modapi.h` and call `aoqmm_register()` from your constructor. This writes a `.meta.json` file that the mod manager reads to display your mod's name, version, and author instead of the raw `.so` filename.

```c
#include "../../AoQ-ModLoader-For-Quest/shared/modapi/modapi.h"

__attribute__((constructor)) void lib_main()
{
    aoqmm_register(
        "libmymod.so",              /* your .so filename — must match exactly */
        "My Mod Display Name",      /* shown in the Mods panel */
        "1.0.0",                    /* version string */
        "YourName",                 /* author */
        "One-line description."     /* shown in Configure Mods */
    );

    // ... rest of your mod setup
}
```

`aoqmm_register` always overwrites the metadata file so the displayed info stays current with your build.

---

## 3. Adding a Config

Call `aoqmm_ensure_config()` after `aoqmm_register()`. It writes a JSON config file **only on first run** — if the file already exists it is left untouched, so user edits are preserved across game restarts.

```c
aoqmm_ensure_config("libmymod.so",
    "{\n"
    "  \"entries\": [\n"
    "    {\n"
    "      \"key\": \"Enabled\",\n"
    "      \"type\": \"bool\",\n"
    "      \"value\": true,\n"
    "      \"description\": \"Enables the mod.\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"Strength\",\n"
    "      \"type\": \"float\",\n"
    "      \"value\": 1.0,\n"
    "      \"description\": \"Effect strength (0.0 to 2.0).\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"Label\",\n"
    "      \"type\": \"string\",\n"
    "      \"value\": \"default\",\n"
    "      \"description\": \"A text label.\"\n"
    "    },\n"
    "    {\n"
    "      \"key\": \"Count\",\n"
    "      \"type\": \"int\",\n"
    "      \"value\": 5,\n"
    "      \"description\": \"How many times to do the thing.\"\n"
    "    }\n"
    "  ]\n"
    "}\n"
);
```

**Supported types:** `bool`, `float`, `int`, `string`

The config file is written to:
```
/sdcard/Android/data/com.AoQ.AttackOnQuest/files/modconfigs/mymod.json
```
(`lib` prefix and `.so` suffix are stripped automatically.)

Users can edit all values in-game via **Mods → Configure Mods → [Your Mod]**.

---

## 4. Reading Config at Runtime

Include `modconfig.h` to read your config values in C. Always read on demand — don't cache at startup, since the user may have changed values in-game.

```c
#include "../../AoQ-ModLoader-For-Quest/modmanager/modconfig.h"

static int get_cfg_enabled(void)
{
    ModConfig cfg;
    if (load_config("libmymod.so", &cfg) != 0) return 1; /* default if missing */
    ModCfgEntry *e = get_entry(&cfg, "Enabled");
    return e ? (int)e->value_num : 1;
}

static float get_cfg_strength(void)
{
    ModConfig cfg;
    if (load_config("libmymod.so", &cfg) != 0) return 1.0f;
    ModCfgEntry *e = get_entry(&cfg, "Strength");
    return e ? (float)e->value_num : 1.0f;
}
```

For string values use `e->value_str`. For booleans, `value_num` is `1.0` (true) or `0.0` (false).

---

## 5. Finding Methods in dump.cs

`Il2CppDumper/dump.cs` was generated from the game APK. Every method lists its **RVA** (relative virtual address) — the number you pass to `MAKE_HOOK` or `getRealOffset`.

```csharp
// RVA: 0x4A8008 Offset: 0x4A8008 VA: 0x4A8008
public void UpdateKills() { }
```

Fields also show their byte offset from the start of the object:

```csharp
public class WeaponSwap : MonoBehaviour
{
    public GameObject rightSword;    // 0xC
    public GameObject flareGun;      // 0x14
    private GameObject timerCanvas;  // 0x18
    private GameObject player;       // 0x1C
}
```

Search tip:
```bash
grep -n "MethodName\|ClassName" Il2CppDumper/dump.cs | grep RVA
```

---

## 6. IL2CPP ABI Basics

AoQ is compiled as **ARMv7 (armeabi-v7a)** with the **softfp** ABI. All pointers are 4 bytes.

### Instance methods — `self` is the first argument
```csharp
// C#
public void UpdateKills() { }

// Native
void UpdateKills(Scoring *self);
```

### Static methods — no `self`, hidden `MethodInfo*` at the end
```csharp
// C#
public static bool GetDown(OVRInput.Button btn, OVRInput.Controller ctrl) { }

// Native
int OVRInput_GetDown(int btn, int ctrl, void *method_info);
```

Passing `NULL` for `method_info` works for the vast majority of concrete (non-generic) methods.

### Enums are ints
```c
// OVRInput.Button.One              = 0x00000001
// OVRInput.Button.Two              = 0x00000002
// OVRInput.Button.PrimaryThumbstick= 0x00000400 (up) / 0x00000800 (down)
// OVRInput.Controller.Active       = 0x80000000
```

### Unity structs are plain C structs
```c
typedef struct { float x, y, z; }    Vector3;
typedef struct { float x, y, z, w; } Quaternion;
typedef struct { float r, g, b, a; } Color;
typedef struct { float x, y; }       Vector2;
```

### Structs returned by value
- Structs ≤ 8 bytes (e.g. `Vector2`) are returned in `r0:r1`
- Structs > 8 bytes (e.g. `Vector3`) are returned via a hidden first-argument output pointer

### Bool return values
IL2CPP returns booleans as `int` (0 or 1), not C `_Bool`.

---

## 7. The MAKE_HOOK Pattern

```c
#include "../../AoQ-ModLoader-For-Quest/shared/inline-hook/inlineHook.h"
#include "../../AoQ-ModLoader-For-Quest/shared/utils/utils.h"

// MAKE_HOOK(name, RVA, returnType, args...)
//   Creates a trampoline `name(args)` that calls the original function
//   Creates your hook body `hook_name(args)` where you put your code
//
// INSTALL_HOOK(name)
//   Patches the game at runtime to redirect to your hook

MAKE_HOOK(UpdateKills, 0x4A8008, void, void *self)
{
    log("A titan was killed!");
    UpdateKills(self);  // call original — omit to replace entirely
}

__attribute__((constructor)) void lib_main()
{
    INSTALL_HOOK(UpdateKills);
}
```

Inside the hook body, `UpdateKills(self)` calls the original unhooked code. The name you give `MAKE_HOOK` is what you call to invoke the original.

---

## 8. Prefix / Postfix Equivalents

| Harmony pattern | Quest C equivalent |
|---|---|
| Prefix returning `true` | Call original at the **end** of your hook |
| Prefix returning `false` | Don't call original — just return |
| Postfix | Call original **first**, then do your work |
| `__instance.field` | Read via byte offset (see §9) |

### Prefix (run before, then continue)
```c
MAKE_HOOK(WeaponSwap_Update, 0x771800, void, void *self)
{
    log("Before Update");
    WeaponSwap_Update(self);  // run original
}
```

### Replace entirely
```c
MAKE_HOOK(WeaponSwap_Update, 0x771800, void, void *self)
{
    log("Original Update never runs");
    // no call to WeaponSwap_Update(self)
}
```

### Postfix (run after)
```c
MAKE_HOOK(OVRCameraRig_UpdateAnchors, 0x5AB168, void, void *self, int updateEye, int updateHand)
{
    OVRCameraRig_UpdateAnchors(self, updateEye, updateHand);  // original first
    log("After UpdateAnchors");
}
```

---

## 9. Accessing Struct Fields

Cast `self` to `char *`, add the byte offset from `dump.cs`, then cast to the field's type.

```c
// WeaponSwap fields from dump.cs:
//   public GameObject rightSword;    // 0xC
//   public GameObject flareGun;      // 0x14
//   private GameObject timerCanvas;  // 0x18

// Read:
void *rightSword  = *(void **)((char *)self + 0x0C);
void *flareGun    = *(void **)((char *)self + 0x14);
void *timerCanvas = *(void **)((char *)self + 0x18);

// Write:
*(void **)((char *)self + 0x0C) = someNewGameObject;

// Read a float field (e.g. "public float speed; // 0x24"):
float speed = *(float *)((char *)self + 0x24);

// Read a bool/int field (e.g. "private bool isActive; // 0x28"):
int isActive = *(int *)((char *)self + 0x28);
```

> All object references are 4-byte pointers on ARMv7. The 4-byte spacing between fields in dump.cs confirms this.

---

## 10. Calling Game Functions From a Hook

Resolve any game method by its RVA and call it through a function pointer.

```c
// Example: call GameObject.SetActive(bool)
// dump.cs: public void SetActive(bool value) { }  RVA: 0xC745F0
// Native ABI: self + value (as int)

typedef void (*SetActive_t)(void *self, int value);
static SetActive_t fn_SetActive = NULL;

// In lib_main, resolve once:
fn_SetActive = (SetActive_t) getRealOffset(0xC745F0);

// Call it:
fn_SetActive(myGameObject, 0);  // SetActive(false)
fn_SetActive(myGameObject, 1);  // SetActive(true)
```

> The hidden `MethodInfo *` parameter appears at the **end** of static methods only. For instance methods like `SetActive`, there is no `MethodInfo*` — the ABI is just `(self, args...)`. Start without it; add it only if the game crashes on that specific call (very rare for simple concrete methods).

---

## 11. Calling OVRInput

`OVRInput.GetDown` / `Get` / `GetUp` are **static** methods — no `self`, but they do have the hidden `MethodInfo*` at the end.

```c
typedef int (*OVRInput_GetDown_t)(int button, int controller, void *mi);
typedef int (*OVRInput_Get_t)    (int button, int controller, void *mi);
typedef int (*OVRInput_GetUp_t)  (int button, int controller, void *mi);

static OVRInput_GetDown_t OVRInput_GetDown = NULL;
static OVRInput_Get_t     OVRInput_Get     = NULL;
static OVRInput_GetUp_t   OVRInput_GetUp   = NULL;

// In lib_main:
OVRInput_GetDown = (OVRInput_GetDown_t) getRealOffset(0xAF7CAC);
OVRInput_Get     = (OVRInput_Get_t)     getRealOffset(0xAF7A08);
OVRInput_GetUp   = (OVRInput_GetUp_t)   getRealOffset(0xAF7F80);

// Usage inside a per-frame hook:
if (OVRInput_GetDown(0x00000001, (int)0x80000000, NULL))  // A button, Active controller
    log("A pressed!");

if (OVRInput_Get(0x00000400, (int)0x80000000, NULL))      // Thumbstick up, held
    log("Thumbstick held up!");
```

**Button / controller values** (from `rva.h` / `dump.cs`):

| Constant | Value | Meaning |
|---|---|---|
| `OVR_BTN_ONE` | `0x00000001` | A (right) / X (left) |
| `OVR_BTN_TWO` | `0x00000002` | B (right) / Y (left) |
| `OVR_BTN_PRIMARY_TS_U` | `0x00000400` | Primary thumbstick up |
| `OVR_BTN_PRIMARY_TS_D` | `0x00000800` | Primary thumbstick down |
| `OVR_CTRL_ACTIVE` | `0x80000000` | Active controller (either hand) |

---

## 12. Building and Deploying

### Android.mk

```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE     := mymod                # output: libmymod.so
LOCAL_LDLIBS     := -llog
LOCAL_SRC_FILES  := src/main.c \
    ../../AoQ-ModLoader-For-Quest/shared/inline-hook/inlineHook.c \
    ../../AoQ-ModLoader-For-Quest/shared/inline-hook/relocate.c \
    ../../AoQ-ModLoader-For-Quest/shared/utils/utils.c
include $(BUILD_SHARED_LIBRARY)
```

If you use `modconfig.h` to read configs, also add:
```makefile
    ../../AoQ-ModLoader-For-Quest/modmanager/modconfig.c \
    ../../AoQ-ModLoader-For-Quest/modmanager/cJSON.c
```

And add the include path:
```makefile
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../AoQ-ModLoader-For-Quest/modmanager
```

### Application.mk

```makefile
APP_ABI      := armeabi-v7a
APP_PLATFORM := android-21
APP_STL      := c++_static
APP_CFLAGS   := -std=gnu17
```

### Build and deploy

```bash
# From your mod folder:
/path/to/ndk/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=armeabi-v7a

# Push to device:
adb push libs/armeabi-v7a/libmymod.so \
    /sdcard/Android/data/com.AoQ.AttackOnQuest/files/mods/

# Watch logs:
adb logcat -s MyModTag,AoQModManager,QuestHook
```

Restart the game after pushing. In-game, open **Mods → Configure Mods** to see your config.

---

## 13. Case Study: Porting LeviGrip to Quest

### What the mod does

| C# file | Purpose |
|---|---|
| `Plugin.cs` | State (grip toggles, flip animation vars). Left A toggles left grip. |
| `weaponSwapPatch.cs` | Replaces `WeaponSwap.Update`. Right thumbstick hold = swap weapon; tap = flip grip. |
| `networkWeaponSwapPatch.cs` | Same for `NetworkWeaponSwap.Update` (multiplayer). |
| `OVRPatch.cs` | Postfix on `OVRCameraRig.UpdateAnchors`. Rotates hand anchors with animated lerp. |

### Key dump.cs values

| Method | RVA | Notes |
|---|---|---|
| `WeaponSwap.Update` | `0x771800` | Replace entirely |
| `NetworkWeaponSwap.Update` | `0x63584C` | Replace entirely |
| `OVRCameraRig.UpdateAnchors` | `0x5AB168` | Postfix |
| `OVRInput.GetDown(Button, Controller)` | `0xAF7CAC` | Static — call via function pointer |
| `OVRInput.Get(Button, Controller)` | `0xAF7A08` | Static — call via function pointer |

### Field layouts from dump.cs

**OVRCameraRig** (line 196020):
```
private Transform <leftHandAnchor>k__BackingField;   // 0x1C
private Transform <rightHandAnchor>k__BackingField;  // 0x20
```

**WeaponSwap** (line 252784):
```
public GameObject rightSword;    // 0xC
public GameObject leftSword;     // 0x10
public GameObject flareGun;      // 0x14
private GameObject timerCanvas;  // 0x18
private GameObject player;       // 0x1C
```

### Converted mod skeleton

```c
#include <android/log.h>
#include <math.h>
#include "../../AoQ-ModLoader-For-Quest/shared/inline-hook/inlineHook.h"
#include "../../AoQ-ModLoader-For-Quest/shared/utils/utils.h"
#include "../../AoQ-ModLoader-For-Quest/shared/modapi/modapi.h"

#define TAG "LeviGrip"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

typedef struct { float x, y, z; }    Vector3;
typedef struct { float x, y, z, w; } Quaternion;

/* Static method pointers */
typedef int   (*OVRInput_GetDown_t)(int btn, int ctrl, void *mi);
typedef int   (*OVRInput_Get_t)    (int btn, int ctrl, void *mi);
typedef void  (*Transform_Rotate_t)(void *self, Vector3 eulers, void *mi);
typedef float (*Time_get_time_t)   (void *mi);

static OVRInput_GetDown_t OVRInput_GetDown = NULL;
static OVRInput_Get_t     OVRInput_Get     = NULL;
static Transform_Rotate_t Transform_Rotate = NULL;
static Time_get_time_t    Time_get_time    = NULL;

static float get_time(void) { return Time_get_time ? Time_get_time(NULL) : 0.0f; }

/* Grip state */
static int   leftLeviGrip  = 0, rightLeviGrip = 0;
static int   leftFlipAnim  = 0, rightFlipAnim  = 0;
static float leftFlipStart = 0, rightFlipStart = 0;

static const float FLIP_DURATION = 0.25f;
static const float HOLD_DURATION = 0.2f;
static const Vector3 GRIP_OFFSET = {82.0f, 0.0f, 180.0f};

static Vector3 lerp_v3(Vector3 a, Vector3 b, float t) {
    return (Vector3){ a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t };
}

/* ── WeaponSwap.Update ─────────────────────────────────────────────────── */
static float weaponSwapPressStart = 0; static int weaponSwapPressing = 0;

MAKE_HOOK(WeaponSwap_Update, 0x771800, void, void *self)
{
    /* Original never called — full replacement */
    float now = get_time();
    if (OVRInput_GetDown(0x400, (int)0x80000000, NULL)) {
        weaponSwapPressStart = now; weaponSwapPressing = 1;
    }
    if (OVRInput_Get(0x400, (int)0x80000000, NULL)) {
        if (now - weaponSwapPressStart > HOLD_DURATION && weaponSwapPressing) {
            weaponSwapPressing = 0;
            LOGI("swap weapon");
            /* TODO: call SetActive on rightSword/flareGun */
        }
    } else if (weaponSwapPressing) {
        weaponSwapPressing = 0;
        rightFlipAnim = 1; rightLeviGrip = !rightLeviGrip; rightFlipStart = now;
    }
}

/* ── OVRCameraRig.UpdateAnchors ────────────────────────────────────────── */
MAKE_HOOK(OVRCameraRig_UpdateAnchors, 0x5AB168, void, void *self, int updateEye, int updateHand)
{
    OVRCameraRig_UpdateAnchors(self, updateEye, updateHand);  /* postfix */

    void *leftAnchor  = *(void **)((char *)self + 0x1C);
    void *rightAnchor = *(void **)((char *)self + 0x20);
    float now = get_time();
    Vector3 zero = {0,0,0};

    /* Left A button toggles left grip */
    if (OVRInput_GetDown(0x1, (int)0x80000000, NULL) && !leftFlipAnim) {
        leftFlipAnim = 1; leftLeviGrip = !leftLeviGrip; leftFlipStart = now;
    }

    /* Rotate left hand anchor */
    if (leftLeviGrip && !leftFlipAnim) {
        if (Transform_Rotate) Transform_Rotate(leftAnchor, GRIP_OFFSET, NULL);
    } else if (leftFlipAnim) {
        Vector3 start = leftLeviGrip ? zero : GRIP_OFFSET;
        Vector3 end   = leftLeviGrip ? GRIP_OFFSET : zero;
        float t = (now - leftFlipStart) / FLIP_DURATION;
        if (Transform_Rotate) Transform_Rotate(leftAnchor, lerp_v3(start, end, t), NULL);
        if (now - leftFlipStart > FLIP_DURATION) leftFlipAnim = 0;
    }

    /* Rotate right hand anchor */
    Vector3 rightOffset = {GRIP_OFFSET.x, GRIP_OFFSET.y, GRIP_OFFSET.z - 360.0f};
    if (rightLeviGrip && !rightFlipAnim) {
        if (Transform_Rotate) Transform_Rotate(rightAnchor, GRIP_OFFSET, NULL);
    } else if (rightFlipAnim) {
        Vector3 start = rightLeviGrip ? zero : rightOffset;
        Vector3 end   = rightLeviGrip ? rightOffset : zero;
        float t = (now - rightFlipStart) / FLIP_DURATION;
        if (Transform_Rotate) Transform_Rotate(rightAnchor, lerp_v3(start, end, t), NULL);
        if (now - rightFlipStart > FLIP_DURATION) rightFlipAnim = 0;
    }
}

/* ── Entry point ───────────────────────────────────────────────────────── */
__attribute__((constructor)) void lib_main()
{
    LOGI("loading...");

    aoqmm_register("liblevigrip.so", "Levi Grip", "1.0.0", "YourName",
                   "Rotates hand anchors to replicate levi-style weapon grip.");

    aoqmm_ensure_config("liblevigrip.so",
        "{\n  \"entries\": [\n"
        "    {\"key\":\"GripOffsetX\",\"type\":\"float\",\"value\":82.0,"
            "\"description\":\"X rotation of the grip offset in degrees.\"},\n"
        "    {\"key\":\"FlipDuration\",\"type\":\"float\",\"value\":0.25,"
            "\"description\":\"Seconds for the flip animation.\"}\n"
        "  ]\n}\n"
    );

    OVRInput_GetDown = (OVRInput_GetDown_t) getRealOffset(0xAF7CAC);
    OVRInput_Get     = (OVRInput_Get_t)     getRealOffset(0xAF7A08);
    /* Find these RVAs in dump.cs: */
    /* Transform_Rotate = (Transform_Rotate_t) getRealOffset(0x???); */
    /* Time_get_time    = (Time_get_time_t)    getRealOffset(0x???); */

    INSTALL_HOOK(WeaponSwap_Update);
    INSTALL_HOOK(OVRCameraRig_UpdateAnchors);

    LOGI("hooks installed!");
}
```

### Remaining TODOs

1. **`Transform.Rotate(Vector3)` RVA** — `grep -n "void Rotate" dump.cs | grep RVA`
2. **`Time.get_time` RVA** — `grep -n "get_time\|float time" dump.cs | grep RVA`
3. **`GameObject.SetActive` RVA** — already in the framework at `0xC745F0`
4. **`NetworkWeaponSwap.Update`** — same pattern as `WeaponSwap_Update` above, RVA `0x63584C`

---

## 14. Concept Cheat Sheet

| C# / Harmony | Quest C equivalent |
|---|---|
| `[HarmonyPatch(typeof(Foo), "Bar")]` | `MAKE_HOOK(Foo_Bar, 0xRVA, ...)` |
| Prefix returning `false` | Hook body that doesn't call the original |
| Prefix returning `true` | Hook body that calls original at the end |
| Postfix | Hook body that calls original first |
| `__instance.publicField` | `*(Type **)((char *)self + OFFSET)` |
| `Traverse.Create(x).Field("f").GetValue()` | Same — private fields have offsets in dump too |
| `Time.time` | `Time_get_time(NULL)` — resolve RVA at startup |
| `OVRInput.GetDown(btn, ctrl)` | `OVRInput_GetDown(btn, ctrl, NULL)` |
| `new Vector3(x, y, z)` | `(Vector3){x, y, z}` — C compound literal |
| `Vector3.Lerp(a, b, t)` | Implement inline (it's just linear math) |
| `Logger.LogInfo("msg")` | `__android_log_print(ANDROID_LOG_INFO, TAG, "msg")` |
| Register mod name/version | `aoqmm_register("libmod.so", "Name", "1.0", "Author", "Desc")` |
| Write default config | `aoqmm_ensure_config("libmod.so", json_string)` |
| Read config value | `load_config("libmod.so", &cfg)` → `get_entry(&cfg, "Key")` |
