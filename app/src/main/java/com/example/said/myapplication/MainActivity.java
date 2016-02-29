package com.example.said.myapplication;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;

import android.annotation.TargetApi;
import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class MainActivity extends Activity {

	Button startRec, stopRec, playBack;
	int minBufferSizeIn, minBufferSizeOut;
	AudioRecord audioRecord;
	AudioTrack audioTrack;
	short[] audioData;
	Boolean recording;
	int sampleRateInHz = 44100;
	private String TAG = "MainActivity";

	static {
		System.loadLibrary("nativeCalls");
	}
	public native void start();
	public native void stop();

	AudioManager manager;

	/**
	 * Called when the activity is first created.
	 */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		startRec = (Button) findViewById(R.id.startrec);
		stopRec = (Button) findViewById(R.id.stoprec);
		playBack = (Button) findViewById(R.id.playback);

		startRec.setOnClickListener(startRecOnClickListener);
		stopRec.setOnClickListener(stopRecOnClickListener);
		playBack.setOnClickListener(playBackOnClickListener);
//		playBack.setEnabled(false);
//		startRec.setEnabled(true);
//		stopRec.setEnabled(false);
////		stopRec.setText(start());

		minBufferSizeIn = AudioRecord.getMinBufferSize(sampleRateInHz,
				AudioFormat.CHANNEL_IN_MONO,
				AudioFormat.ENCODING_PCM_16BIT);

		audioData = new short[minBufferSizeIn];

		minBufferSizeOut = AudioTrack.getMinBufferSize(sampleRateInHz,
				AudioFormat.CHANNEL_OUT_MONO,
				AudioFormat.ENCODING_PCM_16BIT);
		audioTrack = new AudioTrack(
				AudioManager.STREAM_MUSIC, sampleRateInHz,
				AudioFormat.CHANNEL_OUT_MONO,
				AudioFormat.ENCODING_PCM_16BIT,
				minBufferSizeOut,
				AudioTrack.MODE_STREAM);
		audioTrack.play();

		manager = (AudioManager) getSystemService(AUDIO_SERVICE);
		manager.setMode(AudioManager.MODE_IN_COMMUNICATION);
//		manager.setSpeakerphoneOn(true);
//		manager.

	}
	
	OnClickListener startRecOnClickListener
			= new OnClickListener() {

		@Override
		public void onClick(View arg0) {
//			playBack.setEnabled(false);
//			startRec.setEnabled(false);
//			stopRec.setEnabled(true);
//			manager.setMode(AudioManager.MODE_IN_COMMUNICATION);
			manager.setSpeakerphoneOn(true);
			Thread recordThread = new Thread(new Runnable() {

				@Override
				public void run() {
					recording = true;
//					startRecord();
					start();
				}

			});

			recordThread.start();

		}
	};


	OnClickListener stopRecOnClickListener
			= new OnClickListener() {

		@Override
		public void onClick(View arg0) {
//			playBack.setEnabled(true);
//			startRec.setEnabled(false);
//			stopRec.setEnabled(false);
			recording = false;
//			manager.setMode(AudioManager.MODE_IN_COMMUNICATION);
			manager.setSpeakerphoneOn(true);
			stop();
		}
	};

	
	
	OnClickListener playBackOnClickListener
			= new OnClickListener() {

		@Override
		public void onClick(View v) {
//			playBack.setEnabled(false);
//			startRec.setEnabled(true);
//			stopRec.setEnabled(false);
			Thread playThread = new Thread(new Runnable() {
				@Override
				public void run() {
					playRecord();
				}
			});

			playThread.start();
		}

	};


	@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
	private void startRecord() {
		File file = new File(Environment.getExternalStorageDirectory(), "start.pcm");

		try {
			FileOutputStream outputStream = new FileOutputStream(file);
			BufferedOutputStream bufferedOutputStream = new BufferedOutputStream(outputStream);
			DataOutputStream dataOutputStream = new DataOutputStream(bufferedOutputStream);

			if (audioRecord != null){
				audioRecord.release();
				audioRecord = null;
			}
			audioRecord = new AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION,
					sampleRateInHz,
					AudioFormat.CHANNEL_IN_MONO,
					AudioFormat.ENCODING_PCM_16BIT,
					minBufferSizeIn);

			AcousticEchoCanceler aec = null;
			NoiseSuppressor ns = null;

			if (AcousticEchoCanceler.isAvailable()) {
				aec = AcousticEchoCanceler.create(audioRecord.getAudioSessionId());
				if (aec != null) {
					aec.setEnabled(true);
					Log.d(TAG, "AudioInput: AcousticEchoCanceler.isAvailable(): " + AcousticEchoCanceler.isAvailable() + "| isEnabled: " + aec.getEnabled() + "| isControling :" + aec.hasControl());
				} else {
					Log.e(TAG, "AudioInput: AcousticEchoCanceler is null and not enabled");
				}
			}

			if (NoiseSuppressor.isAvailable()) {
				ns = NoiseSuppressor.create(audioRecord.getAudioSessionId());
				if (ns != null) {
					ns.setEnabled(true);
				} else {
					Log.e(TAG, "AudioInput: NoiseSuppressor is null and not enabled");
				}
			}
			audioRecord.startRecording();

			while (recording) {
				int numberOfShort = audioRecord.read(audioData, 0, minBufferSizeIn);
				for (int i = 0; i < numberOfShort; i++) {
					dataOutputStream.writeShort(audioData[i]);
				}
			}

			audioRecord.stop();
			dataOutputStream.close();
			if(aec !=null) aec.release();
			if(ns !=null) ns.release();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}


	void playRecord() {
		File file = new File(Environment.getExternalStorageDirectory(), "test.pcm");
		DataInputStream dataInputStream;

		try {
			FileInputStream inputStream = new FileInputStream(file);
			BufferedInputStream bufferedInputStream = new BufferedInputStream(inputStream);
			dataInputStream = new DataInputStream(bufferedInputStream);
//			ArrayList<short[]> shortsArray = new ArrayList<>();
			ArrayList<byte[]> bytesArray = new ArrayList<>();

			while (dataInputStream.available() > 0){
//				short[] audioData = new short[minBufferSizeOut];
				byte[] audioData = new byte[minBufferSizeOut];
//				shortsArray.add(audioData);
				bytesArray.add(audioData);
				int j = 0;
				while (dataInputStream.available() > 0 && j < minBufferSizeOut) {
//					audioData[j] = dataInputStream.readShort();
					audioData[j] = dataInputStream.readByte();
					j++;
				}
			}
			dataInputStream.close();


//			for (int i = 0; i < shortsArray.size(); ++i) {
//				audioTrack.write(shortsArray.get(i), 0, minBufferSizeOut);
//			}
			for (int i = 0; i < bytesArray.size(); i++) {
				audioTrack.write(bytesArray.get(i), 0, minBufferSizeOut);
			}



		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

}