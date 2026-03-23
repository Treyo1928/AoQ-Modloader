LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_LDLIBS    := -llog
LOCAL_MODULE    := modloader
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../modmanager
LOCAL_SRC_FILES := \
    main.c \
    ../modmanager/il2cpp.c \
    ../modmanager/utils.c \
    ../modmanager/shader_fix.c \
    ../modmanager/ui_inject.c \
    ../modmanager/ui_panel.c \
    ../modmanager/modlist.c \
    ../modmanager/modconfig.c \
    ../modmanager/cJSON.c \
    ../modmanager/main.c \
    ../shared/inline-hook/inlineHook.c \
    ../shared/inline-hook/relocate.c \
    ../shared/utils/utils.c
include $(BUILD_SHARED_LIBRARY)
