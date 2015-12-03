//
// Created by barikang on 2015. 12. 2..
//

#include "AudioPlayer.h"
#include "AudioEngine.h"

using namespace SoundPoolCompat;


void bqAudioPlayer_Callback_SimpleBufferQueue(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    if (context) {
        AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
        if (pAudioPlayer->enqueueBuffer() == false) {
            //LOGD("[%d] enqueueBuffer end",pAudioPlayer->_streamID);
        }
    }
}

void bqAudioPlayer_Callback_PlayerPlay(SLPlayItf caller, void* context, SLuint32 playEvent)
{
    if (context)
    {
        if((playEvent & SL_PLAYEVENT_HEADATEND) > 0) {
            AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
            LOGD("[%d] callback playevent : end", pAudioPlayer->_streamID);
            auto pEngine = AudioEngine::getInstance();
            if (pEngine)
                pEngine->enqueueFinishedPlay(pAudioPlayer->_streamID);
        }
    }
}

void bqAudioPlayer_Callback_PrefetchStatus(SLPrefetchStatusItf caller,void *context,SLuint32 event)
{
    if(context)
    {
        SLuint32 status = 0;
        SLpermille fillLevel = 0;
        (*caller)->GetPrefetchStatus(caller,&status);
        (*caller)->GetFillLevel(caller,&fillLevel);

        AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
        //LOGD("PrefetchStatus streamID : %d filllevel %d status %d",pAudioPlayer->_streamID,(int)fillLevel,(int)status);
        if( (status & SL_PREFETCHSTATUS_UNDERFLOW) && fillLevel == 0)
        {
            LOGD("Prefetch error! StreamID = %d",pAudioPlayer->_streamID);
            auto pEngine = AudioEngine::getInstance();
            if (pEngine)
                pEngine->enqueueFinishedPlay(pAudioPlayer->_streamID);
        }
    }

}


AudioPlayer::AudioPlayer()
        : _fdPlayerObject(nullptr)
        , _playOver(false)
        , _repeatCount(0)
        , _streamID(0)
        , _stoppedTime(0.0f)
        , _currentBufIndex(0)
        , _dupFD(0)
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
        _fdPlayerPlayRate = nullptr;

    }

    if(_dupFD > 0)
    {
        close(_dupFD);
    }
    LOGD("[%d] AudioPlayer destoryed",_streamID);
}

bool AudioPlayer::init(int streamID,SLEngineItf engineEngine, SLObjectItf outputMixObject,
                       std::shared_ptr<AudioSource> pAudioSrc,
                       SLint32 androidStreamType,int streamGroupID)
{
    LOGD("[%d] Player Init : ",streamID);
    bool ret = false;
    _streamID = streamID;
    _streamGroupID = streamGroupID;

    do {
        _audioSrc = pAudioSrc;

        SLDataSource audioSrc;
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq;
        SLDataFormat_PCM format_pcm;
        SLDataLocator_AndroidFD loc_fd;
        SLDataLocator_URI loc_uri;
        SLDataFormat_MIME format_mime;

        bool useBufferQueue = false;

        if (_audioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            useBufferQueue = true;

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
            SLuint32 speaker = numChannels == 1 ? SL_SPEAKER_FRONT_CENTER : (SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT);

            loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
            format_pcm = {SL_DATAFORMAT_PCM, (SLuint32) numChannels,
                          (SLuint32) (samplingRate * 1000),
                          (SLuint32) bitPerSample, containerSize,
                          speaker, SL_BYTEORDER_LITTLEENDIAN};

            audioSrc = {&loc_bufq, &format_pcm};
        }
        else if (_audioSrc->_type == AudioSource::AudioSourceType::FileDescriptor) {

            _dupFD = dup(_audioSrc->_fd);
            loc_fd = {SL_DATALOCATOR_ANDROIDFD, _dupFD,(SLAint64)_audioSrc->_fd_offset,(SLAint64)_audioSrc->_fd_length};
            format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};

            audioSrc = { &loc_fd,&format_mime };
        }
        else if (_audioSrc->_type == AudioSource::AudioSourceType::Uri) {
            loc_uri = {SL_DATALOCATOR_URI , (SLchar*)_audioSrc->_uri_path.c_str()};
            format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
            audioSrc = { &loc_uri,&format_mime};
        }
        else {
            assert(0);
        }

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID ids[6] = {
                SL_IID_ANDROIDSIMPLEBUFFERQUEUE
                ,SL_IID_VOLUME
                ,SL_IID_PLAY
                ,SL_IID_ANDROIDCONFIGURATION
                ,SL_IID_PLAYBACKRATE
                ,SL_IID_PREFETCHSTATUS
        };
        const SLboolean req[6] = {
                useBufferQueue ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
        };

        auto result = (*engineEngine)->CreateAudioPlayer(engineEngine, &_fdPlayerObject,
                                                         &audioSrc, &audioSnk, useBufferQueue ? 5 : 6, ids, req);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("create audio player fail");
            break;
        }

        // set configuration before realize
        result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_ANDROIDCONFIGURATION,
                                                  &_fdPlayerConfig);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the config interface fail"); break; };
        (*_fdPlayerConfig)->SetConfiguration(_fdPlayerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                                             &androidStreamType, sizeof(SLint32));

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


        if (useBufferQueue)
        {
            // get the bufferqueue interface
            result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                      &_fdPlayerBufferQueue);
            if (SL_RESULT_SUCCESS != result) {LOGE("get the bufferqueue interface fail");break; };

            // register callback on the buffer queue
            result = (*_fdPlayerBufferQueue)->RegisterCallback(_fdPlayerBufferQueue, bqAudioPlayer_Callback_SimpleBufferQueue,this);
            if(SL_RESULT_SUCCESS != result){ LOGE("register buffer queue callback fail"); break; }

        }
        else
        {
            result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject,SL_IID_PREFETCHSTATUS,&_fdPlayerPrefetchStatus);
            if (SL_RESULT_SUCCESS != result) {LOGE("get the prefetch interface fail");break; };

            result = (*_fdPlayerPrefetchStatus)->SetCallbackEventsMask(_fdPlayerPrefetchStatus,SL_PREFETCHEVENT_STATUSCHANGE|SL_PREFETCHEVENT_FILLLEVELCHANGE);
            if (SL_RESULT_SUCCESS != result) {LOGE("set prefetch callback event mask fail");break; };
            result = (*_fdPlayerPrefetchStatus)->SetFillUpdatePeriod(_fdPlayerPrefetchStatus,50);
            if (SL_RESULT_SUCCESS != result) {LOGE("set fillupdateperiod fail");break; };
            result = (*_fdPlayerPrefetchStatus)->RegisterCallback(_fdPlayerPrefetchStatus,bqAudioPlayer_Callback_PrefetchStatus , this );
            if (SL_RESULT_SUCCESS != result) {LOGE("register prefetch callback fail");break; };

        }

        // get the playbackrate interface
        result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_PLAYBACKRATE, &_fdPlayerPlayRate);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the playbackrate interface fail"); break; }

        // get the volume interface
        result = (*_fdPlayerObject)->GetInterface(_fdPlayerObject, SL_IID_VOLUME, &_fdPlayerVolume);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the volume interface fail"); break; }

        ret = true;
    } while (0);

    return ret;
}

bool AudioPlayer::enqueueBuffer() {
    if (_playOver)
        return false;

    bool ret = false;
    if (_audioSrc != nullptr)
    {
        if(_audioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            if(_currentBufIndex < _audioSrc->_pcm_nativeBuffers.size())
            {
                auto pBuffer = _audioSrc->getPCMBuffer(_currentBufIndex);
                if (pBuffer != nullptr) {
                    _currentBufIndex++;
                    SLresult result = (*_fdPlayerBufferQueue)->Enqueue(_fdPlayerBufferQueue, pBuffer->ptr,
                                                                       pBuffer->size);
                    ret = SL_RESULT_SUCCESS == result;
                    if (SL_RESULT_SUCCESS != result) { LOGE("Enqueue error : %u", (unsigned int) result); };
                }
            }
        }
        else{
            return true;
        }

    }

    return ret;

}

void AudioPlayer::resetBuffer()
{
    if(_playOver)
        return;

    if(_audioSrc != nullptr && _audioSrc->_type == AudioSource::AudioSourceType::PCM)
    {
        _currentBufIndex = 0;
        auto result = (*_fdPlayerBufferQueue)->Clear(_fdPlayerBufferQueue);
        if(SL_RESULT_SUCCESS != result) { LOGE("Queue clear error : %u",(unsigned int)result); };
    }

}


void AudioPlayer::setVolume(float volume)
{
    if(_playOver)
        return;

    if(volume < 0.0f )
        volume = 0.0f;
    else if (volume > 1.0f)
        volume = 1.0f;

    int dbVolume = 2000 * log10(volume);
    if(dbVolume < SL_MILLIBEL_MIN){
        dbVolume = SL_MILLIBEL_MIN;
    }
    if(_fdPlayerVolume) {
        auto result = (*_fdPlayerVolume)->SetVolumeLevel(_fdPlayerVolume, dbVolume);
        if(SL_RESULT_SUCCESS != result) { LOGE("[%d] setVolume error",_streamID); };
    }
}

void AudioPlayer::setPlayRate(float rate)
{
    if(_playOver)
        return;

    if(_fdPlayerPlayRate)
    {
        SLpermille playRate = (SLpermille)(rate*1000);
        auto result = (*_fdPlayerPlayRate)->SetRate(_fdPlayerPlayRate,playRate);
        if(SL_RESULT_SUCCESS != result) { LOGE("[%d] setRate error",_streamID); };
    }
}

void AudioPlayer::setRepeatCount(int loop)
{
    _repeatCount = loop;

}

bool AudioPlayer::play()
{
    if(_playOver)
        return false;

    resetBuffer();
    auto result = (*_fdPlayerPlay)->SetPlayState(_fdPlayerPlay, SL_PLAYSTATE_PLAYING);
    if(SL_RESULT_SUCCESS != result){
        LOGE("SetPlayState play fail (play)");
        return false;
    }
    else {
        if(!enqueueBuffer())
            return false;
    }
    return true;
}

void AudioPlayer::pause()
{
    if(_playOver)
        return;

    auto result = (*_fdPlayerPlay)->SetPlayState(_fdPlayerPlay, SL_PLAYSTATE_PAUSED);
    if(SL_RESULT_SUCCESS != result){
        LOGE("SetPlayState pause fail (resume)");
    }
}

void AudioPlayer::resume()
{
    if(_playOver)
        return;

    auto result = (*_fdPlayerPlay)->SetPlayState(_fdPlayerPlay, SL_PLAYSTATE_PLAYING);
    if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState play fail (resume)"); };

}

void AudioPlayer::stop()
{
    if(_playOver)
        return;

    auto result = (*_fdPlayerPlay)->SetPlayState(_fdPlayerPlay, SL_PLAYSTATE_STOPPED);
    if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState stop fail "); };

    if(_playOver == false) {
        _stoppedTime = AudioEngine::getCurrentTime();
        _playOver = true;
    }



}
