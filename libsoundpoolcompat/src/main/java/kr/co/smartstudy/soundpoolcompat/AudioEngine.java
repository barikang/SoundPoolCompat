package kr.co.smartstudy.soundpoolcompat;

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
    static native int nativePlayAudio(int audioID,boolean loop ,float volume);
    static native void nativePause(int streamID);
    static native void nativeStop(int streamID);
    static native void nativeResume(int streamID);

    //////////////////////////////////////////////////////////////

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

    public int playAudio(AudioSource audioSrc,boolean loop ,float volume)
    {
        return nativePlayAudio(audioSrc.getAudioID(),loop,volume);
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
