//
// Created by barikang on 2015. 11. 26..
//
#ifndef SOUNDPOOLCOMPAT_UTILS_H
#define SOUNDPOOLCOMPAT_UTILS_H

#include <android/log.h>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <assert.h>
#include <jni.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <unistd.h>
#include <chrono>
#include <deque>

#define SOUNDPOOLCOMPAT_TAG "SoundPoolCompat"

#ifdef SOUNDPOOLCOMPAT_DEBUG
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,SOUNDPOOLCOMPAT_TAG,__VA_ARGS__)
#else
#define LOGD(...)
#endif
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,SOUNDPOOLCOMPAT_TAG,__VA_ARGS__)
#define CHECK_SL_RESULT(x) if(x != SL_RESULT_SUCCESS) { LOGE("%s:%s(%d) failed",__FILE__,__func__,__LINE__); }


#endif //SOUNDPOOLCOMPAT_UTILS_H
