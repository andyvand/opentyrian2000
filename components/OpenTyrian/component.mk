#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

#include $(IDF_PATH)/make/component_common.mk

#COMPONENT_ADD_INCLUDEDIRS += ../tft ../spidriver
COMPONENT_ADD_INCLUDEDIRS += ../SDL/include
COMPONENT_ADD_INCLUDEDIRS += ../SDL/SDL/include
COMPONENT_ADD_INCLUDEDIRS += ../SDL3_net
CFLAGS += -DWITH_SDL3=1 -DWITH_SDL3_ESP=1 -DWITH_SDL2NET=1 -DWITH_NETWORK=1
