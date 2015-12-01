//
// Created by barikang on 2015. 11. 24..
//

#ifndef SOUNDPOOLCOMPAT_AUDIOENGINE_H
#define SOUNDPOOLCOMPAT_AUDIOENGINE_H

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
                  float volume, int repeatCount,SLint32 androidStreamType);

        bool enqueueBuffer();
        void resetBuffer();

        int _streamID;
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

        std::shared_ptr<AudioSource> _audioSrc;


        volatile int _currentBufIndex;

        friend class AudioEngine;
    };

class AudioEngine
{
public:
    static std::shared_ptr<AudioEngine> getInstance();
    static void initialize();
    static void release();
    static double getCurrentTime();
private:
    static std::shared_ptr<AudioEngine> g_audioEngine;
    static int g_refCount;
    static std::mutex g_mutex;


private:
    AudioEngine();
public:
    ~AudioEngine();

public:
    int playAudio(int audioID,int repeatCount ,float volume,SLint32 androidStreamType);
    void setVolume(int streamID,float volume);
    void pause(int streamID);
    void resume(int streamID);
    void stop(int streamID);
    float getCurrentTime(int streamID);
    void stopAll(bool wait);
    void enqueueFinishedPlay(int streamID);
private:
    AudioPlayer*  getAudioPlayer(int streamID);
    int releaseUnusedAudioPlayer();

    bool init();
    static void threadFunc(AudioEngine* audioEngine);


    // engine interfaces
    SLObjectItf _engineObject;
    SLEngineItf _engineEngine;

    // output mix interfaces
    SLObjectItf _outputMixObject;

    //streamID,AudioPlayer
    std::unordered_map<int, AudioPlayer >  _audioPlayers;

    std::atomic<int> _currentAudioStreamID;
    std::recursive_mutex _recurMutex;

    std::mutex _queueMutex;
    std::deque<int> _queueFinishedStreamID;
    std::condition_variable _threadCondition;
    std::thread* _thread;
    volatile bool _released;

};

}


#endif //SOUNDPOOLCOMPAT_AUDIOENGINE_H
