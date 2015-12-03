package kr.co.smartstudy.soundpoolcompat;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.AudioManager;
import android.media.SoundPool;
import android.os.Build;
import android.util.Log;

import java.io.FileDescriptor;

/**
 * Created by Jaehun on 2015. 12. 1..
 */
public class SoundPoolCompat {
    private static final int SAMPLE_LOADED = 1;

    private final static String TAG = "SoundPoolCompat";
    private final static boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    private final Object mLock;

    final private boolean mUseOpenSLES;
    private SoundPool mSoundPool = null;
    private AudioPool mAudioPool = null;
    private SoundPoolCompat.OnLoadCompleteListener mOnLoadCompleteListener = null;

    /**
     * Constructor. Constructs a SoundPool object.
     * OS < LOLLIPOP : use original soundpool.
     * OS >= LOLLIPOP : use OpenSL ES SoundPool.
     *
     * @param maxStreams the maximum number of simultaneous streams for this
     *                   SoundPool object
     * @param streamType the audio stream type as described in AudioManager
     *                   For example, game applications will normally use
     *                   {@link AudioManager#STREAM_MUSIC}.
     * @param srcQuality the sample-rate converter quality. Currently has no
     *                   effect. Use 0 for the default.
     * @return a SoundPoolCompat object, or null if creation failed
     */
    public SoundPoolCompat(int maxStreams, int streamType, int srcQuality) {
        this(maxStreams,streamType,Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP);
    }

    /**
     * Constructor. Constructs a SoundPool object with the following
     * characteristics:
     *
     * @param maxStreams the maximum number of simultaneous streams for this
     *                   SoundPool object
     * @param streamType the audio stream type as described in AudioManager
     *                   For example, game applications will normally use
     *                   {@link AudioManager#STREAM_MUSIC}.
     * @param useOpenSL use OpenSL ES SoundPool.
     * @return a SoundPoolCompat object, or null if creation failed
     */
    public SoundPoolCompat(int maxStreams, int streamType,boolean useOpenSL) {
        mLock = new Object();
        mUseOpenSLES = useOpenSL;
        if(mUseOpenSLES)
        {
            mAudioPool = new AudioPool(streamType);
        }
        else
        {
            mSoundPool = new SoundPool(maxStreams,streamType,0);
        }
    }


    /**
     * Release the SoundPool resources.
     *
     * Release all memory and native resources used by the SoundPool
     * object. The SoundPool can no longer be used and the reference
     * should be set to null.
     */
    public void release() {
        if(mUseOpenSLES){
            mAudioPool.release();
        }
        else {
            mSoundPool.release();
        }

    }

    /**
     * Load the sound from the specified path.
     *
     * @param path the path to the audio file
     * @param priority the priority of the sound. Currently has no effect. Use
     *                 a value of 1 for future compatibility.
     * @return a sound ID. This value can be used to play or unload the sound.
     */
    public int load(String path, int priority) {
        return mUseOpenSLES ? mAudioPool.loadAsync(path) : mSoundPool.load(path,priority);
    }

    /**
     * Load the sound from the specified APK resource.
     *
     * Note that the extension is dropped. For example, if you want to load
     * a sound from the raw resource file "explosion.mp3", you would specify
     * "R.raw.explosion" as the resource ID. Note that this means you cannot
     * have both an "explosion.wav" and an "explosion.mp3" in the res/raw
     * directory.
     *
     * @param context the application context
     * @param resId the resource ID
     * @param priority the priority of the sound. Currently has no effect. Use
     *                 a value of 1 for future compatibility.
     * @return a sound ID. This value can be used to play or unload the sound.
     */
    public int load(Context context, int resId, int priority) {
        return mUseOpenSLES ? mAudioPool.loadAsync(context, resId) : mSoundPool.load(context,resId,priority);
    }

    /**
     * Load the sound from an asset file descriptor.
     *
     * @param afd an asset file descriptor
     * @param priority the priority of the sound. Currently has no effect. Use
     *                 a value of 1 for future compatibility.
     * @return a sound ID. This value can be used to play or unload the sound.
     */
    public int load(AssetFileDescriptor afd, int priority) {
        return mUseOpenSLES ? mAudioPool.loadAsync(afd) : mSoundPool.load(afd,priority);
    }

    /**
     * Load the sound from a FileDescriptor.
     *
     * This version is useful if you store multiple sounds in a single
     * binary. The offset specifies the offset from the start of the file
     * and the length specifies the length of the sound within the file.
     *
     * @param fd a FileDescriptor object
     * @param offset offset to the start of the sound
     * @param length length of the sound
     * @param priority the priority of the sound. Currently has no effect. Use
     *                 a value of 1 for future compatibility.
     * @return a sound ID. This value can be used to play or unload the sound.
     */
    public int load(FileDescriptor fd, long offset, long length, int priority) {
        return mUseOpenSLES ? mAudioPool.loadAsync(fd,offset,length) : mSoundPool.load(fd,offset,length,priority);
    }

    /**
     * Unload a sound from a sound ID.
     *
     * Unloads the sound specified by the soundID. This is the value
     * returned by the load() function. Returns true if the sound is
     * successfully unloaded, false if the sound was already unloaded.
     *
     * @param soundID a soundID returned by the load() function
     * @return true if just unloaded, false if previously unloaded
     */
    public boolean unload(int soundID) {
        return true;
    }

    /**
     * Play a sound from a sound ID.
     *
     * Play the sound specified by the soundID. This is the value
     * returned by the load() function. Returns a non-zero streamID
     * if successful, zero if it fails. The streamID can be used to
     * further control playback. Note that calling play() may cause
     * another sound to stop playing if the maximum number of active
     * streams is exceeded. A loop value of -1 means loop forever,
     * a value of 0 means don't loop, other values indicate the
     * number of repeats, e.g. a value of 1 plays the audio twice.
     * The playback rate allows the application to vary the playback
     * rate (pitch) of the sound. A value of 1.0 means play back at
     * the original frequency. A value of 2.0 means play back twice
     * as fast, and a value of 0.5 means playback at half speed.
     *
     * @param soundID a soundID returned by the load() function
     * @param leftVolume left volume value (range = 0.0 to 1.0)
     * @param rightVolume right volume value (range = 0.0 to 1.0)
     * @param priority stream priority (0 = lowest priority)
     * @param loop loop mode (0 = no loop, -1 = loop forever)
     * @param rate playback rate (1.0 = normal playback, range 0.5 to 2.0)
     * @return non-zero streamID if successful, zero if failed
     */
    public int play(int soundID, float leftVolume, float rightVolume,
                          int priority, int loop, float rate) {
        return mUseOpenSLES ?
                mAudioPool.play(soundID,leftVolume,rightVolume,priority,loop,rate)
                : mSoundPool.play(soundID, leftVolume, rightVolume, priority, loop, rate);
    }

    /**
     * Pause a playback stream.
     *
     * Pause the stream specified by the streamID. This is the
     * value returned by the play() function. If the stream is
     * playing, it will be paused. If the stream is not playing
     * (e.g. is stopped or was previously paused), calling this
     * function will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     */
    public void pause(int streamID)
    {
        if(mUseOpenSLES)
            mAudioPool.pause(streamID);
        else
            mSoundPool.pause(streamID);

    }

    /**
     * Resume a playback stream.
     *
     * Resume the stream specified by the streamID. This
     * is the value returned by the play() function. If the stream
     * is paused, this will resume playback. If the stream was not
     * previously paused, calling this function will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     */
    public void resume(int streamID)
    {
        if(mUseOpenSLES)
            mAudioPool.resume(streamID);
        else
            mSoundPool.resume(streamID);

    }

    /**
     * Pause all active streams.
     *
     * Pause all streams that are currently playing. This function
     * iterates through all the active streams and pauses any that
     * are playing. It also sets a flag so that any streams that
     * are playing can be resumed by calling autoResume().
     */
    public void autoPause()
    {
        if(mUseOpenSLES)
            mAudioPool.autoPause();
        else
            mSoundPool.autoPause();
    }

    /**
     * Resume all previously active streams.
     *
     * Automatically resumes all streams that were paused in previous
     * calls to autoPause().
     */
    public void autoResume()
    {
        if(mUseOpenSLES)
            mAudioPool.autoResume();
        else
            mSoundPool.autoResume();
    }

    /**
     * Stop a playback stream.
     *
     * Stop the stream specified by the streamID. This
     * is the value returned by the play() function. If the stream
     * is playing, it will be stopped. It also releases any native
     * resources associated with this stream. If the stream is not
     * playing, it will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     */
    public void stop(int streamID)
    {
        if(mUseOpenSLES)
            mAudioPool.stop(streamID);
        else
            mSoundPool.stop(streamID);
    }

    /**
     * Set stream volume.
     *
     * Sets the volume on the stream specified by the streamID.
     * This is the value returned by the play() function. The
     * value must be in the range of 0.0 to 1.0. If the stream does
     * not exist, it will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     * @param leftVolume left volume value (range = 0.0 to 1.0)
     * @param rightVolume right volume value (range = 0.0 to 1.0)
     */
    public final void setVolume(int streamID, float leftVolume, float rightVolume) {
        if(mUseOpenSLES)
            mAudioPool.setVolume(streamID, leftVolume,rightVolume);
        else
            mSoundPool.setVolume(streamID, leftVolume,rightVolume);

    }
    /**
     * Similar, except set volume of all channels to same value.
     */
    public void setVolume(int streamID, float volume) {
        if(mUseOpenSLES)
            mAudioPool.setVolume(streamID,volume);
        else
            mSoundPool.setVolume(streamID, volume,volume);

    }

    /**
     * Change stream priority.
     *
     * Change the priority of the stream specified by the streamID.
     * This is the value returned by the play() function. Affects the
     * order in which streams are re-used to play new sounds. If the
     * stream does not exist, it will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     */
    public void setPriority(int streamID, int priority)
    {
        if(mUseOpenSLES)
            mAudioPool.setPriority(streamID, priority);
        else
            mSoundPool.setPriority(streamID, priority);

    }

    /**
     * Set loop mode.
     *
     * Change the loop mode. A loop value of -1 means loop forever,
     * a value of 0 means don't loop, other values indicate the
     * number of repeats, e.g. a value of 1 plays the audio twice.
     * If the stream does not exist, it will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     * @param loop loop mode (0 = no loop, -1 = loop forever)
     */
    public void setLoop(int streamID, int loop) {
        if(mUseOpenSLES)
            mAudioPool.setLoop(streamID,loop);
        else
            mSoundPool.setLoop(streamID,loop);

    }

    /**
     * Change playback rate.
     *
     * The playback rate allows the application to vary the playback
     * rate (pitch) of the sound. A value of 1.0 means playback at
     * the original frequency. A value of 2.0 means playback twice
     * as fast, and a value of 0.5 means playback at half speed.
     * If the stream does not exist, it will have no effect.
     *
     * @param streamID a streamID returned by the play() function
     * @param rate playback rate (1.0 = normal playback, range 0.5 to 2.0)
     */
    public void setRate(int streamID, float rate)
    {
        if(mUseOpenSLES)
            mAudioPool.pause(streamID);
        else
            mSoundPool.pause(streamID);
    }

    private class SoundPoolOnCompleteListener implements SoundPool.OnLoadCompleteListener
    {
        @Override
        public void onLoadComplete(SoundPool soundPool, int sampleId, int status) {
            synchronized(mLock) {
                if (mOnLoadCompleteListener != null) {
                    mOnLoadCompleteListener.onLoadComplete(SoundPoolCompat.this, sampleId, status);
                }
            }

        }
    }

    private class AudioPoolOnCompleteListener implements AudioPool.OnLoadCompleteListener
    {
        @Override
        public void onLoadComplete(AudioPool audioPool, int sampleId, int status) {
            synchronized(mLock) {
                if (mOnLoadCompleteListener != null) {
                    mOnLoadCompleteListener.onLoadComplete(SoundPoolCompat.this, sampleId, status);
                }
            }
        }
    }

    public interface OnLoadCompleteListener {
        /**
         * Called when a sound has completed loading.
         *
         * @param soundPool SoundPool object from the load() method
         * @param sampleId the sample ID of the sound loaded.
         * @param status the status of the load operation (0 = success)
         */
        void onLoadComplete(SoundPoolCompat soundPool, int sampleId, int status);
    }

    /**
     * Sets the callback hook for the OnLoadCompleteListener.
     */
    public void setOnLoadCompleteListener(OnLoadCompleteListener listener) {
        final boolean isNull = listener == null;

        if(mUseOpenSLES)
        {
            mAudioPool.setOnLoadCompleteListener(isNull ? null : new AudioPoolOnCompleteListener());
        }
        else
        {
            mSoundPool.setOnLoadCompleteListener(isNull ? null : new SoundPoolOnCompleteListener());
        }
        synchronized(mLock) {
            mOnLoadCompleteListener = listener;
        }

    }

}
