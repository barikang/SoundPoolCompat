package kr.co.smartstudy.soundpoolcompat;

import android.media.AudioManager;
import android.util.SparseArray;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;
import java.util.concurrent.ConcurrentHashMap;
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
    static native int nativeDecodeAudio(int audioID,int streamGroupID);
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


    final private static SparseArray<WeakReference<AudioPool>> gStreamGroupID2AudioPool = new SparseArray<>();
    final private static WeakReference<AudioPool> gNullAudioPool = new WeakReference<AudioPool>(null);

    static void registerAudioPool(int streamGroupID,AudioPool audioPool)
    {
        synchronized (gStreamGroupID2AudioPool) {
            gStreamGroupID2AudioPool.put(streamGroupID,new WeakReference<AudioPool>(audioPool));
        }
    }

    static void onDecodingComplete(int streamGroupID,int audioID,int result)
    {
        AudioPool audioPool = null;
        synchronized (gStreamGroupID2AudioPool)
        {
            audioPool = gStreamGroupID2AudioPool.get(streamGroupID, gNullAudioPool).get();
        }
        if(audioPool != null)
            audioPool.postEvent(AudioPool.SAMPLE_LOADED,audioID,result,null);

    }

}
