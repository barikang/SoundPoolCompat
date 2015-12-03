package kr.co.smartstudy.soundpoolcompat.sample;

import android.content.res.AssetFileDescriptor;
import android.media.AudioManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.View;

import java.io.IOException;
import java.util.Random;

import kr.co.smartstudy.soundpoolcompat.AudioEngine;
import kr.co.smartstudy.soundpoolcompat.AudioSource;
import kr.co.smartstudy.soundpoolcompat.SoundPoolCompat;

public class MainActivity extends AppCompatActivity {


    int mLastStreamID = 0;
    int mAudioID1 = 0;
    SoundPoolCompat mSoundPool = new SoundPoolCompat(4, AudioManager.STREAM_MUSIC,true);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        AssetFileDescriptor fd = getResources().openRawResourceFd(R.raw.numbers_en_1);

        try {
            mAudioID1 = mSoundPool.load(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength(), 0);
            fd.close();
        } catch (Exception e) {
            e.printStackTrace();
        }

        findViewById(R.id.btn_test).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mLastStreamID = mSoundPool.play(mAudioID1,1.0f,1.0f,0,0,1.0f);
            }
        });

        findViewById(R.id.btn_test2).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mSoundPool.stop(mLastStreamID);
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
                                    mSoundPool.play(mAudioID1,1.0f,1.0f,0,0,1.0f);
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
