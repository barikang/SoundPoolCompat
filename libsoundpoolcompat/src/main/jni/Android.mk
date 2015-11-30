LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_CFLAGS += -fexceptions
LOCAL_EXPORT_LDLIBS := -lOpenSLES -landroid
LOCAL_CPPFLAGS += -std=c++11 -pthread -fsigned-char

LOCAL_LDLIBS := -llog -lOpenSLES -landroid -latomic
LOCAL_SRC_FILES := AudioEngine.cpp \
    AudioSource.cpp

LOCAL_MODULE := SoundPoolCompat

include $(BUILD_SHARED_LIBRARY)