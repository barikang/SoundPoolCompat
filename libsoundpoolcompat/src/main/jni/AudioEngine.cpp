#include "AudioEngine.h"
#include "kr_co_smartstudy_soundpoolcompat_AudioEngine.h"
#include "AudioEngine.h"
#include "utils.h"
#include <inttypes.h>

//
// Created by barikang on 2015. 11. 24..
//

using namespace SoundPoolCompat;

#define DELAY_TIME_TO_REMOVE 0.5f


void bqAudioPlayer_Callback_SimpleBufferQueue(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    AudioPlayer* pAudioPlayer = (AudioPlayer*)context;
    if(pAudioPlayer->enqueueBuffer() == false)
    {
        LOGD("enqueueBuffer end");
    }
}

void bqAudioPlayer_Callback_PlayerPlay(SLPlayItf caller, void* context, SLuint32 playEvent)
{
    if (context && (playEvent & SL_PLAYEVENT_HEADATEND) > 0)
    {
        LOGD("got playevent : end");
        AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
        pAudioPlayer->_playOver = true;
        pAudioPlayer->_stoppedTime = AudioEngine::getCurrentTime();
    }

}

//void bgPlayerPCM_Callback_


AudioPlayer::AudioPlayer()
        : _fdPlayerObject(nullptr)
        , _playOver(false)
        , _loop(false)
        , _streamID(0)
        , _stoppedTime(0.0f)
        , _currentBufIndex(0)
{

}

AudioPlayer::~AudioPlayer()
{
    if (_fdPlayerObject)
    {
        (*_fdPlayerObject)->Destroy(_fdPlayerObject);
        _fdPlayerObject = nullptr;
        _fdPlayerPlay = nullptr;
        _fdPlayerVolume = nullptr;
        _fdPlayerBufferQueue = nullptr;
        _fdPlayerPrefetchStatus = nullptr;
        _fdPlayerConfig = nullptr;
    }
}

bool AudioPlayer::init(SLEngineItf engineEngine, SLObjectItf outputMixObject,
                            std::shared_ptr<AudioSource> pAudioSrc,
                           float volume, bool loop)
{
    bool ret = false;
    if(volume < 0.0f )
        volume = 0.0f;
    else if (volume > 1.0f)
        volume = 1.0f;

    do {
        _audioSrc = pAudioSrc;
        _loop = loop;

        if (_audioSrc->_type == AudioSource::AudioSourceType::PCM) {
            SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
                    SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
            SLuint32 containerSize = 8;
            const int bitPerSample = _audioSrc->_pcm_bitPerSample;
            const int samplingRate = _audioSrc->_pcm_samplingRate;
            const int numChannels = _audioSrc->_pcm_numChannels;
            if (bitPerSample == 8)
                containerSize = 8;
            else if (bitPerSample == 16)
                containerSize = 16;
            else if (bitPerSample > 16)
                containerSize = 32;
            else {
                LOGD("Not suppoert bitPerSample %d", bitPerSample);
            }
            SLuint32 speaker = numChannels == 1 ? SL_SPEAKER_FRONT_CENTER : SL_SPEAKER_FRONT_LEFT |
                                                                            SL_SPEAKER_FRONT_RIGHT;
            SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, (SLuint32) numChannels,
                                           (SLuint32) (samplingRate * 1000),
                                           (SLuint32) bitPerSample, containerSize,
                                           speaker, SL_BYTEORDER_LITTLEENDIAN};
            SLDataSource audioSrc = {&loc_bufq, &format_pcm};

            // configure audio sink
            SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
            SLDataSink audioSnk = {&loc_outmix, NULL};

            // create audio player
            const SLInterfaceID ids[4] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_VOLUME,
                                          SL_IID_PLAY, SL_IID_ANDROIDCONFIGURATION};
            const SLboolean req[4] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                                      SL_BOOLEAN_TRUE};
            auto result = (*engineEngine)->CreateAudioPlayer(engineEngine, &_fdPlayerObject,
                                                             &audioSrc, &audioSnk, 4, ids, req);
            if (SL_RESULT_SUCCESS != result) {
                LOGE("create audio player fail");
                break;
            }

        }
        else if (_audioSrc->_type == AudioSource::AudioSourceType::FileDescriptor) {

        }
        else if (_audioSrc->_type == AudioSource::AudioSourceType::Uri) {

        }

        // set configuration before realize
        auto result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_ANDROIDCONFIGURATION,
                                                       &_fdPlayerConfig);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the config interface fail"); break; };
        SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
        (*_fdPlayerConfig)->SetConfiguration(_fdPlayerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                                             &streamType, sizeof(SLint32));

        // realize the player
        result = (*_fdPlayerObject)->Realize(_fdPlayerObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) { LOGE("realize the player fail"); break; }

        // get the play interface
        result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_PLAY, &_fdPlayerPlay);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the play interface fail"); break; }

        (*_fdPlayerPlay)->SetCallbackEventsMask(_fdPlayerPlay, SL_PLAYEVENT_HEADATEND);
        result = (*_fdPlayerPlay)->RegisterCallback(_fdPlayerPlay, bqAudioPlayer_Callback_PlayerPlay,
                                                    this);
        if (SL_RESULT_SUCCESS != result) { LOGE("register play callback fail");break; }


        if (_audioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            // get the bufferqueue interface
            result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                      &_fdPlayerBufferQueue);
            if (SL_RESULT_SUCCESS != result) {LOGE("get the bufferqueue interface fail");break; };

            // register callback on the buffer queue
            result = (*_fdPlayerBufferQueue)->RegisterCallback(_fdPlayerBufferQueue, bqAudioPlayer_Callback_SimpleBufferQueue,this);
            if(SL_RESULT_SUCCESS != result){ LOGE("register buffer queue callback fail"); break; }

        }

        // get the volume interface
        result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_VOLUME, &_fdPlayerVolume);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the volume interface fail"); break; }


        /*
        if (loop){
            (*_fdPlayerSeek)->SetLoop(_fdPlayerSeek, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
        }
         */

        int dbVolume = 2000 * log10(volume);
        if(dbVolume < SL_MILLIBEL_MIN){
            dbVolume = SL_MILLIBEL_MIN;
        }
        (*_fdPlayerVolume)->SetVolumeLevel(_fdPlayerVolume, dbVolume);
        result = (*_fdPlayerPlay)->SetPlayState(_fdPlayerPlay, SL_PLAYSTATE_PLAYING);
        if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState fail"); break; }

        if (_audioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            enqueueBuffer();
        }

        ret = true;
    } while (0);

    return ret;
}

bool AudioPlayer::enqueueBuffer()
{
    bool ret = false;
    if(_audioSrc != nullptr && _currentBufIndex < _audioSrc->_pcm_nativeBuffers.size())
    {
        //LOGD("enqueueBuffer %d/%d",_currentBufIndex,(int)_nativeBufIds.size());

        auto pBuffer = _audioSrc->getPCMBuffer(_currentBufIndex);
        if(pBuffer != nullptr)
        {
            _currentBufIndex++;
            SLresult result = (*_fdPlayerBufferQueue)->Enqueue(_fdPlayerBufferQueue, pBuffer->ptr, pBuffer->size);
            ret = SL_RESULT_SUCCESS == result;
            if(SL_RESULT_SUCCESS != result) { LOGE("Enqueue error : %u",(unsigned int)result); };
        }
    }
    return ret;

}


///////////////////////////////

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
        (JNIEnv *env, jclass clasz, jint audioID, jboolean loop, jfloat volume)
{
    jint ret = -1;
    auto pEngine = AudioEngine::getInstance();
    if(pEngine)
    {
        ret = pEngine->playAudio(audioID,loop,volume);
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




AudioEngine::AudioEngine()
: _currentAudioStreamID(1)
, _engineObject(nullptr)
, _engineEngine(nullptr)
, _outputMixObject(nullptr)
{

}
AudioEngine::~AudioEngine()
{
    _audioPlayers.clear();

    if (_outputMixObject)
    {
        (*_outputMixObject)->Destroy(_outputMixObject);
    }
    if (_engineObject)
    {
        (*_engineObject)->Destroy(_engineObject);
    }

    LOGD("released AudioEngine");

}

AudioPlayer* AudioEngine::getAudioPlayer(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto iter = _audioPlayers.find(streamID);
    if(iter != _audioPlayers.end())
    {
        return &iter->second;
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

    return ret;

}
int AudioEngine::playAudio(int audioID,bool loop ,float volume)
{
    int streamID = -1;

    do
    {
        if (_engineEngine == nullptr)
            break;

        LOGD("playAudio");
        releaseUnusedAudioPlayer();

        auto pAudioSrc = AudioSource::getSharedPtrAudioSource(audioID);
        const int newStreamID = _currentAudioStreamID.fetch_add(1);
        AudioPlayer *pPlayer = nullptr;
        _recurMutex.lock();
        pPlayer = &_audioPlayers[newStreamID];
        _recurMutex.unlock();
        //auto& player = _audioPlayers[newStreamID];
        auto initPlayer = pPlayer->init(_engineEngine, _outputMixObject,pAudioSrc,volume, loop);
        if (!initPlayer){
            _recurMutex.lock();
            _audioPlayers.erase(newStreamID);
            _recurMutex.unlock();
            LOGE( "%s,%d message:create player fail", __func__, __LINE__);
            break;
        }
        streamID = newStreamID;
        pPlayer->_streamID = streamID;

    } while (0);

    return streamID;

}

void AudioEngine::releaseUnusedAudioPlayer()
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    const double currentTime = AudioEngine::getCurrentTime();
    for(auto iter = _audioPlayers.begin() ; iter != _audioPlayers.end();)
    {
        if(iter->second._playOver && currentTime-iter->second._stoppedTime  > DELAY_TIME_TO_REMOVE)
        {
            iter = _audioPlayers.erase(iter);
            continue;
        }
        iter++;
    }
}


void AudioEngine::setVolume(int streamID,float volume)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        int dbVolume = 2000 * log10(volume);
        if (dbVolume < SL_MILLIBEL_MIN) {
            dbVolume = SL_MILLIBEL_MIN;
        }
        auto result = (*pPlayer->_fdPlayerVolume)->SetVolumeLevel(pPlayer->_fdPlayerVolume, dbVolume);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("%s error:%u", __func__, (unsigned int) result);
        }
    }

}
void AudioEngine::pause(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        auto result = (*pPlayer->_fdPlayerPlay)->SetPlayState(pPlayer->_fdPlayerPlay,SL_PLAYSTATE_PAUSED);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("%s error:%u", __func__, (unsigned int) result);
        }
    }

}
void AudioEngine::resume(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        auto result = (*pPlayer->_fdPlayerPlay)->SetPlayState(pPlayer->_fdPlayerPlay,SL_PLAYSTATE_PLAYING);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("%s error:%u", __func__, (unsigned int) result);
        }
    }

}
void AudioEngine::stop(int streamID)
{
    std::lock_guard<std::recursive_mutex> guard(_recurMutex);
    auto pPlayer = getAudioPlayer(streamID);
    if(pPlayer) {
        auto result = (*pPlayer->_fdPlayerPlay)->SetPlayState(pPlayer->_fdPlayerPlay, SL_PLAYSTATE_STOPPED);
        if(SL_RESULT_SUCCESS != result){
            LOGE("%s error:%u",__func__, (unsigned int)result);
        }

        /*If destroy openSL object immediately,it may cause dead lock.
         *It's a system issue.For more information:
         *    https://github.com/cocos2d/cocos2d-x/issues/11697
         *    https://groups.google.com/forum/#!msg/android-ndk/zANdS2n2cQI/AT6q1F3nNGIJ
         */
        pPlayer->_stoppedTime = AudioEngine::getCurrentTime();
        pPlayer->_playOver = true;
        //_audioPlayers.erase(streamID);
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
