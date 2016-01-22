//
// Created by barikang on 2015. 12. 2..
//

#ifndef SOUNDPOOLCOMPAT_AUDIOPLAYER_H
#define SOUNDPOOLCOMPAT_AUDIOPLAYER_H

#include "utils.h"
#include "AudioSource.h"

namespace SoundPoolCompat {

    class AudioEngine;

    enum AudioPlayerState
    {
        Paused,
        Playing,
        Stopped,

    };

    class AudioPlayer {
    private:
        static void callback_SimpleBufferQueue(SLAndroidSimpleBufferQueueItf bq, void *context);
        static void callback_PlayerPlay(SLPlayItf caller, void* context, SLuint32 playEvent);
        static void callback_PrefetchStatus(SLPrefetchStatusItf caller,void *context,SLuint32 event);

    public:
        AudioPlayer(AudioEngine *pAudioEngine,int streamID,int streamGroupID);
        ~AudioPlayer();

        SLresult initForPlay(const std::shared_ptr<AudioSource>& pAudioSrc);
        SLresult initForDecoding(const std::shared_ptr<AudioSource>& pAudioSrc);

        bool enqueueBuffer();
        void resetBuffer();
        void setVolume(float volume);
        void setPlayRate(float rate);
        void setRepeatCount(int loop);
        void setAndroidStreamType(SLint32 androidStreamType);
        void setPosition(int milli);
        int  getPosition();
        bool play();
        void pause();
        void resume();
        void stop();
        void fillOutPCMInfo();
        float getCurrentTime();

        bool isForDecoding() { return _isForDecoding; };

    private:
        int _streamID;
        int _streamGroupID;

        int _repeatCount;
        float _volume;
        float _playRate;

        volatile bool _inited;

        SLObjectItf _itf_playerObject;
        SLPlayItf _itf_play;
        SLAndroidSimpleBufferQueueItf _itf_bufferQueue;
        SLPrefetchStatusItf _itf_prefetchStatus;
        SLVolumeItf _itf_volume;
        SLAndroidConfigurationItf _itf_androidConfiguration;
        SLPlaybackRateItf _itf_playerackRate;
        SLMetadataExtractionItf _itf_metadataExtraction;
        SLSeekItf _itf_seek;

        std::shared_ptr<AudioSource> _pAudioSrc;
        int _dupFD;
        AudioEngine *_pAudioEngine;
        int _currentBufIndex;
        bool _isForDecoding;
        bool _doPlayEndCallBack;
        AudioPlayerState _audioPlayerState;

        SLint32 _androidStreamType;

        int _keyIdx_ChannelCount = -1;
        int _keyIdx_SampleRate = -1;
        int _keyIdx_BitsPerSample = -1;
        int _keyIdx_ContainerSize = -1;
        int _keyIdx_Endianness = -1;

        std::recursive_mutex _recurMutex;

        friend class AudioEngine;
        friend class AudioTask;
    };

}

#endif //SOUNDPOOLCOMPAT_AUDIOPLAYER_H
