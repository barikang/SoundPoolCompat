package kr.co.smartstudy.soundpoolcompat.sample;

import android.media.AudioManager;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.util.SparseArray;
import android.view.View;

import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;

import kr.co.smartstudy.soundpoolcompat.SoundPoolCompat;

public class MainActivity extends AppCompatActivity {
    private final static String TAG = "SoundPoolCompatTest";

    ArrayList<Integer> mSoundIDs = new ArrayList<>();
    SoundPoolCompat mSoundPool = new SoundPoolCompat(4, AudioManager.STREAM_MUSIC,true,true);
    final Random mRand = new Random();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        findViewById(R.id.btn_load).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
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
                        R.raw.numbers_en_10};
                final int loadCnt = 60;
                final AtomicInteger _count = new AtomicInteger(loadCnt);
                final SparseArray<Long> startTimes = new SparseArray<Long>(resids.length);
                final long _startTime = System.currentTimeMillis();
                mSoundPool.setOnLoadCompleteListener(new SoundPoolCompat.OnLoadCompleteListener() {
                    @Override
                    public void onLoadComplete(SoundPoolCompat soundPool, int sampleId, int status) {
                        synchronized (startTimes) {
                            if (startTimes.indexOfKey(sampleId) >= 0) {
                                long loadingTime = System.currentTimeMillis() - startTimes.get(sampleId);
                                Log.d(TAG, String.format("load complete %d %d [%dms] [%d/%d]", sampleId, status, loadingTime,(loadCnt-_count.get()+1),loadCnt));
                            }
                        }

                        if (_count.decrementAndGet() == 0) {
                            Log.d(TAG, String.format("Total loading time = %dms", System.currentTimeMillis() - _startTime));
                        }
                    }
                });

                long callTimeSum = 0;
                for (int i = 0; i < loadCnt; i++) {
                    long st = System.currentTimeMillis();
                    synchronized (startTimes) {
                        int soundID = mSoundPool.load(MainActivity.this, resids[i%resids.length], 0);
                        synchronized (mSoundIDs) {
                            mSoundIDs.add(soundID);
                        }
                        long en = System.currentTimeMillis();
                        startTimes.append(soundID, en);
                        callTimeSum += (en - st);
                    }

                }

                Log.d(TAG, "total call load time = " + callTimeSum);
            }
        });

        findViewById(R.id.btn_unloadAll).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mSoundPool.unloadAll();
                synchronized (mSoundIDs) {
                    mSoundIDs.clear();
                }
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
                                    synchronized (mSoundIDs) {
                                        if (mSoundIDs.size() > 0)
                                            mSoundPool.play(mSoundIDs.get(mRand.nextInt(mSoundIDs.size())), 1.0f, 1.0f, 0, 0, 1.0f);
                                    }
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

        findViewById(R.id.btn_pause_all).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mSoundPool.autoPause();
            }
        });

        findViewById(R.id.btn_resume_all).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mSoundPool.autoResume();
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.e(TAG,"onDestroy()");
    }

    @Override
    protected void finalize() throws Throwable {
        Log.e(TAG, "onFinalize()");
        super.finalize();
    }
}
