//
// Created by barikang on 2015. 12. 2..
//

#include "AudioPlayer.h"
#include "AudioEngine.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

using namespace SoundPoolCompat;
#define PCM_METADATA_VALUE_SIZE 32
#define EMPTY_BUFFER_SIZE 4096

void AudioPlayer::callback_SimpleBufferQueue(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    if (context) {
        AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
        if (pAudioPlayer->enqueueBuffer() == false) {
            //LOGD("[%d] enqueueBuffer end",pAudioPlayer->_streamID);
        }
    }
}

void AudioPlayer::callback_PlayerPlay(SLPlayItf caller, void* context, SLuint32 playEvent)
{
    if (context)
    {
        if((playEvent & SL_PLAYEVENT_HEADATEND) > 0) {
            AudioPlayer *pAudioPlayer = (AudioPlayer *) context;
            LOGD("[%d] callback playevent : end", pAudioPlayer->_streamID);
            auto pEngine = AudioEngine::getInstance();
            if (pEngine) {
                AudioTask task(SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_PLAYCOMPLETE,pAudioPlayer);
                pEngine->enqueueTask(task);
            }

        }
    }
}

void AudioPlayer::callback_PrefetchStatus(SLPrefetchStatusItf caller,void *context,SLuint32 event)
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
            if (pEngine) {
                AudioTask task(SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_PLAYERROR,pAudioPlayer);
                pEngine->enqueueTask(task);
            }
        }
    }

}


AudioPlayer::AudioPlayer(AudioEngine *pAudioEngine,int streamID,int streamGroupID)
        : _pAudioEngine(pAudioEngine)
        ,_streamID(streamID)
        ,_streamGroupID(streamGroupID)
        , _pAudioSrc(nullptr)

        , _itf_playerObject(nullptr)
        , _playOver(false)
        , _repeatCount(1)
        , _currentBufIndex(0)
        , _dupFD(0)
        , _isForDecoding(false)
        , _inited(false)

        , _itf_play(nullptr)
        , _itf_volume(nullptr)
        , _itf_bufferQueue(nullptr)
        , _itf_prefetchStatus(nullptr)
        , _itf_androidConfiguration(nullptr)
        , _itf_playerackRate(nullptr)
        , _itf_metadataExtraction(nullptr)
{
}

AudioPlayer::~AudioPlayer()
{

    if (_itf_playerObject)
    {
        (*_itf_playerObject)->Destroy(_itf_playerObject);
        _itf_playerObject = nullptr;
        _itf_play = nullptr;
        _itf_volume = nullptr;
        _itf_bufferQueue = nullptr;
        _itf_prefetchStatus = nullptr;
        _itf_androidConfiguration = nullptr;
        _itf_playerackRate = nullptr;
        _itf_metadataExtraction = nullptr;
    }

    if(_dupFD > 0)
    {
        close(_dupFD);
    }
    LOGD("[%d] AudioPlayer destoryed",_streamID);
}

SLresult AudioPlayer::initForPlay(const std::shared_ptr<AudioSource>& pAudioSrc)
{
    const SLEngineItf engineEngine = _pAudioEngine->_engineEngine;
    const SLObjectItf outputMixObject = _pAudioEngine->_outputMixObject;
    SLresult result = SL_RESULT_UNKNOWN_ERROR;
    _isForDecoding = false;

    do {

        SLDataSource audioSrc;
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq;
        SLDataFormat_PCM format_pcm;
        SLDataLocator_AndroidFD loc_fd;
        SLDataLocator_URI loc_uri;
        SLDataFormat_MIME format_mime;

        bool useBufferQueue = false;

        if (pAudioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            useBufferQueue = true;

            const SLuint32 bitPerSample = pAudioSrc->_pcm_bitPerSample;
            const SLuint32 samplingRate = pAudioSrc->_pcm_samplingRate;
            const SLuint32 numChannels = pAudioSrc->_pcm_numChannels;
            const SLuint32 containerSize = pAudioSrc->_pcm_containerSize;
            const SLuint32 byteOrder = pAudioSrc->_pcm_byteOrder;

            const SLuint32 speaker = numChannels == 1 ? SL_SPEAKER_FRONT_CENTER : (SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT);

            loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
            format_pcm = {SL_DATAFORMAT_PCM, numChannels,
                          (samplingRate * 1000),
                          bitPerSample, containerSize,
                          speaker, byteOrder};

            audioSrc = {&loc_bufq, &format_pcm};
        }
        else if (pAudioSrc->_type == AudioSource::AudioSourceType::FileDescriptor) {
            if(_dupFD > 0)
                close(_dupFD);
            _dupFD = dup(pAudioSrc->_fd);
            loc_fd = {SL_DATALOCATOR_ANDROIDFD, _dupFD,(SLAint64) pAudioSrc->_fd_offset,(SLAint64) pAudioSrc->_fd_length};
            format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};

            audioSrc = { &loc_fd,&format_mime };
        }
        else if (pAudioSrc->_type == AudioSource::AudioSourceType::Uri) {
            loc_uri = {SL_DATALOCATOR_URI , (SLchar*) pAudioSrc->_uri_path.c_str()};
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

        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &_itf_playerObject,
                                                    &audioSrc, &audioSnk, useBufferQueue ? 5 : 6, ids, req);
        if (SL_RESULT_SUCCESS != result) {
            //LOGE("create audio player fail");
            break;
        }

        // set configuration before realize
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_ANDROIDCONFIGURATION,
                                                    &_itf_androidConfiguration);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the config interface fail"); break; };
        result = (*_itf_androidConfiguration)->SetConfiguration(_itf_androidConfiguration, SL_ANDROID_KEY_STREAM_TYPE,
                                                                &_androidStreamType, sizeof(SLint32));

        // realize the player
        result = (*_itf_playerObject)->Realize(_itf_playerObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) { LOGE("realize the player fail"); break; }

        // get the play interface
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_PLAY, &_itf_play);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the play interface fail"); break; }

        (*_itf_play)->SetCallbackEventsMask(_itf_play, SL_PLAYEVENT_HEADATEND);
        result = (*_itf_play)->RegisterCallback(_itf_play, AudioPlayer::callback_PlayerPlay,
                                                this);
        if (SL_RESULT_SUCCESS != result) { LOGE("register play callback fail");break; }


        if (useBufferQueue)
        {
            // get the bufferqueue interface
            result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                        &_itf_bufferQueue);
            if (SL_RESULT_SUCCESS != result) {LOGE("get the bufferqueue interface fail");break; };

            // register callback on the buffer queue
            result = (*_itf_bufferQueue)->RegisterCallback(_itf_bufferQueue, AudioPlayer::callback_SimpleBufferQueue,this);
            if(SL_RESULT_SUCCESS != result){ LOGE("register buffer queue callback fail"); break; }

        }
        else
        {
            result = (*_itf_playerObject)->GetInterface(_itf_playerObject,SL_IID_PREFETCHSTATUS,&_itf_prefetchStatus);
            if (SL_RESULT_SUCCESS != result) {LOGE("get the prefetch interface fail");break; };

            result = (*_itf_prefetchStatus)->SetCallbackEventsMask(_itf_prefetchStatus,SL_PREFETCHEVENT_STATUSCHANGE|SL_PREFETCHEVENT_FILLLEVELCHANGE);
            if (SL_RESULT_SUCCESS != result) {LOGE("set prefetch callback event mask fail");break; };
            result = (*_itf_prefetchStatus)->SetFillUpdatePeriod(_itf_prefetchStatus,50);
            if (SL_RESULT_SUCCESS != result) {LOGE("set fillupdateperiod fail");break; };
            result = (*_itf_prefetchStatus)->RegisterCallback(_itf_prefetchStatus,AudioPlayer::callback_PrefetchStatus , this );
            if (SL_RESULT_SUCCESS != result) {LOGE("register prefetch callback fail");break; };

        }

        // get the playbackrate interface
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_PLAYBACKRATE, &_itf_playerackRate);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the playbackrate interface fail"); break; }

        // get the volume interface
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_VOLUME, &_itf_volume);
        if(SL_RESULT_SUCCESS != result){ LOGE("get the volume interface fail"); break; }

        _pAudioSrc = pAudioSrc;
        result = SL_RESULT_SUCCESS;
        _inited = true;
    } while (0);

    return result;
}


SLresult AudioPlayer::initForDecoding(const std::shared_ptr<AudioSource>& pAudioSrc)
{
    const SLEngineItf engineEngine = _pAudioEngine->_engineEngine;

    SLresult result = SL_RESULT_UNKNOWN_ERROR;
    _isForDecoding = true;


    do {
        if(pAudioSrc->_type == AudioSource::AudioSourceType::PCM)
        {
            break;
        }

        SLDataSource audioSrc;

        SLDataLocator_AndroidFD loc_fd;
        SLDataLocator_URI loc_uri;
        SLDataFormat_MIME format_mime;
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, (SLuint32) 1,
                                       (SLuint32) SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       16,SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};


        // configure audio sink
        SLDataSink audioSnk = {&loc_bufq, &format_pcm};

        if (pAudioSrc->_type == AudioSource::AudioSourceType::FileDescriptor) {
            if(_dupFD > 0)
                close(_dupFD);
            _dupFD = dup(pAudioSrc->_fd);
            loc_fd = {SL_DATALOCATOR_ANDROIDFD, _dupFD,(SLAint64) pAudioSrc->_fd_offset,(SLAint64) pAudioSrc->_fd_length};
            format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
            audioSrc = { &loc_fd,&format_mime };
        }
        else if (pAudioSrc->_type == AudioSource::AudioSourceType::Uri) {
            loc_uri = {SL_DATALOCATOR_URI , (SLchar*) pAudioSrc->_uri_path.c_str()};
            format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
            audioSrc = { &loc_uri,&format_mime};
        }
        else {
            assert(0);
        }


        // create audio player
        const SLInterfaceID ids[4] = {
                SL_IID_ANDROIDSIMPLEBUFFERQUEUE
                ,SL_IID_PLAY
                ,SL_IID_PREFETCHSTATUS
                ,SL_IID_METADATAEXTRACTION
        };
        const SLboolean req[4] = {
                SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
                ,SL_BOOLEAN_TRUE
        };

        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &_itf_playerObject,
                                                    &audioSrc, &audioSnk, 4, ids, req);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("create audio player fail");
            break;
        }

        // realize the player
        result = (*_itf_playerObject)->Realize(_itf_playerObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) { LOGE("realize the player fail"); break; }

        // get the play interface
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_PLAY, &_itf_play);
        if (SL_RESULT_SUCCESS != result) { LOGE("get the play interface fail"); break; }

        (*_itf_play)->SetCallbackEventsMask(_itf_play, SL_PLAYEVENT_HEADATEND);
        result = (*_itf_play)->RegisterCallback(_itf_play, AudioPlayer::callback_PlayerPlay,
                                                this);
        if (SL_RESULT_SUCCESS != result) { LOGE("register play callback fail");break; }

        // get the bufferqueue interface
        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                    &_itf_bufferQueue);
        if (SL_RESULT_SUCCESS != result) {LOGE("get the bufferqueue interface fail");break; };

        result = (*_itf_playerObject)->GetInterface(_itf_playerObject, SL_IID_METADATAEXTRACTION,
                                                    &_itf_metadataExtraction);
        if (SL_RESULT_SUCCESS != result) {LOGE("get the metadataextraction interface fail");break; };

        // register callback on the buffer queue
        result = (*_itf_bufferQueue)->RegisterCallback(_itf_bufferQueue, AudioPlayer::callback_SimpleBufferQueue,this);
        if(SL_RESULT_SUCCESS != result){ LOGE("register buffer queue callback fail"); break; }

        result = (*_itf_playerObject)->GetInterface(_itf_playerObject,SL_IID_PREFETCHSTATUS,&_itf_prefetchStatus);
        if (SL_RESULT_SUCCESS != result) {LOGE("get the prefetch interface fail");break; };

        result = (*_itf_prefetchStatus)->SetCallbackEventsMask(_itf_prefetchStatus,SL_PREFETCHEVENT_STATUSCHANGE|SL_PREFETCHEVENT_FILLLEVELCHANGE);
        if (SL_RESULT_SUCCESS != result) {LOGE("set prefetch callback event mask fail");break; };
        result = (*_itf_prefetchStatus)->SetFillUpdatePeriod(_itf_prefetchStatus,50);
        if (SL_RESULT_SUCCESS != result) {LOGE("set fillupdateperiod fail");break; };
        result = (*_itf_prefetchStatus)->RegisterCallback(_itf_prefetchStatus,AudioPlayer::callback_PrefetchStatus , this );
        if (SL_RESULT_SUCCESS != result) {LOGE("register prefetch callback fail");break; };

        ///////////////////////
        SLuint32 itemCount;
        result = (*_itf_metadataExtraction)->GetItemCount(_itf_metadataExtraction, &itemCount);
        if(SL_RESULT_SUCCESS != result){ LOGE("get meta item count fail"); break; }

        SLuint32 keySize, valueSize;
        SLMetadataInfo *keyInfo, *value;
        for(int i=0 ; i<itemCount ; i++) {
            keyInfo = NULL; keySize = 0;
            value = NULL;   valueSize = 0;
            result = (*_itf_metadataExtraction)->GetKeySize(_itf_metadataExtraction, i, &keySize);
            if(SL_RESULT_SUCCESS != result){ LOGE("getKeySize error"); return false; };
            result = (*_itf_metadataExtraction)->GetValueSize(_itf_metadataExtraction, i, &valueSize);
            if(SL_RESULT_SUCCESS != result){ LOGE("getValueSize error"); return false; };
            keyInfo = (SLMetadataInfo*) malloc(keySize);
            if (NULL != keyInfo) {
                result = (*_itf_metadataExtraction)->GetKey(_itf_metadataExtraction, i, keySize, keyInfo);
                if(SL_RESULT_SUCCESS != result){ LOGE("GetKey error"); return false; };

                /*
                  LOGD("key[%d] size=%d, name=%s \tvalue size=%d encoding=0x%X langCountry=%s\n",
                     i, (int) keyInfo->size, keyInfo->data,(int) valueSize, keyInfo->encoding,
                     keyInfo->langCountry);
                */
                if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_NUMCHANNELS)) {
                    _keyIdx_ChannelCount = i;
                } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_SAMPLERATE)) {
                    _keyIdx_SampleRate = i;
                } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_BITSPERSAMPLE)) {
                    _keyIdx_BitsPerSample = i;
                } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_CONTAINERSIZE)) {
                    _keyIdx_ContainerSize = i;
                } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_ENDIANNESS)) {
                    _keyIdx_Endianness = i;
                } else {
                    //LOGD("Unknown key %s ignored\n", (char *)keyInfo->data);
                }
                free(keyInfo);
            }
        }

        _pAudioSrc = pAudioSrc;
        result = SL_RESULT_SUCCESS;
        _inited = true;
    } while (0);

    return result;
}








bool AudioPlayer::enqueueBuffer() {
    if (_playOver || !_inited)
        return false;

    bool ret = false;
    if(_pAudioSrc != nullptr) {
        if (_isForDecoding == false  )
        {
            if(_pAudioSrc->_type == AudioSource::AudioSourceType::PCM)
            {
                auto pBuffer = _pAudioSrc->getPCMBuffer(_currentBufIndex);
                if (pBuffer != nullptr) {
                    _currentBufIndex++;
                    SLresult result = (*_itf_bufferQueue)->Enqueue(_itf_bufferQueue, pBuffer->ptr,
                                                                   pBuffer->size);
                    ret = SL_RESULT_SUCCESS == result;
                    if (SL_RESULT_SUCCESS != result) { LOGE("Enqueue error : %u", (unsigned int) result); };

                }
            }
            else{
                return true;
            }
        }
        else if (_isForDecoding == true )
        {
            auto pBuffer = _pAudioSrc->addEmptyPCMBuffer(EMPTY_BUFFER_SIZE);
            SLresult result = (*_itf_bufferQueue)->Enqueue(_itf_bufferQueue, pBuffer->ptr,
                                                           pBuffer->size);
            if (SL_RESULT_SUCCESS != result) { LOGE("Enqueue error : %u", (unsigned int) result); };
            ret = (SL_RESULT_SUCCESS == result);
        }
    }

    return ret;

}

void AudioPlayer::resetBuffer()
{
    if (_playOver || !_inited)
        return;

    if(_pAudioSrc != nullptr && _pAudioSrc->_type == AudioSource::AudioSourceType::PCM)
    {
        _currentBufIndex = 0;
        auto result = (*_itf_bufferQueue)->Clear(_itf_bufferQueue);
        if(SL_RESULT_SUCCESS != result) { LOGE("Queue clear error : %u",(unsigned int)result); };
    }

}


void AudioPlayer::setVolume(float volume)
{
    _volume = volume;
    if (_playOver || !_inited)
        return;

    if(volume < 0.0f )
        volume = 0.0f;
    else if (volume > 1.0f)
        volume = 1.0f;

    int dbVolume = 2000 * log10(volume);
    if(dbVolume < SL_MILLIBEL_MIN){
        dbVolume = SL_MILLIBEL_MIN;
    }
    if(_itf_volume) {
        auto result = (*_itf_volume)->SetVolumeLevel(_itf_volume, dbVolume);
        if(SL_RESULT_SUCCESS != result) { LOGE("[%d] setVolume error",_streamID); };
    }
}

void AudioPlayer::setPlayRate(float rate)
{
    _playRate = rate;
    if (_playOver || !_inited)
        return;

    if(_itf_playerackRate)
    {
        SLpermille playRate = (SLpermille)(rate*1000);
        auto result = (*_itf_playerackRate)->SetRate(_itf_playerackRate,playRate);
        if(SL_RESULT_SUCCESS != result) { LOGE("[%d] setRate error",_streamID); };
    }
}

void AudioPlayer::setRepeatCount(int loop)
{
    _repeatCount = loop;

}

void AudioPlayer::setAndroidStreamType(SLint32 androidStreamType)
{
    _androidStreamType = androidStreamType;
}

bool AudioPlayer::play() {
    if (_playOver || !_inited)
        return false;

    resetBuffer();
    if(!enqueueBuffer() || !enqueueBuffer())
        return false;

    auto result = (*_itf_play)->SetPlayState(_itf_play, SL_PLAYSTATE_PLAYING);
    if (SL_RESULT_SUCCESS != result) {
        LOGE("SetPlayState play fail (play)");
        return false;
    }

    return true;
}

void AudioPlayer::pause()
{
    if (_playOver || !_inited)
        return;

    auto result = (*_itf_play)->SetPlayState(_itf_play, SL_PLAYSTATE_PAUSED);
    if(SL_RESULT_SUCCESS != result){
        LOGE("SetPlayState pause fail (resume)");
    }
}

void AudioPlayer::resume()
{
    if (_playOver || !_inited)
        return;

    auto result = (*_itf_play)->SetPlayState(_itf_play, SL_PLAYSTATE_PLAYING);
    if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState play fail (resume)"); };

}

void AudioPlayer::stop()
{
    if (_playOver || !_inited)
        return;



    auto result = (*_itf_play)->SetPlayState(_itf_play, SL_PLAYSTATE_STOPPED);
    if(SL_RESULT_SUCCESS != result){ LOGE("SetPlayState stop fail "); };
    if(_isForDecoding) {
        AudioSource::DecodingState expectedState = AudioSource::DecodingState::DecodingNow;
        if (_pAudioSrc->_decodingState.compare_exchange_strong(expectedState,
                                                             AudioSource::DecodingState::Completed)) {
            LOGD("%d AudioSource decoding stopped", _pAudioSrc->_audioID);

        }
    }
    _playOver = true;
}

void AudioPlayer::fillOutPCMInfo()
{
    auto pAudioSrc = _pAudioSrc;
    union {
        SLMetadataInfo pcmMetaData;
        char withData[PCM_METADATA_VALUE_SIZE];
    } u;
    auto res = (*_itf_metadataExtraction)->GetValue(_itf_metadataExtraction, _keyIdx_SampleRate,
                                                    PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);
    pAudioSrc->_pcm_samplingRate = *((SLuint32*)u.pcmMetaData.data);

    res = (*_itf_metadataExtraction)->GetValue(_itf_metadataExtraction, _keyIdx_ChannelCount,
                                               PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);
    pAudioSrc->_pcm_numChannels = *((SLuint32*)u.pcmMetaData.data);

    res = (*_itf_metadataExtraction)->GetValue(_itf_metadataExtraction, _keyIdx_BitsPerSample,
                                               PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);
    pAudioSrc->_pcm_bitPerSample = *((SLuint32*)u.pcmMetaData.data);

    res = (*_itf_metadataExtraction)->GetValue(_itf_metadataExtraction, _keyIdx_ContainerSize,
                                               PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);

    pAudioSrc->_pcm_containerSize = *((SLuint32*)u.pcmMetaData.data);

    res = (*_itf_metadataExtraction)->GetValue(_itf_metadataExtraction, _keyIdx_Endianness,
                                               PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);
    pAudioSrc->_pcm_byteOrder = *((SLuint32*)u.pcmMetaData.data);

    if(_itf_play) {
        SLmillisecond duration;
        res = (*_itf_play)->GetDuration(_itf_play,&duration);
        if(res == SL_RESULT_SUCCESS && duration > 0)
        {
            pAudioSrc->setPCMDuration(duration);

        }
    }


}

float AudioPlayer::getCurrentTime()
{
    if(_inited && _itf_play) {
        SLmillisecond currPos;
        (*_itf_play)->GetPosition(_itf_play, &currPos);
        return currPos / 1000.0f;
    }
    return -1.0f;

}