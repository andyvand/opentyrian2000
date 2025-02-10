LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := c++_shared
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libc++_shared.so
#LOCAL_EXPORT_C_INCLUDES :=
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL3
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libSDL3.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL3_net
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libSDL3_net.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := SDL3_mixer
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libSDL3_mixer.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := fluidsynth
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libfluidsynth.so
#LOCAL_EXPORT_C_INCLUDES :=
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := midiproc
LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(APP_ABI)/libmidiproc_shared.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/midiproc
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := main

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES := ../../../../src/animlib.c ../../../../src/arg_parse.c ../../../../src/backgrnd.c ../../../../src/config.c ../../../../src/config_file.c ../../../../src/destruct.c ../../../../src/editship.c ../../../../src/episodes.c ../../../../src/file.c ../../../../src/font.c ../../../../src/fonthand.c ../../../../src/game_menu.c ../../../../src/helptext.c ../../../../src/joystick.c ../../../../src/jukebox.c ../../../../src/keyboard.c ../../../../src/lds_play.c ../../../../src/loudness.c ../../../../src/lvllib.c ../../../../src/lvlmast.c ../../../../src/mainint.c ../../../../src/menus.c ../../../../src/mouse.c ../../../../src/mtrand.c ../../../../src/musmast.c ../../../../src/network.c ../../../../src/nortsong.c ../../../../src/nortvars.c ../../../../src/opentyr.c ../../../../src/opl.c ../../../../src/palette.c ../../../../src/params.c ../../../../src/pcxload.c ../../../../src/pcxmast.c ../../../../src/picload.c ../../../../src/player.c ../../../../src/shots.c ../../../../src/sizebuf.c ../../../../src/sndmast.c ../../../../src/sprite.c ../../../../src/starlib.c ../../../../src/tyrian2.c ../../../../src/varz.c ../../../../src/vga256d.c ../../../../src/vga_palette.c ../../../../src/video.c ../../../../src/video_scale.c ../../../../src/video_scale_hqNx.c ../../../../src/xmas.c

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid -lc++ -lc++abi
LOCAL_SHARED_LIBRARIES := SDL3 SDL3_net SDL3_mixer fluidsynth midiproc c++_shared

include $(BUILD_SHARED_LIBRARY)
