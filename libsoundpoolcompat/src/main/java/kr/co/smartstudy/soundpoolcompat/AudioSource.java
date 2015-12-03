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
    static native boolean nativeSetAudioSourcePCM(int audioID,int numChannels,int samplingRate,int bitPerSample);
    static native boolean nativeAddPCMBuffer_DirectByteBuffer(int audioID,ByteBuffer byteBuffer,int offset,int size);
    static native boolean nativeAddPCMBuffer_ByteArray(int audioID,byte[] byteArray,int offset,int size);
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

    public interface OnCreateAudioSourceComplete
    {
        void onCreateAudioSourceComplete(AudioSource audioSrc,boolean success);

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

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private static boolean decodeAndBindPCM(AudioSource audioSrc,FileDescriptor fd, long fdOffset, long fdLength)
    {
        boolean ret = true;
        final int audioID = audioSrc.getAudioID();
        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        ByteBuffer[] codecInputBuffers = null;
        ByteBuffer[] codecOutputBuffers = null;
        try {
            extractor.setDataSource(fd, fdOffset, fdLength);
            MediaFormat format = null;
            String mime = null;
            for (int i = 0; i < extractor.getTrackCount(); i++) {
                format = extractor.getTrackFormat(0);
                mime = format.getString(MediaFormat.KEY_MIME);
                if (mime != null && mime.startsWith("audio/")) {
                    extractor.selectTrack(i);
                    break;
                }
            }

            codec = MediaCodec.createDecoderByType(mime);
            codec.configure(format, null /* surface */, null /* crypto */, 0 /* flags */);
            codec.start();

            final boolean useBuffersUnder21 = Build.VERSION.SDK_INT < 21;
            if (useBuffersUnder21) {
                codecInputBuffers = codec.getInputBuffers();
                codecOutputBuffers = codec.getOutputBuffers();
            }
            // start decoding
            final long kTimeOutUs = 5000;
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

            long totalOutputSize = 0;
            boolean sawInputEOS = false;
            boolean sawOutputEOS = false;
            int noOutputCounter = 0;
            while (!sawOutputEOS && noOutputCounter < 50) {
                noOutputCounter++;
                if (!sawInputEOS) {
                    final int inputBufIndex = codec.dequeueInputBuffer(kTimeOutUs);
                    if (inputBufIndex >= 0) {
                        ByteBuffer dstBuf = useBuffersUnder21 ? codecInputBuffers[inputBufIndex] : codec.getInputBuffer(inputBufIndex);
                        int sampleSize = extractor.readSampleData(dstBuf, 0 /* offset */);
                        long presentationTimeUs = 0;
                        if (sampleSize < 0) {
                            Log.d(TAG, "saw input EOS.");
                            sawInputEOS = true;
                            sampleSize = 0;
                        } else {
                            presentationTimeUs = extractor.getSampleTime();
                        }
                        codec.queueInputBuffer(
                                inputBufIndex,
                                0 /* offset */,
                                sampleSize,
                                presentationTimeUs,
                                sawInputEOS ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                        if (!sawInputEOS) {
                            extractor.advance();
                        }
                    } else {
                        Log.e(TAG, "dequeueInputBuffer error");
                    }
                }

                final int outputBufIndex = codec.dequeueOutputBuffer(info, kTimeOutUs);
                if (outputBufIndex >= 0) {
                    //Log.d(TAG, "got frame, size " + info.size + "/" + info.presentationTimeUs);
                    if (info.size > 0) {
                        noOutputCounter = 0;

                        ByteBuffer buf = useBuffersUnder21 ? codecOutputBuffers[outputBufIndex] : codec.getOutputBuffer(outputBufIndex);

                        boolean resultAddPCMBuffer = false;
                        if (buf.isDirect())
                            resultAddPCMBuffer = nativeAddPCMBuffer_DirectByteBuffer(audioID, buf, 0, info.size);

                        if (resultAddPCMBuffer == false) {
                            resultAddPCMBuffer = nativeAddPCMBuffer_ByteArray(audioID, buf.array(), 0, info.size);
                        }
                        if (resultAddPCMBuffer == false)
                            throw new IOException("Failed add PCMBuffer");

                        totalOutputSize += info.size;
                    }

                    codec.releaseOutputBuffer(outputBufIndex, false /* render */);

                    if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        Log.d(TAG, "saw output EOS.");
                        sawOutputEOS = true;
                    }

                } else if (outputBufIndex == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
                    if (useBuffersUnder21)
                        codecOutputBuffers = codec.getOutputBuffers();
                    Log.d(TAG, "output buffers have changed.");
                } else if (outputBufIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    MediaFormat oformat = codec.getOutputFormat();

                    Log.d(TAG, "output format has changed to " + oformat);
                } else {
                    Log.d(TAG, "dequeueOutputBuffer returned " + outputBufIndex);
                }
            }

            MediaFormat outputFormat = codec.getOutputFormat();


            final long duration = format.getLong(MediaFormat.KEY_DURATION);
            final int channelCount = outputFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT);
            final int sampleRate = outputFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE);

            Log.i(TAG, "Duraiton " + duration + " chl:" + channelCount + " sampleRate:" + sampleRate);
            Log.i(TAG, "total output size = " + totalOutputSize);

            if(false == nativeSetAudioSourcePCM(audioID,channelCount,sampleRate,16))
                throw new IllegalStateException();

        }catch (Exception e) {
            Log.e(TAG,"",e);
            ret = false;
        }
        finally {
            if(codec != null) {
                codec.stop();
                codec.release();
            }
            if(extractor != null)
                extractor.release();
        }

        return ret;
    }

    public static AudioSource createPCMFromFD(FileDescriptor fd, long fdOffset, long fdLength) {
        AudioSource audioSrc = new AudioSource();
        if(!decodeAndBindPCM(audioSrc,fd,fdOffset,fdLength))
        {
            audioSrc.release();
            audioSrc = null;
        }
        return audioSrc;
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public static AudioSource createPCMFromFDAsync(final FileDescriptor fd, final long fdOffset,final long fdLength
            ,final Executor executor,final OnCreateAudioSourceComplete listener) {

        final AudioSource audioSrc = new AudioSource();
        ParcelFileDescriptor dupFD = null;
        try {
             dupFD =  ParcelFileDescriptor.dup(fd);
        } catch (IOException e) {
            Log.e(TAG, "", e);
        }
        final ParcelFileDescriptor fdupFD = dupFD;
        Runnable decodeRun = new Runnable() {
            @Override
            public void run() {
                final boolean decodeResult = decodeAndBindPCM(audioSrc,fdupFD.getFileDescriptor(),fdOffset,fdLength);
                try {
                    fdupFD.close();
                } catch (IOException e) {
                    Log.e(TAG,"",e);
                }
                if(!decodeResult)
                {
                    Log.e(TAG,"Decoding is failed");
                    audioSrc.release();
                }
                if(listener != null)
                    listener.onCreateAudioSourceComplete(audioSrc,decodeResult);

            }
        };
        if(executor != null)
            executor.execute(decodeRun);
        else
            decodeRun.run();
        return audioSrc;
    }


}
