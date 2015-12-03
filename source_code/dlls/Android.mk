#HLSDK server Android port
#Copyright (c) nicknekit

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := server

include $(XASH3D_CONFIG)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a-hard)
LOCAL_MODULE_FILENAME = libserver_hardfp
endif

LOCAL_CFLAGS += -D_LINUX -DCLIENT_WEAPONS -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf \
	-fno-exceptions

LOCAL_CPPFLAGS := $(LOCAL_CFLAGS) -frtti

LOCAL_C_INCLUDES := $(SDL_PATH)/include \
		    $(LOCAL_PATH)/. \
		    $(LOCAL_PATH)/wpn_shared \
		    $(LOCAL_PATH)/../common \
		    $(LOCAL_PATH)/../engine/common \
		    $(LOCAL_PATH)/../engine \
		    $(LOCAL_PATH)/../public \
		    $(LOCAL_PATH)/../pm_shared \
		    $(LOCAL_PATH)/../game_shared

LOCAL_SRC_FILES := animating.cpp \
        animation.cpp \
        bmodels.cpp \
        boss.cpp \
        buttons.cpp \
        cbase.cpp \
        client.cpp \
        combat.cpp \
        demon.cpp \
        dll_int.cpp \
        dog.cpp \
        doors.cpp \
        enforcer.cpp \
        fish.cpp \
        game.cpp \
        gamerules.cpp \
        globals.cpp \
        hellknight.cpp \
        items.cpp \
        knight.cpp \
        lights.cpp \
        misc.cpp \
        monsters.cpp \
        multiplay_gamerules.cpp \
        ogre.cpp \
        oldone.cpp \
        physics.cpp \
        plats.cpp \
        player.cpp \
        saverestore.cpp \
        shalrath.cpp \
        shambler.cpp \
        singleplay_gamerules.cpp \
        soldier.cpp \
        sound.cpp \
        subs.cpp \
        tarbaby.cpp \
        teamplay_gamerules.cpp \
        triggers.cpp \
        util.cpp \
        weapons.cpp \
        wizard.cpp \
        world.cpp \
        zombie.cpp \
        ../pm_shared/pm_debug.c \
        ../pm_shared/pm_shared.c \
        ../pm_shared/pm_math.c

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
