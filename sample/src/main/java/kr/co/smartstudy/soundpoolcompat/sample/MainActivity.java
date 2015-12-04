package kr.co.smartstudy.soundpoolcompat.sample;

import android.media.AudioManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;

import java.util.Random;

import kr.co.smartstudy.soundpoolcompat.SoundPoolCompat;

public class MainActivity extends AppCompatActivity {
    private final static String TAG = "SoundPoolCompatTest";

    int[] mSoundIDs = new int[10];
    SoundPoolCompat mSoundPool = new SoundPoolCompat(4, AudioManager.STREAM_MUSIC,true,true);
    final Random mRand = new Random();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        findViewById(R.id.btn_load).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final SparseArray<Long> startTimes = new SparseArray<Long>(10);
                mSoundPool.setOnLoadCompleteListener(new SoundPoolCompat.OnLoadCompleteListener() {
                    @Override
                    public void onLoadComplete(SoundPoolCompat soundPool, int sampleId, int status) {
                        long loadingTime = System.currentTimeMillis() - startTimes.get(sampleId);
                        Log.d(TAG,String.format("load complete %d %d [%dms]",sampleId,status,loadingTime));
                    }
                });
                int[] resids = {
                        R.raw.numbers_en_1,
                        R.raw.numbers_en_2,
                        R.raw.numbers_en_3,
                        R.raw.numbers_en_4,
                        R.raw.numbers_en_5,
                        R.raw.numbers_en_6,
                        R.raw.numbers_en_7,
                        R.raw.numbers_en_8,
                        R.raw.numbers_en_9,
                        R.raw.numbers_en_10 };
                long callTimeSum = 0;
                for(int i = 0 ; i < 10 ; i++)
                {
                    long st = System.currentTimeMillis();
                    mSoundIDs[i] = mSoundPool.load(MainActivity.this,resids[i],0);
                    long en = System.currentTimeMillis();
                    startTimes.append(mSoundIDs[i],en);
                    callTimeSum += (en-st);
                }

                Log.d(TAG,"total call load time = "+callTimeSum);
            }
        });

        findViewById(R.id.btn_unloadAll).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mSoundPool.unloadAll();
            }
        });

        findViewById(R.id.btn_plays).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                Runnable r = new Runnable() {
                    @Override
                    public void run() {
                        for (int i = 0; i < 20; i++) {

                            //if (id >= 0)
                            {
                                try {
                                    Thread.sleep(30 + mRand.nextInt(100));
                                    mSoundPool.play(mSoundIDs[mRand.nextInt(10)] , 1.0f, 1.0f, 0, 0, 1.0f);
                                } catch (InterruptedException e) {
                                    e.printStackTrace();
                                }
                                //mAudioEngine.stop(id);
                            }
                        }
                    }
                };

                for (int i = 0; i < 10; i++)
                    new Thread(r,"test "+i).start();
            }
        });



    }
}
