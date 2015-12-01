package kr.co.smartstudy.soundpoolcompat;

import android.media.AudioManager;

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
    static native int nativePlayAudio(int audioID,int repeatcount ,float volume,int androidStreamType);
    static native void nativePause(int streamID);
    static native void nativeStop(int streamID);
    static native void nativeResume(int streamID);

    //////////////////////////////////////////////////////////////
    public static final int ANDROID_STREAM_VOICE = AudioManager.STREAM_VOICE_CALL;
    public static final int ANDROID_STREAM_SYSTEM = AudioManager.STREAM_SYSTEM;
    public static final int ANDROID_STREAM_RING = AudioManager.STREAM_RING;
    public static final int ANDROID_STREAM_MEDIA = AudioManager.STREAM_MUSIC;
    public static final int ANDROID_STREAM_ALARM = AudioManager.STREAM_ALARM;
    public static final int ANDROID_STREAM_NOTIFICATION = AudioManager.STREAM_NOTIFICATION;

    ///////////////////////////////////////////////////////////////
    private boolean mReleased;

    public AudioEngine() {
        nativeInitilizeAudioEngine();
        mReleased = false;
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

    public int playAudio(AudioSource audioSrc,int repeatcount ,float volume,int androidStreamType)
    {
        return nativePlayAudio(audioSrc.getAudioID(),repeatcount,volume,androidStreamType);
    }

    public void pause(int streamID)
    {
        nativePause(streamID);
    }

    public void stop(int streamID)
    {
        nativeStop(streamID);
    }

    public void resume(int streamID)
    {
        nativeResume(streamID);
    }

}
