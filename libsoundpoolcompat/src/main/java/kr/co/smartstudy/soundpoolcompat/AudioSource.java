package kr.co.smartstudy.soundpoolcompat;

import android.annotation.TargetApi;
import android.content.res.AssetFileDescriptor;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

/**
 * Created by Jaehun on 2015-11-30.
 */
public class AudioSource {
    private final static String TAG = "AudioSource";
    static {
        System.loadLibrary("SoundPoolCompat");
    }


    static native int nativeCreateAudioSource();
    static native void nativeReleaseAudioSource(int audioID);
    static native boolean nativeSetAudioSourceFileDescriptor(int audioID,FileDescriptor fd,long offset,long length);
    static native boolean nativeSetAudioSourceURI(int audioID,String uri);


    ////////////////////////////////////////////////////////////////////////////////

    public final static int INVALID_AUDIOID = -1;

    private final int mAudioID;
    private boolean mReleased = false;

    private AudioSource() {
        mAudioID = nativeCreateAudioSource();
    }

    public int getAudioID() { return mAudioID; };

    synchronized
    public void release() {
        if(mReleased == false) {
            mReleased = true;
            nativeReleaseAudioSource(mAudioID);
        }
    }

    synchronized
    public boolean isReleased() { return mReleased; };

    @Override
    protected void finalize() throws Throwable {
        release();
    }


    /////////////////////////////////////////////////////////////////////////////////
    public static AudioSource createFromFD(FileDescriptor fd, long fdOffset, long fdLength)
    {
        AudioSource audioSrc = new AudioSource();
        if(!nativeSetAudioSourceFileDescriptor(audioSrc.getAudioID(),fd,fdOffset,fdLength))
        {
            audioSrc.release();
        }
        return audioSrc;
    }

    public static AudioSource createFromURI(String uri)
    {
        AudioSource audioSrc = new AudioSource();
        if(!nativeSetAudioSourceURI(audioSrc.getAudioID(), uri))
        {
            audioSrc.release();
        }
        return audioSrc;
    }

}
