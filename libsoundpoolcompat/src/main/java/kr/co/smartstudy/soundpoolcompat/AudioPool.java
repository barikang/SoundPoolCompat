package kr.co.smartstudy.soundpoolcompat;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.SoundPool;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.ParcelFileDescriptor;
import android.util.AndroidRuntimeException;
import android.util.Log;
import android.util.SparseArray;
import android.util.SparseIntArray;

import java.io.File;
import java.io.FileDescriptor;
import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Created by Jaehun on 2015. 12. 2..
 */
public class AudioPool {
    private final static String TAG = "AudioPool";
    static final int SAMPLE_LOADED = 1;
    private final static boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private boolean mReleased;
    private EventHandler mEventHandler;
    private AudioPool.OnLoadCompleteListener mOnLoadCompleteListener;
    final private Object mLock;
    final private SparseArray<AudioSource> mAudioSources = new SparseArray<>();
    private final int mAudioStreamType;
    private boolean mTryPreDecode = true;
    final private static AtomicInteger gStreamGroupID = new AtomicInteger(1);
    final int mStreamGroupID;

    public AudioPool(int streamType) {
        this(streamType,true);
    }

    public AudioPool(int streamType,boolean tryPreDecode) {
        mLock = new Object();
        mAudioStreamType = streamType;
        mTryPreDecode = tryPreDecode;

        AudioEngine.nativeInitilizeAudioEngine();
        mReleased = false;
        mStreamGroupID = gStreamGroupID.getAndIncrement();
        AudioEngine.registerAudioPool(mStreamGroupID, this);

    }

    synchronized
    public void release() {
        if(mReleased == false)
        {
            mReleased = true;
            AudioEngine.nativeReleaseAudioEngine();
        }
    }

    @Override
    protected void finalize() throws Throwable {
        release();
    }


    public final int loadAsync(String path) {
        int id = 0;
        try {
            File f = new File(path);
            ParcelFileDescriptor fd = ParcelFileDescriptor.open(f,
                    ParcelFileDescriptor.MODE_READ_ONLY);
            if (fd != null) {
                id = loadAsync(fd.getFileDescriptor(), 0, f.length());
                fd.close();
            }
        } catch (java.io.IOException e) {
            Log.e(TAG, "error loading " + path);
        }
        return id;
    }

    public final int loadAsync(Context context, int resId) {
        AssetFileDescriptor afd = context.getResources().openRawResourceFd(resId);
        int id = 0;
        if (afd != null) {
            id = loadAsync(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
            try {
                afd.close();
            } catch (java.io.IOException ex) {
                //Log.d(TAG, "close failed:", ex);
            }
        }
        return id;
    }

    public final int loadAsync(AssetFileDescriptor afd) {
        if (afd != null) {
            long len = afd.getLength();
            if (len < 0) {
                throw new AndroidRuntimeException("no length for fd");
            }
            return loadAsync(afd.getFileDescriptor(), afd.getStartOffset(), len);
        } else {
            return 0;
        }
    }

    public final int loadAsync(FileDescriptor fd, long offset, long length) {
        if (mReleased)
            return -1;


        AudioSource audioSrc = AudioSource.createFromFD(fd, offset, length);
        synchronized (mAudioSources) {
            mAudioSources.append(audioSrc.getAudioID(), audioSrc);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH && mTryPreDecode) {
            AudioEngine.nativeDecodeAudio(audioSrc.getAudioID(), mStreamGroupID);
        } else {
            postEvent(SAMPLE_LOADED, audioSrc.getAudioID(), 0, null);
        }

        return audioSrc.getAudioID();
    }

    public final boolean unload(int audioID)
    {
        AudioSource audioSrc = null;
        synchronized (mAudioSources)
        {
            audioSrc = mAudioSources.get(audioID);
            mAudioSources.remove(audioID);
        }
        if(audioSrc != null) {
            audioSrc.release();
        }

        return audioSrc != null;
    }

    public final void unloadAll()
    {
        stopAll();
        synchronized (mAudioSources)
        {
            final int size = mAudioSources.size();
            for (int i = 0; i < size; i++) {
                AudioSource audioSrc = mAudioSources.valueAt(i);
                if(audioSrc != null)
                    audioSrc.release();
            }
            mAudioSources.clear();
        }
    }


    public final int play(int soundID, float leftVolume, float rightVolume,
                          int priority, int loop, float rate) {
        if(mReleased)
            return -1;

        AudioSource audioSrc = null;
        synchronized (mAudioSources) {
            audioSrc = mAudioSources.get(soundID);
        }
        if(audioSrc != null)
        {
            return AudioEngine.nativePlayAudio(audioSrc.getAudioID(), loop, (leftVolume + rightVolume) / 2, mAudioStreamType, mStreamGroupID, rate);
        }
        return -1;
    }

    public final void pause(int streamID)
    {
        if(mReleased)
            return;

        AudioEngine.nativePause(streamID);
    }

    public final void resume(int streamID)
    {
        if(mReleased)
            return;

        AudioEngine.nativeResume(streamID);
    }

    public final void autoPause()
    {
        if(mReleased)
            return;

        AudioEngine.nativePauseAll(mStreamGroupID);
    }

    public final void autoResume()
    {
        if(mReleased)
            return;

        AudioEngine.nativeResumeAll(mStreamGroupID);
    }

    public void stop(int streamID)
    {
        if(mReleased)
            return;

        AudioEngine.nativeStop(streamID);
    }

    public void stopAll()
    {
        if(mReleased)
            return;

        AudioEngine.nativeStopAll(mStreamGroupID);
    }

    public final void setVolume(int streamID, float leftVolume, float rightVolume)
    {
        if(mReleased)
            return;

        setVolume(streamID,(leftVolume+rightVolume)/2);
    }
    public void setVolume(int streamID, float volume)
    {
        if(mReleased)
            return;

        AudioEngine.nativeSetVolume(streamID, volume);

    }
    public void setPriority(int streamID, int priority)
    {
        // do nothing
    }

    public void setLoop(int streamID, int loop)
    {
        if(mReleased)
            return;

        AudioEngine.nativeSetRepeatCount(streamID, loop);
    }

    public void setRate(int streamID, float rate) {
        if(mReleased)
            return;

        AudioEngine.nativeSetPlayRate(streamID, rate);
    }

    public interface OnLoadCompleteListener {
        /**
         * Called when a sound has completed loading.
         *
         * @param audioPool AudioPool object from the load() method
         * @param sampleId the sample ID of the sound loaded.
         * @param status the status of the load operation (0 = success)
         */
        void onLoadComplete(AudioPool audioPool, int sampleId, int status);
    }

    /**
     * Sets the callback hook for the OnLoadCompleteListener.
     */
    public void setOnLoadCompleteListener(OnLoadCompleteListener listener) {
        synchronized(mLock) {
            if (listener != null) {
                // setup message handler
                Looper looper;
                if ((looper = Looper.myLooper()) != null) {
                    mEventHandler = new EventHandler(looper);
                } else if ((looper = Looper.getMainLooper()) != null) {
                    mEventHandler = new EventHandler(looper);
                } else {
                    mEventHandler = null;
                }
            } else {
                mEventHandler = null;
            }
            mOnLoadCompleteListener = listener;
        }
    }


    void postEvent(int msg, int arg1, int arg2, Object obj) {
        if (mEventHandler != null) {
            Message m = mEventHandler.obtainMessage(msg, arg1, arg2, obj);
            mEventHandler.sendMessage(m);
        }
    }

    private final class EventHandler extends Handler {
        public EventHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case SAMPLE_LOADED:
                    if (DEBUG) Log.d(TAG, "Sample " + msg.arg1 + " loaded");
                    synchronized(mLock) {
                        if (mOnLoadCompleteListener != null) {
                            mOnLoadCompleteListener.onLoadComplete(AudioPool.this, msg.arg1, msg.arg2);
                        }
                    }
                    break;
                default:
                    Log.e(TAG, "Unknown message type " + msg.what);
                    return;
            }
        }
    }






}
