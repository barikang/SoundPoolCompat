package kr.co.smartstudy.soundpoolcompat;

import android.media.AudioManager;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Created by Jaehun on 2015-11-30.
 */
public class AudioEngine {
    final static String TAG = "AudioEngine";
    static {
        System.loadLibrary("SoundPoolCompat");

    }


    static native void nativeInitilizeAudioEngine();
    static native void nativeReleaseAudioEngine();
    static native int nativePlayAudio(int audioID,int repeatcount ,float volume,int androidStreamType,int streamGroupID,float playRate);
    static native void nativePause(int streamID);
    static native void nativeStop(int streamID);
    static native void nativeResume(int streamID);
    static native void nativePauseAll(int streamGroupID);
    static native void nativeResumeAll(int streamGroupID);
    static native void nativeStopAll(int streamGroupID);
    static native void nativeSetVolume(int streamID,float volume);
    static native void nativeSetPlayRate(int streamID,float playRate);
    static native void nativeSetRepeatCount(int streamID,int repeatCount);

    //////////////////////////////////////////////////////////////
    public static final int ANDROID_STREAM_VOICE = AudioManager.STREAM_VOICE_CALL;
    public static final int ANDROID_STREAM_SYSTEM = AudioManager.STREAM_SYSTEM;
    public static final int ANDROID_STREAM_RING = AudioManager.STREAM_RING;
    public static final int ANDROID_STREAM_MEDIA = AudioManager.STREAM_MUSIC;
    public static final int ANDROID_STREAM_ALARM = AudioManager.STREAM_ALARM;
    public static final int ANDROID_STREAM_NOTIFICATION = AudioManager.STREAM_NOTIFICATION;

    ///////////////////////////////////////////////////////////////
    private boolean mReleased;
    static AtomicInteger gStreamGroupID = new AtomicInteger(1);
    final int mStreamGroupID;

    public AudioEngine() {
        nativeInitilizeAudioEngine();
        mReleased = false;
        mStreamGroupID = gStreamGroupID.getAndIncrement();
    }

    public void release() {
        if(mReleased == false)
        {
            mReleased = true;
            nativeReleaseAudioEngine();
        }
    }

    @Override
    protected void finalize() throws Throwable {
        release();
        super.finalize();
    }

    public int playAudio(AudioSource audioSrc,int repeatcount ,float volume,int androidStreamType,float playRate)
    {

        if(mReleased)
            return -1;
        return nativePlayAudio(audioSrc.getAudioID(),repeatcount,volume,androidStreamType,mStreamGroupID,playRate);
    }

    public void pause(int streamID)
    {
        if(mReleased)
            return;
        nativePause(streamID);
    }

    public void stop(int streamID)
    {
        if(mReleased)
            return;
        nativeStop(streamID);
    }

    public void resume(int streamID)
    {
        if(mReleased)
            return;
        nativeResume(streamID);
    }

    public void pauseAll() {
        if(mReleased)
            return;

        nativePauseAll(mStreamGroupID);

    }

    public void resumeAll() {
        if(mReleased)
            return;
        nativeResumeAll(mStreamGroupID);
    }

    public void stopAll() {
        if(mReleased)
            return;

        nativeStopAll(mStreamGroupID);
    }

    public void setVolume(int streamID,float volume)
    {
        if(mReleased)
            return;

        nativeSetVolume(streamID,volume);
    }

    public void setPlayRate(int streamID,float playRate)
    {
        if(mReleased)
            return;

        nativeSetPlayRate(mStreamGroupID,playRate);
    }

    public void setRepeatCount(int streamID,int repeatCount)
    {
        if(mReleased)
            return;

        nativeSetRepeatCount(mStreamGroupID,repeatCount);
    }

}
