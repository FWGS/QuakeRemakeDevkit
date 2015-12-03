#hlsdk-2.3 client port for android
#Copyright (c) mittorn

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := client
#ifeq ($(XASH_SDL),1)
#APP_PLATFORM := android-12
#LOCAL_SHARED_LIBRARIES += SDL2 
#LOCAL_CFLAGS += -DXASH_SDL
#else
APP_PLATFORM := android-8
#endif
LOCAL_CONLYFLAGS += -std=c99

include $(XASH3D_CONFIG)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a-hard)
LOCAL_MODULE_FILENAME = libclient_hardfp
endif

LOCAL_CFLAGS += -fsigned-char -DCLIENT_DLL=1 -DDISABLE_VEC_ORIGIN

SRCS := cdll_int.cpp \
        death.cpp \
        entity.cpp \
        hud.cpp \
        hud_msg.cpp \
        hud_redraw.cpp \
        hud_sbar.cpp \
        hud_update.cpp \
        in_camera.cpp \
        input.cpp \
        input_xash3d.cpp \
        message.cpp \
        parsemsg.cpp \
        saytext.cpp \
        scoreboard.cpp \
        StudioModelRenderer.cpp \
        text_message.cpp \
        util.cpp \
        view.cpp \
        ../pm_shared/pm_debug.c \
        ../pm_shared/pm_shared.c \
        ../pm_shared/pm_math.c

INCLUDES =  -I../common -I. -I../game_shared -I../pm_shared -I../engine -I../dlls -I../public
DEFINES = -Wno-write-strings -DLINUX -D_LINUX -Dstricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -DCLIENT_WEAPONS -DCLIENT_DLL -w -Wl,--no-undefined

LOCAL_C_INCLUDES := $(LOCAL_PATH)/. \
		 $(LOCAL_PATH)/../common \
		 $(LOCAL_PATH)/../engine \
		 $(LOCAL_PATH)/../game_shared \
		 $(LOCAL_PATH)/../dlls \
		 $(LOCAL_PATH)/../pm_shared \
		 $(LOCAL_PATH)/../public
LOCAL_CFLAGS += $(DEFINES) $(INCLUDES)

LOCAL_SRC_FILES := $(SRCS) $(SRCS_C)

include $(BUILD_SHARED_LIBRARY)
