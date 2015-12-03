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
    public:
        AudioPlayer();
        ~AudioPlayer();

        bool init(int streamID,SLEngineItf engineEngine, SLObjectItf outputMixObject,
                  std::shared_ptr<AudioSource> pAudioSrc,
                  SLint32 androidStreamType,int streamGroupID);

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

        std::shared_ptr<AudioSource> _audioSrc;


        volatile int _currentBufIndex;

        friend class AudioEngine;
    };

}
#endif //SOUNDPOOLCOMPAT_AUDIOPLAYER_H
