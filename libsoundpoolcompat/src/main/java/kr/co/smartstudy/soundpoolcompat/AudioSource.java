package kr.co.smartstudy.soundpoolcompat;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;

import java.io.FileDescriptor;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;

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
    static native boolean nativeSetAudioSourceFileDescriptor(int audioID,int fd,int offset,int length,boolean autoclose);
    static native boolean nativeSetAudioSourceURI(int audioID,String uri);


    ////////////////////////////////////////////////////////////////////////////////

    public final static int INVALID_AUDIOID = -1;

    private final int mAudioID;
    private boolean mReleased = false;

    private AudioSource(int audioID) {
        mAudioID = audioID;
    }

    public int getAudioID() { return mAudioID; };

    synchronized
    public void release() {
        if(mReleased == false) {
            mReleased = true;
            nativeReleaseAudioSource(mAudioID);
        }
    }


    /////////////////////////////////////////////////////////////////////////////////

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public static AudioSource createPCMFromFileDescriptor(FileDescriptor fd, long fdOffset, long fdLength) {
        int audioID = nativeCreateAudioSource();

        MediaExtractor extractor = new MediaExtractor();
        MediaCodec codec = null;
        ByteBuffer[] codecInputBuffers = null;
        ByteBuffer[] codecOutputBuffers = null;
        try {
            extractor.setDataSource(fd, fdOffset, fdLength);


            //assertEquals("wrong number of tracks", 1, extractor.getTrackCount());
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


            Log.d(TAG, "first output format " + codec.getOutputFormat());


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
            if(audioID != INVALID_AUDIOID)
                nativeReleaseAudioSource(audioID);
            audioID = INVALID_AUDIOID;
        }
        finally {
            if(codec != null) {
                codec.stop();
                codec.release();
            }
            if(extractor != null)
                extractor.release();
        }

        if(audioID != INVALID_AUDIOID)
            return new AudioSource(audioID);

        return null;

    }

}
