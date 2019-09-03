BOARD_TAG = nano
BOARD_SUB = atmega328
ARDMK_VENDOR = archlinux-arduino
CPPFLAGS += -Wno-narrowing
include /usr/share/arduino/Arduino.mk

# Add usbdrv files
LOCAL_MY_C_SRCS := $(wildcard usbdrv/*.c)
LOCAL_MY_CPP_SRCS := $(wildcard usbdrv/*.cpp)
LOCAL_MY_AS_SRCS := $(wildcard usbdrv/*.S)
LOCAL_C_SRCS += $(LOCAL_MY_C_SRCS)
LOCAL_CPP_SRCS += $(LOCAL_MY_CPP_SRCS)
LOCAL_AS_SRCS += $(LOCAL_MY_AS_SRCS)
LOCAL_MY_OBJ_FILES := $(LOCAL_MY_C_SRCS:.c=.c.o) $(LOCAL_MY_CPP_SRCS:.cpp=.cpp.o) $(LOCAL_MY_AS_SRCS:.S=.S.o)
LOCAL_MY_OBJS := $(patsubst %,$(OBJDIR)/%,$(LOCAL_MY_OBJ_FILES))


.PHONY: my myup

my: $(LOCAL_MY_OBJS) all

myup: my upload