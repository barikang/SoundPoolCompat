#include "AudioEngine.h"
#include "kr_co_smartstudy_soundpoolcompat_AudioEngine.h"
#include "AudioPlayer.h"
#include "utils.h"
#include <inttypes.h>

#define DELAY_TIME_TO_REMOVE    (0.5f)
#define MAX_AUDIOPLAYER_COUNT   (28)

//
// Created by barikang on 2015. 11. 24..
//
using namespace SoundPoolCompat;

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
    while(true)
    {
        int streamID = -1;
        {
            std::unique_lock<std::mutex> lock(pAudioEngine->_queueMutex);

            while(!pAudioEngine->_released && pAudioEngine->_queueFinishedStreamID.empty())
                pAudioEngine->_threadCondition.wait(lock);

            if(pAudioEngine->_released)
                return;

            streamID = pAudioEngine->_queueFinishedStreamID.front();
            pAudioEngine->_queueFinishedStreamID.pop_front();
        }
        // dodo~

        auto pAudioPlayer = pAudioEngine->getAudioPlayer(streamID);
        if(pAudioPlayer)
        {
            pAudioPlayer->_repeatCount--;
            bool doStop = true;
            if(pAudioPlayer->_repeatCount != 0)
            {
                do {
                    if(!pAudioPlayer->play())
                        break;
                    doStop = false;

                } while(0);
            }

            if(doStop)
            {
                pAudioEngine->stop(streamID);
            }

        }
    }

    LOGD("AudioEngine worker thread is finished");
}

void AudioEngine::enqueueFinishedPlay(int streamID)
{
    {
        std::unique_lock<std::mutex> lock(_queueMutex);
        _queueFinishedStreamID.push_back(streamID);
    }
    _threadCondition.notify_one();
}

AudioEngine::AudioEngine()
: _currentAudioStreamID(1)
, _engineObject(nullptr)
, _engineEngine(nullptr)
, _outputMixObject(nullptr)
, _released(false)
, _thread(nullptr)
,_currentAudioPlayerCount(0)
{

}
AudioEngine::~AudioEngine()
{
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
        if(pAudioSrc == nullptr)
            break;

        if(!incAudioPlayerCount()) {
            LOGD("Skip play. exceed max audioplayer count. AudioID : %d",audioID);
            break;
        }
        streamID = _currentAudioStreamID.fetch_add(1);

        std::shared_ptr<AudioPlayer> pPlayer(new AudioPlayer(this));
        auto initPlayer = pPlayer->init(streamID,_engineEngine, _outputMixObject,pAudioSrc,androidStreamType,streamGroupID);
        if (!initPlayer){
            LOGE( "%s,%d message:create player fail", __func__, __LINE__);
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

bool AudioEngine::incAudioPlayerCount()
{
    bool overMax = false;
    for(;;) {
        int cnt = _currentAudioPlayerCount.load();
        if(cnt >= MAX_AUDIOPLAYER_COUNT) {
            overMax = true;
            break;
        }
        if(_currentAudioPlayerCount.compare_exchange_weak(cnt,cnt+1))
            break;
    }

    return !overMax;
}
void AudioEngine::decAudioPlayerCount()
{
    _currentAudioPlayerCount.fetch_sub(1);

}


int AudioEngine::releaseUnusedAudioPlayer()
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    const double currentTime = AudioEngine::getCurrentTime();
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end();)
    {
        auto pAudioPlayer = iter->second;
        if(pAudioPlayer->_playOver && currentTime-pAudioPlayer->_stoppedTime  > DELAY_TIME_TO_REMOVE)
        {
            iter = _audioPlayers.erase(iter);
            continue;
        }
        iter++;
    }
    return (int)_audioPlayers.size();
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
    //double maxRemoveTime = 0;
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end(); ) {
        auto pPlayer = iter->second;
        if (streamGroupID == 0 || pPlayer->_streamGroupID == streamGroupID) {
            //removeStreamIDS.push_back(iter->first);
            pPlayer->stop();
            iter = _audioPlayers.erase(iter);
            continue;
        }
        iter++;
    }
    /*
    if(wait) {
        double remainTime = maxRemoveTime - AudioEngine::getCurrentTime();
        if(remainTime > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(remainTime*1000)));
        }
    }
     */

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
