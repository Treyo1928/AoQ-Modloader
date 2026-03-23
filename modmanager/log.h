#pragma once
#include <android/log.h>

#define MOD_TAG     "AoQModManager"
#define BUNDLE_PATH "/data/data/com.AoQ.AttackOnQuest/files/aoqmodmanager.bundle"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  MOD_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, MOD_TAG, __VA_ARGS__)
