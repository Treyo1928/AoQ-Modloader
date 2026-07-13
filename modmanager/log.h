#pragma once
#include <android/log.h>

#define MOD_TAG     "AoQModManager"
#define BUNDLE_DIR  "/data/data/com.AoQ.AttackOnQuest/files"
#define BUNDLE_PATH BUNDLE_DIR "/aoqmodmanager.bundle"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  MOD_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MOD_TAG, __VA_ARGS__)
