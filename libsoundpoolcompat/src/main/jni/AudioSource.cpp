//
// Created by barikang on 2015. 11. 27..
//

#include "AudioSource.h"
#include "kr_co_smartstudy_soundpoolcompat_AudioSource.h"

using namespace SoundPoolCompat;

PCMBuffer::PCMBuffer(void *_ptr,int _offset,int _size)
{
    this->ptr = malloc(_size);
    this->size = _size;
    memcpy(this->ptr,((char*)_ptr)+_offset,_size );
}

PCMBuffer::~PCMBuffer()
{
    if(this->ptr)
        free(this->ptr);
    this->ptr = nullptr;
}

int AudioSource::g_currentAudioId = 1;
std::mutex AudioSource::g_mutex;
std::unordered_map<int, std::shared_ptr<AudioSource> > AudioSource::g_id2source;
///


std::shared_ptr<PCMBuffer> AudioSource::getPCMBuffer(size_t idx)
{
    if(idx < _pcm_nativeBuffers.size())
        return _pcm_nativeBuffers[idx];
    return nullptr;

}

// statics...

int AudioSource::createAudioSource()
{
    std::shared_ptr<AudioSource> audioSrc(new AudioSource());
    std::lock_guard<std::mutex> guard(g_mutex);
    const int audioID = g_currentAudioId++;
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

bool AudioSource::setAudioSourcePCM(int audioID,int numChannels,int samplingRate,int bitPerSample)
{
    auto audioSrc = AudioSource::getSharedPtrAudioSource(audioID);
    if(audioSrc != nullptr && audioSrc->_type == AudioSourceType::NotDefined)
    {
        audioSrc->_type = AudioSourceType::PCM;
        audioSrc->_pcm_numChannels = numChannels;
        audioSrc->_pcm_samplingRate = samplingRate;
        audioSrc->_pcm_bitPerSample = bitPerSample;
        return true;
    }
    return false;
}

bool AudioSource::addPCMBuffer(int audioID,void* pBuf,int offset,int size) {
    if (pBuf == nullptr || size <= 0) {
        LOGD("addPCMBuffer error : null pointer or size is 0");
        return false;
    }

    auto audioSrc = getSharedPtrAudioSource(audioID);
    if(audioSrc != nullptr)
    {
        std::shared_ptr<PCMBuffer> pcmBuf(new PCMBuffer(pBuf,offset,size));
        audioSrc->_pcm_nativeBuffers.push_back(pcmBuf);
        return true;
    }
    return false;

}

bool AudioSource::setAudioSourceFileDescriptor(int audioID,int fd,int64_t offset,int64_t length,bool autoclose )
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
        audioSrc->_fd = fd;
        audioSrc->_fd_offset = offset;
        audioSrc->_fd_length = length;
        audioSrc->_fd_autoclose = autoclose;
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
        return true;
    }

    return false;

}


AudioSource::AudioSource()
        : _type(AudioSourceType::NotDefined)
        ,_pcm_numChannels(0)
        ,_pcm_samplingRate(0)
        ,_pcm_bitPerSample(0)
        ,_fd(0)
        ,_fd_offset(0)
        ,_fd_length(0)
        ,_fd_autoclose(true)
{
}

AudioSource::~AudioSource()
{
    if(_fd > 0 && _fd_autoclose)
    {
        close(_fd);
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

/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeSetAudioSourcePCM
 * Signature: (IIII)Z
 */
JNIEXPORT jboolean JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeSetAudioSourcePCM
        (JNIEnv *env, jclass clasz, jint audioID, jint numChannels, jint samplingRate, jint bitPerSample)
{
    return AudioSource::setAudioSourcePCM(audioID,numChannels,samplingRate,bitPerSample);
}

/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeAddPCMBuffer_DirectByteBuffer
 * Signature: (ILjava/nio/ByteBuffer;II)Z
 */
JNIEXPORT jboolean JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeAddPCMBuffer_1DirectByteBuffer
        (JNIEnv *env, jclass clasz, jint audioID, jobject byteBuffer, jint offset, jint length)
{
    jbyte *dBuf = (jbyte*)env->GetDirectBufferAddress(byteBuffer);
    return AudioSource::addPCMBuffer(audioID,dBuf,offset,length);
}

/*
 * Class:     kr_co_smartstudy_soundpoolcompat_AudioSource
 * Method:    nativeAddPCMBuffer_ByteArray
 * Signature: (I[BII)Z
 */
JNIEXPORT jboolean JNICALL Java_kr_co_smartstudy_soundpoolcompat_AudioSource_nativeAddPCMBuffer_1ByteArray
        (JNIEnv *env, jclass clasz, jint audioID, jbyteArray byteArray, jint offset, jint length)
{
    jbyte* pBuf = (jbyte*) env->GetPrimitiveArrayCritical(byteArray, 0);

    jboolean ret = AudioSource::addPCMBuffer(audioID,pBuf,offset,length);

    env->ReleasePrimitiveArrayCritical(byteArray, pBuf, 0);
    return ret;
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
        (JNIEnv *env, jclass clasz, jint audioID, jobject fd, jlong offset, jlong length, jboolean autoclose)
{
    return AudioSource::setAudioSourceFileDescriptor(audioID,jniGetFDFromFileDescriptor(env, fd),int64_t(offset),int64_t(length),autoclose);
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
