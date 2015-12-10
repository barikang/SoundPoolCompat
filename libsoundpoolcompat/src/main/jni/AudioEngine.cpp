#include "AudioEngine.h"
#include "kr_co_smartstudy_soundpoolcompat_AudioEngine.h"
#include "AudioPlayer.h"
#include "utils.h"
#include <inttypes.h>
#include <queue>

#define MAX_AUDIOPLAYER_COUNT   (28)
#define  CLASS_NAME "kr/co/smartstudy/soundpoolcompat/AudioEngine"

//
// Created by barikang on 2015. 11. 24..
//
using namespace SoundPoolCompat;

//JNIHelp
typedef struct JniMethodInfo_
{
    jclass      classID;
    jmethodID   methodID;
} JniMethodInfo;

extern "C"
{
static JavaVM *gJavaVM = nullptr;
static JniMethodInfo callbackMethodInfo;

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    gJavaVM = vm;
    JNIEnv* env;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_4) != JNI_OK) {
        LOGD("GETENVFAILEDONLOAD");
        return -1;
    }

    jclass clazz = env->FindClass(CLASS_NAME);
    callbackMethodInfo.methodID = env->GetStaticMethodID(clazz,"onDecodingComplete", "(III)V");
    callbackMethodInfo.classID = (jclass) env->NewGlobalRef(clazz);

    return JNI_VERSION_1_4;
}


// get env and cache it
static JNIEnv *getJNIEnv(void) {
    JavaVM *jvm = gJavaVM;
    if (NULL == jvm) {
        return NULL;
    }

    JNIEnv *env = NULL;
    // get jni environment
    jint ret = jvm->GetEnv((void **) &env, JNI_VERSION_1_4);

    switch (ret) {
        case JNI_OK :
            // Success!
            return env;

        case JNI_EDETACHED :
            // Thread not attached

            // TODO : If calling AttachCurrentThread() on a native thread
            // must call DetachCurrentThread() in future.
            // see: http://developer.android.com/guide/practices/design/jni.html
            if (jvm->AttachCurrentThread(&env, NULL) < 0) {
                LOGD("Failed to get the environment using AttachCurrentThread()");
                return NULL;
            } else {
                // Success : Attached and obtained JNIEnv!
                return env;
            }

        case JNI_EVERSION :
            // Cannot recover from this error
            LOGD("JNI interface version 1.4 not supported");
        default :
            LOGD("Failed to get the environment using GetEnv()");
            return NULL;
    }
}

}
///////////////////////////////////////////////////////////////////


std::shared_ptr<AudioEngine> AudioEngine::g_audioEngine(nullptr);
int AudioEngine::g_refCount = 0;
std::mutex AudioEngine::g_mutex;

std::shared_ptr<AudioEngine> AudioEngine::getInstance()
{
    return g_audioEngine;
}
void AudioEngine::initialize() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if(g_audioEngine == nullptr) {
        g_audioEngine = std::shared_ptr<AudioEngine>(new AudioEngine());
        g_audioEngine->init();
        LOGD("initlized AudioEngine");
    }
    g_refCount++;
}

void AudioEngine::release() {
    std::lock_guard<std::mutex> guard(g_mutex);

    if(g_audioEngine){
        g_refCount--;
        if(g_refCount == 0) {
            g_audioEngine.reset();
        }
    }
}

double AudioEngine::getCurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    return (double)tv.tv_sec + (double)tv.tv_usec/1000000;
}



void AudioEngine::threadFunc(AudioEngine* pAudioEngine)
{
    LOGD("AudioEngine worker thread start");

    auto compareFunc = [](AudioTask a, AudioTask b) {
        if(a.taskType == b.taskType)
        {
            if(a.streamID == b.streamID)
            {
                return a.audioID > b.audioID;
            }
            return a.streamID > b.streamID;
        }
        return a.taskType > b.taskType;
    };

    std::priority_queue<AudioTask, std::vector<AudioTask>, decltype(compareFunc)> priorAudioTaskQueue(compareFunc);


    while(true)
    {
        {
            std::unique_lock<std::mutex> lock(pAudioEngine->_queueMutex);

            while(!pAudioEngine->_released && pAudioEngine->_queueTask.empty())
                pAudioEngine->_threadCondition.wait(lock);

            if(pAudioEngine->_released)
                return;

            while(!pAudioEngine->_queueTask.empty()) {
                priorAudioTaskQueue.push(pAudioEngine->_queueTask.front());
                pAudioEngine->_queueTask.pop_front();
            }
        }

        while(!priorAudioTaskQueue.empty())
        {
            AudioTask task = priorAudioTaskQueue.top();
            priorAudioTaskQueue.pop();

            if(task.taskType == SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_PLAYFINISHEDNOTI)
            {
                auto pAudioPlayer = pAudioEngine->getAudioPlayer(task.streamID);
                if(pAudioPlayer)
                {
                    pAudioPlayer->_repeatCount--;
                    bool doStop = true;
                    if(pAudioPlayer->_isForDecoding == false) {
                        if (pAudioPlayer->_repeatCount != 0) {
                            do {
                                if (!pAudioPlayer->play())
                                    break;
                                doStop = false;

                            } while (0);
                        }
                    }

                    if(doStop)
                    {
                        pAudioEngine->onPlayComplete(task.streamID);
                    }

                }
            }
            else if(task.taskType == SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_DECODE)
            {
                auto pAudioSrc = AudioSource::getSharedPtrAudioSource(task.audioID);
                if(pAudioSrc == nullptr)
                    continue;

                AudioSource::DecodingState expectedState = AudioSource::DecodingState::None;
                if(!pAudioSrc->_decodingState.compare_exchange_weak(expectedState,AudioSource::DecodingState::DecodingNow))
                    continue;

                pAudioSrc->_pcm_nativeBuffers.clear();

                const int newStreamID = pAudioEngine->_currentAudioStreamID.fetch_add(1);

                std::shared_ptr<AudioPlayer> pPlayer(new AudioPlayer(pAudioEngine));
                SLresult  resultInitPlayer = false;
                resultInitPlayer = pPlayer->initForDecoding(newStreamID,pAudioEngine->_engineEngine,pAudioSrc,task.streamGroupID);

                // Try it next;
                if(resultInitPlayer == SL_RESULT_MEMORY_FAILURE)
                {
                    pAudioSrc->_decodingState.store(AudioSource::DecodingState::None);
                    priorAudioTaskQueue.push(task);
                    break;
                }

                if (resultInitPlayer != SL_RESULT_SUCCESS){
                    LOGE( "%s,%d message:create player fail : %x", __func__, __LINE__,resultInitPlayer);
                    continue;
                }
                pAudioEngine->_recurMutex.lock();
                pAudioEngine->_audioPlayers[newStreamID] = pPlayer;
                pAudioEngine->_recurMutex.unlock();

                pPlayer->enqueueBuffer();
                pPlayer->enqueueBuffer();
                pPlayer->play();



            }

            // if succ

        }
    }

    JNIEnv *env = getJNIEnv();
    gJavaVM->DetachCurrentThread();

    LOGD("AudioEngine worker thread is finished");
}

void AudioEngine::enqueueTask(const AudioTask& task)
{
    std::unique_lock<std::mutex> lock(_queueMutex);
    _queueTask.push_back(task);
    _threadCondition.notify_one();

}

AudioEngine::AudioEngine()
        : _currentAudioStreamID(1)
        , _engineObject(nullptr)
        , _engineEngine(nullptr)
        , _outputMixObject(nullptr)
        , _released(false)
        , _thread(nullptr)
{

}
AudioEngine::~AudioEngine()
{
    LOGD("AudioEngine release start");
    _released = true;
    if(_thread) {
        _threadCondition.notify_one();
        _thread->join();
        delete _thread;
        _thread = nullptr;
    }


    stopAll(0,true);

    if (_outputMixObject)
    {
        (*_outputMixObject)->Destroy(_outputMixObject);
    }
    if (_engineObject)
    {
        (*_engineObject)->Destroy(_engineObject);
    }

    LOGD("AudioEngine released");

}

std::shared_ptr<AudioPlayer> AudioEngine::getAudioPlayer(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto iter = _audioPlayers.find(streamID);
    if(iter != _audioPlayers.end())
    {
        return iter->second;
    }
    return nullptr;

}

bool AudioEngine::init()
{
    bool ret = false;
    do{
        // create engine
        auto result = slCreateEngine(&_engineObject, 0, nullptr, 0, nullptr, nullptr);
        if(SL_RESULT_SUCCESS != result){ LOGE("create opensl engine fail"); break; }

        // realize the engine
        result = (*_engineObject)->Realize(_engineObject, SL_BOOLEAN_FALSE);
        if(SL_RESULT_SUCCESS != result){ LOGE("realize the engine fail"); break; }

        // get the engine interface, which is needed in order to create other objects
        result = (*_engineObject)->GetInterface(_engineObject, SL_IID_ENGINE, &_engineEngine);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the engine interface fail"); break; }

        // create output mix
        const SLInterfaceID outputMixIIDs[] = {};
        const SLboolean outputMixReqs[] = {};
        result = (*_engineEngine)->CreateOutputMix(_engineEngine, &_outputMixObject, 0, outputMixIIDs, outputMixReqs);
        if(SL_RESULT_SUCCESS != result){ LOGE("create output mix fail"); break; }

        // realize the output mix
        result = (*_outputMixObject)->Realize(_outputMixObject, SL_BOOLEAN_FALSE);
        if(SL_RESULT_SUCCESS != result){ LOGE("realize the output mix fail"); break; }


        ret = true;
    }while (false);

    if(ret) {
        _thread = new std::thread(AudioEngine::threadFunc,this);
    }

    return ret;

}

int AudioEngine::playAudio(int audioID,int repeatCount ,float volume,SLint32 androidStreamType,int streamGroupID,float playRate)
{
    int streamID = -1;

    do
    {
        if (_engineEngine == nullptr)
            break;

        auto pAudioSrc = AudioSource::getSharedPtrAudioSource(audioID);
        auto decodingState = pAudioSrc->_decodingState.load();
        if(pAudioSrc == nullptr || decodingState == AudioSource::DecodingState::DecodingNow)
            break;

        streamID = _currentAudioStreamID.fetch_add(1);

        std::shared_ptr<AudioPlayer> pPlayer(new AudioPlayer(this));
        SLresult resultInitPlayer = pPlayer->initForPlay(streamID,_engineEngine, _outputMixObject,pAudioSrc,androidStreamType,streamGroupID);
        if (resultInitPlayer != SL_RESULT_SUCCESS){
            LOGE( "%s,%d message:create player fail : %x", __func__, __LINE__,resultInitPlayer);
            streamID = -1;
            break;
        }
        _recurMutex.lock();
        _audioPlayers[streamID] = pPlayer;
        _recurMutex.unlock();

        if(repeatCount == 0)
            repeatCount = 1;

        pPlayer->setVolume(volume);
        pPlayer->setPlayRate(playRate);
        pPlayer->setRepeatCount(repeatCount);
        pPlayer->play();


    } while (0);

    return streamID;

}

int AudioEngine::decodeAudio(int audioID,int streamGroupID)
{
    AudioTask task(SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_DECODE,audioID,-1,streamGroupID);
    enqueueTask(task);
    return 0;
}



void AudioEngine::pause(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->pause();
    }

}
void AudioEngine::resume(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->resume();
    }

}
void AudioEngine::stop(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->stop();
        _audioPlayers.erase(streamID);
    }
}

float AudioEngine::getCurrentTime(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    SLmillisecond currPos;
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        (*pPlayer->_fdPlayerPlay)->GetPosition(pPlayer->_fdPlayerPlay, &currPos);
        return currPos / 1000.0f;
    }

}

void AudioEngine::stopAll(int streamGroupID,bool wait)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end(); ) {
        auto pPlayer = iter->second;
        if (streamGroupID == 0 || pPlayer->_streamGroupID == streamGroupID) {
            pPlayer->stop();
            iter = _audioPlayers.erase(iter);
            continue;
        }
        iter++;
    }

}

void AudioEngine::pauseAll(int streamGroupID) {
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end(); iter++) {
        auto pPlayer = iter->second;
        if (pPlayer->_streamGroupID == streamGroupID) {
            pPlayer->pause();
        }
    }

}

void AudioEngine::resumeAll(int streamGroupID) {
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end(); iter++) {
        auto pPlayer = iter->second;
        if (pPlayer->_streamGroupID == streamGroupID) {
            pPlayer->resume();
        }
    }
}

void AudioEngine::setVolume(int streamID,float volume)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->setVolume(volume);
    }

}
void AudioEngine::setPlayRate(int streamID,float playRate)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->setPlayRate(playRate);
    }

}
void AudioEngine::setRepeatCount(int streamID,int repeatCount)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        pPlayer->setRepeatCount(repeatCount);
    }

}

////////////////////////////////////////////////////


void AudioEngine::onPlayComplete(int streamID)
{

    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        if(pPlayer->_isForDecoding) {
            auto pAudioSrc = pPlayer->_audioSrc;
            pAudioSrc->_type = AudioSource::AudioSourceType::PCM;
            pPlayer->fillOutPCMInfo();
            pAudioSrc->_decodingState.store(AudioSource::DecodingState::Completed);
            pAudioSrc->closeFD();
        }

        {
            std::lock_guard <std::recursive_mutex> guard(_recurMutex);
            pPlayer->stop();
            _audioPlayers.erase(streamID);
        }

        if(pPlayer->_isForDecoding) {
            JNIEnv *env = getJNIEnv();
            env->CallStaticVoidMethod(callbackMethodInfo.classID,callbackMethodInfo.methodID, pPlayer->_streamGroupID,pPlayer->_streamID,0);
            JavaVM *jvm = gJavaVM;
            //jvm->DetachCurrentThread();

        }

    }


}



JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeInitilizeAudioEngine
        (JNIEnv *env, jclass clasz)
{

    AudioEngine::initialize();
}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeReleaseAudioEngine
        (JNIEnv *env, jclass clasz)
{
    AudioEngine::release();
}

JNIEXPORT jint JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativePlayAudio
        (JNIEnv *env, jclass clasz, jint audioID, jint repeatCount, jfloat volume,jint androidStreamType,jint streamGroupID, jfloat playRate)
{
    jint ret = -1;
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        ret = pEngine->playAudio(audioID,repeatCount,volume,(SLint32)androidStreamType,streamGroupID,playRate);
    }

    return ret;

}


JNIEXPORT jint JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeDecodeAudio
        (JNIEnv *env, jclass clasz, jint audioID, jint streamGroupID)
{
    jint ret = -1;
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        ret = pEngine->decodeAudio(audioID,streamGroupID);
    }

    return ret;
}



JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativePause
        (JNIEnv *env, jclass clasz, jint streamID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->pause(streamID);
    }

}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeStop
        (JNIEnv *env, jclass clasz, jint streamID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->stop(streamID);
    }

}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeResume
        (JNIEnv *env, jclass clasz, jint streamID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->resume(streamID);
    }

}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativePauseAll
        (JNIEnv *, jclass, jint streamGroupID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->pauseAll(streamGroupID);
    }
}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeResumeAll
        (JNIEnv *, jclass, jint streamGroupID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->resumeAll(streamGroupID);
    }
}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeStopAll
        (JNIEnv *, jclass, jint streamGroupID)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->stopAll(streamGroupID,false);
    }

}
JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeSetVolume
        (JNIEnv *, jclass, jint streamID, jfloat volume)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->setVolume(streamID,volume);
    }


}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeSetPlayRate
        (JNIEnv *, jclass, jint streamID, jfloat playRate)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->setPlayRate(streamID,playRate);
    }

}

JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioEngine_nativeSetRepeatCount
        (JNIEnv *, jclass, jint streamID, jint repeatCount)
{
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        pEngine->setRepeatCount(streamID,repeatCount);
    }

}
