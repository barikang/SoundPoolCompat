//
// Created by barikang on 2015. 11. 27..
//

#include "AudioSource.h"
#include "kr_co_smartstudy_soundpoolcompat_AudioSource.h"

using namespace SoundPoolCompat;

DataBuffer::DataBuffer(int _size)
{
    this->ptr = malloc(_size);
    this->size = _size;
    memset(this->ptr,0,_size);
}

DataBuffer::DataBuffer(void *_ptr,int _offset,int _size)
{
    this->ptr = malloc(_size);
    this->size = _size;
    memcpy(this->ptr,((char*)_ptr)+_offset,_size );
}

DataBuffer::~DataBuffer()
{
    if(this->ptr)
        free(this->ptr);
    this->ptr = nullptr;
}

int AudioSource::g_currentAudioId = 1;
std::mutex AudioSource::g_mutex;
std::unordered_map<int, std::shared_ptr<AudioSource> > AudioSource::g_id2source;
///


std::shared_ptr<DataBuffer> AudioSource::getPCMBuffer(int idx)
{
    if(0 <= idx && idx < _pcm_nativeBuffers.size())
        return _pcm_nativeBuffers[idx];
    return nullptr;

}

std::shared_ptr<DataBuffer> AudioSource::addEmptyPCMBuffer(int size)
{
    std::shared_ptr<DataBuffer> ret(new DataBuffer(size));
    _pcm_nativeBuffers.push_back(ret);
    return ret;
}
void AudioSource::setPCMDuration(int durationMilli)
{
    _pcm_duration = durationMilli;
    if(_pcm_duration > 0) {
        const int needSize = int(_pcm_samplingRate * _pcm_duration / 1000) * _pcm_containerSize *
                             _pcm_numChannels / 8;
        int bufSizeSum = 0;
        for (size_t i = 0; i < _pcm_nativeBuffers.size(); i++) {
            auto pBuffer = getPCMBuffer(i);
            bufSizeSum += pBuffer->size;
            if (bufSizeSum > needSize) {
                pBuffer->size = pBuffer->size - (bufSizeSum - needSize);
                _pcm_nativeBuffers.resize(i + 1);
                break;
            }
        }
    }

}

// statics...

int AudioSource::createAudioSource()
{
    std::shared_ptr<AudioSource> audioSrc(new AudioSource());
    std::lock_guard<std::mutex> guard(g_mutex);
    const int audioID = g_currentAudioId++;
    audioSrc->_audioID = audioID;
    g_id2source[audioID] = audioSrc;
    return audioID;
}

void AudioSource::releaseAudioSource(int audioID)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    g_id2source.erase(audioID);
}

std::shared_ptr<AudioSource> AudioSource::getSharedPtrAudioSource(int audioID)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    auto iter = g_id2source.find(audioID);
    if(iter != g_id2source.end())
        return iter->second;
    LOGE("AudioID : %d is not exist",audioID);
    return nullptr;
}


bool AudioSource::setAudioSourceFileDescriptor(int audioID,int fd,int64_t offset,int64_t length)
{
    if(fd < 0 || length <= 0)
    {
        LOGE("setAudioSourceFileDescriptor : invalid fd: %d , length = %ld",fd,(long)length);
        return false;
    }
    auto audioSrc = AudioSource::getSharedPtrAudioSource(audioID);
    if(audioSrc != nullptr && audioSrc->_type == AudioSourceType::NotDefined)
    {
        audioSrc->_type = AudioSourceType::FileDescriptor;
        audioSrc->_fd = dup(fd);
        audioSrc->_fd_offset = offset;
        audioSrc->_fd_length = length;
        audioSrc->_decodingState.store(AudioSource::DecodingState::Completed);
        LOGD("Setting fd : %d offset %d  leng %d",audioSrc->_fd,(int)offset,(int)length);
        return true;
    }

    return false;
}

bool AudioSource::setAudioSourceURI(int audioID,const std::string& uri)
{
    auto audioSrc = AudioSource::getSharedPtrAudioSource(audioID);
    if(audioSrc != nullptr && audioSrc->_type == AudioSourceType::NotDefined)
    {
        audioSrc->_type = AudioSourceType::Uri;
        audioSrc->_uri_path = uri;
        audioSrc->_decodingState.store(AudioSource::DecodingState::Completed);
        return true;
    }

    return false;

}


AudioSource::AudioSource()
        : _type(AudioSourceType::NotDefined)
        ,_pcm_numChannels(0)
        ,_pcm_samplingRate(0)
        ,_pcm_bitPerSample(0)
        ,_pcm_containerSize(0)
        ,_pcm_byteOrder(0)
        ,_pcm_duration(-1)
        ,_fd(0)
        ,_fd_offset(0)
        ,_fd_length(0)
        ,_decodingState(DecodingState::Completed)
        ,_audioID(-1)
{
}

AudioSource::~AudioSource()
{
    LOGD("[%d] AudioSource destoryed",_audioID);
    closeFD();
}

void AudioSource::closeFD()
{
    if(_fd > 0)
    {
        close(_fd);
        _fd = 0;
    }

}

//////////////////////////////////////////////////////////////////////////////////////////

/*
* Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
* Method:    nativeCreateAudioSource
* Signature: ()I
*/
JNIEXPORT jint JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeCreateAudioSource
        (JNIEnv *env, jclass clasz)
{
    return AudioSource::createAudioSource();
}

/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeReleaseAudioSource
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeReleaseAudioSource
        (JNIEnv *env, jclass clasz, jint audioID)
{
    AudioSource::releaseAudioSource(audioID);
}


// function contents
static int jniGetFDFromFileDescriptor(JNIEnv * env, jobject fileDescriptor) {
    jint fd = -1;
    jclass fdClass = env->FindClass("java/io/FileDescriptor");

    if (fdClass != NULL) {
        jfieldID fdClassDescriptorFieldID = env->GetFieldID(fdClass, "descriptor", "I");
        if (fdClassDescriptorFieldID != NULL && fileDescriptor != NULL) {
            fd = env->GetIntField(fileDescriptor, fdClassDescriptorFieldID);
        }
    }

    return fd;
}
/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeSetAudioSourceFileDescriptor
 * Signature: (ILjava/io/FileDescriptor;JJZ)Z
 */
JNIEXPORT jboolean JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeSetAudioSourceFileDescriptor
        (JNIEnv *env, jclass clasz, jint audioID, jobject fd, jlong offset, jlong length)
{
    return AudioSource::setAudioSourceFileDescriptor(audioID,jniGetFDFromFileDescriptor(env, fd),int64_t(offset),int64_t(length));
}

/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeSetAudioSourceURI
 * Signature: (ILjava/lang/String;)Z
 */
JNIEXPORT jboolean JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeSetAudioSourceURI
        (JNIEnv *env, jclass clasz, jint audioID, jstring uri)
{
    const char* nativeUri = env->GetStringUTFChars(uri, 0);
    jboolean ret = AudioSource::setAudioSourceURI(audioID,nativeUri);
    env->ReleaseStringUTFChars(uri, nativeUri);
    return ret;
}
