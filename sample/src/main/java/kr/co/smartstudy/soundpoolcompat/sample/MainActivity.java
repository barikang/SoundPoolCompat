package kr.co.smartstudy.soundpoolcompat.sample;

import android.content.res.AssetFileDescriptor;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

import java.io.IOException;
import java.util.Random;

import kr.co.smartstudy.soundpoolcompat.AudioEngine;
import kr.co.smartstudy.soundpoolcompat.AudioSource;

public class MainActivity extends AppCompatActivity {


    AudioSource mAudioSrc1;
    AudioEngine mAudioEngine;
    int mLastStreamID = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        AssetFileDescriptor fd = getResources().openRawResourceFd(R.raw.numbers_en_1);

        try {
            mAudioSrc1 = AudioSource.createPCMFromFileDescriptor(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
            fd.close();
        } catch (IOException e) {
            e.printStackTrace();
        }

        mAudioEngine = new AudioEngine();
        findViewById(R.id.btn_test).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mLastStreamID = mAudioEngine.playAudio(mAudioSrc1, 2, 1.0f,AudioEngine.ANDROID_STREAM_SYSTEM);
            }
        });
        findViewById(R.id.btn_test2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mAudioEngine.stop(mLastStreamID);
            }
        });

        findViewById(R.id.btn_test3).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final Random rand = new Random();
                Runnable r = new Runnable() {
                    @Override
                    public void run() {
                        for(int i = 0 ; i < 20 ; i++) {

                            //if (id >= 0)
                            {
                                try {
                                    Thread.sleep(30+rand.nextInt(100));
                                    int id = mAudioEngine.playAudio(mAudioSrc1, 0, 1.0f,AudioEngine.ANDROID_STREAM_SYSTEM);
                                } catch (InterruptedException e) {
                                    e.printStackTrace();
                                }
                                //mAudioEngine.stop(id);
                            }
                        }
                    }
                };

                for(int i = 0 ; i < 10 ; i++)
                    new Thread(r).start();
            }
        });



    }
}
