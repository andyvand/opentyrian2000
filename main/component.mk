#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#

#include $(IDF_PATH)/make/component_common.mk

COMPONENT_ADD_INCLUDEDIRS += ../components/OpenTyrian2000
CFLAGS += -DWITH_SDL=1 -DNETWORK_GAME=1
