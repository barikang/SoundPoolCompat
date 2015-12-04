package kr.co.smartstudy.soundpoolcompat;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
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

/**
 * Created by Jaehun on 2015. 12. 2..
 */
public class AudioPool {
    private final static String TAG = "AudioPool";
    private static final int SAMPLE_LOADED = 1;
    private final static boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private EventHandler mEventHandler;
    private AudioPool.OnLoadCompleteListener mOnLoadCompleteListener;
    final private Object mLock;
    final private AudioEngine mAudioEngine;
    final private ThreadPoolExecutor mThreadPool;
    final private SparseArray<AudioSource> mAudioSources = new SparseArray<>();
    private final int mAudioStreamType;
    private boolean mTryPreDecode = true;

    public AudioPool(int streamType) {
        this(streamType,true);
    }

    public AudioPool(int streamType,boolean tryPreDecode) {
        mLock = new Object();
        mAudioEngine = new AudioEngine();
        mAudioStreamType = streamType;
        mTryPreDecode = tryPreDecode;
        mThreadPool = new ThreadPoolExecutor(4,4,1, TimeUnit.SECONDS,new LinkedBlockingQueue<Runnable>());
    }

    public void release() {
        mAudioEngine.release();
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
    AudioSource.OnCreateAudioSourceComplete mOnCreateAudioSourceCompleteListener = new AudioSource.OnCreateAudioSourceComplete() {
        @Override
        public void onCreateAudioSourceComplete(AudioSource audioSrc, boolean success) {
            EventHandler handler = mEventHandler;
            if (handler != null) {
                Message m = handler.obtainMessage(SAMPLE_LOADED, audioSrc.getAudioID(), success ? 0 : -1, null);
                handler.sendMessage(m);
            }

        }
    };

    public final int loadAsync(FileDescriptor fd, long offset, long length) {
        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN && mTryPreDecode) {
            AudioSource audioSrc = AudioSource.createPCMFromFDAsync(fd, offset, length, mThreadPool, mOnCreateAudioSourceCompleteListener);
            synchronized (mAudioSources) {
                mAudioSources.append(audioSrc.getAudioID(), audioSrc);
            }
            return audioSrc.getAudioID();
        }
        else
        {
            AudioSource audioSrc = AudioSource.createFromFD(fd, offset, length);
            synchronized (mAudioSources) {
                mAudioSources.append(audioSrc.getAudioID(), audioSrc);
            }
            mOnCreateAudioSourceCompleteListener.onCreateAudioSourceComplete(audioSrc,!audioSrc.isReleased());
            return audioSrc.getAudioID();
        }
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
        AudioSource audioSrc = null;
        synchronized (mAudioSources) {
            audioSrc = mAudioSources.get(soundID);
        }
        if(audioSrc != null)
        {
            return mAudioEngine.playAudio(audioSrc,loop,(leftVolume+rightVolume)/2,mAudioStreamType ,rate);
        }
        return -1;
    }

    public final void pause(int streamID)
    {
        mAudioEngine.pause(streamID);
    }

    public final void resume(int streamID)
    {
        mAudioEngine.resume(streamID);
    }

    public final void autoPause()
    {
        mAudioEngine.pauseAll();
    }

    public final void autoResume()
    {
        mAudioEngine.resumeAll();
    }

    public void stop(int streamID)
    {
        mAudioEngine.stop(streamID);
    }

    public void stopAll()
    {
        mAudioEngine.stopAll();
    }

    public final void setVolume(int streamID, float leftVolume, float rightVolume)
    {
        setVolume(streamID,(leftVolume+rightVolume)/2);
    }
    public void setVolume(int streamID, float volume) {
        mAudioEngine.setVolume(streamID,volume);

    }
    public void setPriority(int streamID, int priority)
    {
        // do nothing
    }

    public void setLoop(int streamID, int loop)
    {
        mAudioEngine.setRepeatCount(streamID,loop);
    }

    public void setRate(int streamID, float rate)
    {
        mAudioEngine.setPlayRate(streamID,rate);
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


    private static void postEvent(WeakReference<AudioPool> ref, int msg, int arg1, int arg2, Object obj) {
        AudioPool audioPool = ref.get();
        if (audioPool == null)
            return;

        if (audioPool.mEventHandler != null) {
            Message m = audioPool.mEventHandler.obtainMessage(msg, arg1, arg2, obj);
            audioPool.mEventHandler.sendMessage(m);
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
