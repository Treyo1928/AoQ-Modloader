# Changelog

## v1.2.0

Big reliability pass on the loader and in-game mod manager, plus a new shared
utility library for mod authors.

### Fixed
- **Mods now load on a fresh install without adb.** Mods were loaded at native
  library-load time — before Android grants the storage permission on first
  launch — so nothing loaded and the mod list was empty. Loading is now deferred
  and retried every frame until external storage is reachable (a couple seconds
  in, right after you approve the storage prompt, still on the main menu).
- **Mods button disappearing after a level restart.** Re-injection used a camera
  rig handle captured once at startup; the rig is destroyed and recreated on a
  scene reload, leaving a stale handle so the menu couldn't be found. It's now
  refreshed every frame. The destroyed-button check also runs every frame
  instead of every ~2 seconds, closing a use-after-free window that made the
  button occasionally fail to come back.
- **Config menus stopped responding in long sessions.** The button registry
  leaked an entry on every list repopulation until it filled up; list rows also
  leaked as hidden GameObjects. Rows are now unregistered and destroyed on
  refresh.
- **Disabled mods no longer appear in the Configure list** (they kept their
  config files on disk, so the old filter still showed them).
- **The mod manager lists itself as "Mod Manager"** instead of the raw
  `libaoqmodmanager.so` filename (it never wrote its own metadata).
- Config files are read and written from a single location again —
  `/sdcard/DCIM/AoQMods/modconfigs/`. A half-finished path migration had the
  loader writing there while the manager read the old `Android/data` path, so
  mod names and defaults never showed up.
- App-data directories that don't exist yet on a fresh install are created
  before first use (bundle extraction, temp mod copies) instead of failing
  silently.

### Added
- **`shared/aoqcore` utility library** for mod authors: `photonView.IsMine`
  gating, destroyed-object checks (`aoq_alive`), a cached config reader, a
  tap/hold input helper, TMP/transform helpers, and typed field access. See the
  Modding Guide, section 15.
- Each mod is copied to its own temp filename before loading, so crash
  backtraces name the actual mod; `dlopen`/`open` failures are now logged.

### Patched APK note
- The distributed patched APK now sets **`targetSdkVersion=29`**. At the game's
  original target 30, Android 11+ enforces scoped storage and ignores
  `requestLegacyExternalStorage`, which blocks the game from reading
  `/sdcard/DCIM/AoQMods`. At 29 the single storage permission prompt is all a
  user needs — no adb, no "All files access" toggle. If you build your own APK,
  apply this (see README → Patch the APK, step 3b).

### For mod authors
- Update your `Android.mk` to build `shared/aoqcore/aoq.c` and read config via
  `AoqCfgCache` (Modding Guide §4/§15).
- Always gate `Network*` hooks with `aoq_is_mine()` and guard cached Unity
  objects with `aoq_alive()` — these two rules prevent nearly every multiplayer
  crash and wrong-player bug.
