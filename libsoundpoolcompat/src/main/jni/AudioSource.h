//
// Created by barikang on 2015. 11. 27..
//

#ifndef SOUNDPOOLCOMPAT_AUDIOSOURCE_H
#define SOUNDPOOLCOMPAT_AUDIOSOURCE_H

#include "utils.h"

namespace SoundPoolCompat {
    // DataBuffer
    class DataBuffer {
    public:
        void *ptr;
        int size;

    public:
        DataBuffer(int size);
        DataBuffer(void *ptr,int offset,int size);
        ~DataBuffer();
    };

    // AudioSource
    class AudioSource {
    public:
        enum AudioSourceType {
            NotDefined,
            PCM,
            FileDescriptor,
            Uri,
        };
        enum DecodingState {
            DecodingNow,
            Completed
        };

    private:
        AudioSource();
    public:
        ~AudioSource();
        std::shared_ptr<DataBuffer> getPCMBuffer(int idx);
        std::shared_ptr<DataBuffer> addEmptyPCMBuffer(int size);
        void closeFD();


        AudioSourceType _type;
        std::vector<std::shared_ptr<DataBuffer> > _pcm_nativeBuffers;
        int _pcm_numChannels;
        int _pcm_samplingRate;
        int _pcm_bitPerSample;
        int _pcm_containerSize;
        int _pcm_byteOrder;


        int _fd;
        int64_t _fd_offset;
        int64_t _fd_length;

        std::string _uri_path;
        std::atomic<DecodingState> _decodingState;
        int _audioID;

    private:
        static int g_currentAudioId;
        static std::mutex g_mutex;
        static std::unordered_map<int, std::shared_ptr<AudioSource> > g_id2source;
    public:
        static int createAudioSource();
        static void releaseAudioSource(int audioID);
        static std::shared_ptr<AudioSource> getSharedPtrAudioSource(int audioID);

        static bool setAudioSourceFileDescriptor(int audioID,int fd,int64_t offset,int64_t length);
        static bool setAudioSourceURI(int audioID,const std::string& uri);

    };


}
#endif //SOUNDPOOLCOMPAT_AUDIOSOURCE_H
