//
// Created by barikang on 2015. 12. 2..
//

#ifndef SOUNDPOOLCOMPAT_AUDIOPLAYER_H
#define SOUNDPOOLCOMPAT_AUDIOPLAYER_H

#include "utils.h"
#include "AudioSource.h"

namespace SoundPoolCompat {

    class AudioEngine;

    class AudioPlayer {
    private:
        static void callback_SimpleBufferQueue(SLAndroidSimpleBufferQueueItf bq, void *context);
        static void callback_PlayerPlay(SLPlayItf caller, void* context, SLuint32 playEvent);
        static void callback_PrefetchStatus(SLPrefetchStatusItf caller,void *context,SLuint32 event);

    public:
        AudioPlayer(AudioEngine *pAudioEngine);
        ~AudioPlayer();

        bool initForPlay(int streamID,SLEngineItf engineEngine, SLObjectItf outputMixObject,
                  std::shared_ptr<AudioSource> pAudioSrc,
                  SLint32 androidStreamType,int streamGroupID);

        bool initForDecoding(int streamID,SLEngineItf engineEngine,
                             std::shared_ptr<AudioSource> pAudioSrc,
                             int streamGroupID);


        bool enqueueBuffer();
        void resetBuffer();
        void setVolume(float volume);
        void setPlayRate(float rate);
        void setRepeatCount(int loop);
        bool play();
        void pause();
        void resume();
        void stop();



        int _streamID;
        int _streamGroupID;
        bool _playOver;
        double _stoppedTime;
        int _repeatCount;
        SLPlayItf _fdPlayerPlay;

    private:
        SLObjectItf _fdPlayerObject;
        SLAndroidSimpleBufferQueueItf _fdPlayerBufferQueue;
        SLPrefetchStatusItf _fdPlayerPrefetchStatus;
        SLVolumeItf _fdPlayerVolume;
        SLAndroidConfigurationItf _fdPlayerConfig;
        SLPlaybackRateItf _fdPlayerPlayRate;
        SLMetadataExtractionItf _fdPlayerMetaExtract;

        std::shared_ptr<AudioSource> _audioSrc;
        int _dupFD;
        AudioEngine *_pAudioEngine;
        int _currentBufIndex;
        bool _isForDecoding;

        int _channelCountKeyIndex = -1;
        int _sampleRateKeyIndex = -1;
        int _bitsPerSampleKeyIndex = -1;
        int _containerSizeKeyIndex = -1;
        int _channelMaskKeyIndex = -1;
        int _endiannessKeyIndex = -1;

        friend class AudioEngine;
    };

}

#endif //SOUNDPOOLCOMPAT_AUDIOPLAYER_H
