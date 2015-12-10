//
// Created by barikang on 2015. 11. 24..
//

#ifndef SOUNDPOOLCOMPAT_AUDIOENGINE_H
#define SOUNDPOOLCOMPAT_AUDIOENGINE_H

#include "utils.h"
#include "AudioSource.h"
#include "AudioPlayer.h"

namespace SoundPoolCompat {

#define SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_PLAYFINISHEDNOTI         (1)
#define SOUNDPOOLCOMPAT_AUDIOTASK_TYPE_DECODE                    (2)

    struct AudioTask
    {

        int taskType;
        int audioID = -1;
        int streamID = -1;
        int streamGroupID = -1;

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
        int playAudio(int audioID,int repeatCount ,float volume,SLint32 androidStreamType,int streamGroupID,float playRate);
        int decodeAudio(int audioID,int streamGroupID);
        void pause(int streamID);
        void pauseAll(int streamGroupID);
        void resume(int streamID);
        void resumeAll(int streamGroupID);
        void stop(int streamID);
        float getCurrentTime(int streamID);
        void stopAll(int streamGroupID,bool wait);
        void enqueueFinishedPlay(int streamID);
        void setVolume(int streamID,float volume);
        void setPlayRate(int streamID,float playRate);
        void setRepeatCount(int streamID,int repeatCount);
    private:
        std::shared_ptr<AudioPlayer>  getAudioPlayer(int streamID);

        void onPlayComplete(int streamID);

        bool init();
        static void threadFunc(AudioEngine* audioEngine);
        bool incAudioPlayerCount();
        void decAudioPlayerCount();


        // engine interfaces
        SLObjectItf _engineObject;
        SLEngineItf _engineEngine;

        // output mix interfaces
        SLObjectItf _outputMixObject;

        //streamID,AudioPlayer
        std::unordered_map<int, std::shared_ptr<AudioPlayer> >  _audioPlayers;

        std::atomic<int> _currentAudioStreamID;
        std::atomic<int> _currentAudioPlayerCount;
        std::recursive_mutex _recurMutex;

        std::mutex _queueMutex;
        std::deque<AudioTask> _queueTask;
        std::condition_variable _threadCondition;
        std::thread* _thread;
        volatile bool _released;

        friend class AudioPlayer;
    };

}


#endif //SOUNDPOOLCOMPAT_AUDIOENGINE_H
