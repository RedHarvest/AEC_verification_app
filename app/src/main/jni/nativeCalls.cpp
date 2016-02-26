#include <jni.h>
#include<string.h>
#include <stdio.h>
#include <android/log.h>
#include <sys/types.h>
static void log(android_LogPriority priority, const char * string, ...) {
	char buffer[1000 * 16];
	va_list argptr;
	va_start(argptr, string);
	vsnprintf(buffer, 1000 * 16, string, argptr);
	va_end(argptr);
	__android_log_print(priority, "NativeCalls", buffer);
}

static unsigned int get_supported_rate(JNIEnv *jni_env, unsigned int prefered_rate) {
	jclass audio_record_class = jni_env->FindClass("android/media/AudioRecord");
	int size = jni_env->CallStaticIntMethod(audio_record_class
			,jni_env->GetStaticMethodID(audio_record_class,"getMinBufferSize", "(III)I")
			,prefered_rate
			,2/*CHANNEL_CONFIGURATION_MONO*/
			,2/*  ENCODING_PCM_16BIT */);


	if (size > 0) {
		return prefered_rate;
	} else {
		log(ANDROID_LOG_ERROR, "Cannot configure recorder with rate [%i]", prefered_rate, 10);
//		log("could not configure rate " + prefered_rate);
		if (prefered_rate>48000) {
			return get_supported_rate(jni_env, 48000);
		}
		switch (prefered_rate) {
			case 12000:
			case 24000: return get_supported_rate(jni_env, 48000);
			case 48000: return get_supported_rate(jni_env, 44100);
			case 44100: return get_supported_rate(jni_env, 16000);
			case 16000: return get_supported_rate(jni_env, 8000);
			default: {
				log(ANDROID_LOG_ERROR, "This Android sound card doesn't support any standard sample rate");
				return 0;
			}
		}
	}

}


static bool recording = false;


jobject enable_hardware_echo_canceller(JNIEnv *env, int sessionId) {
	jobject aec = NULL;
	jclass aecClass = env->FindClass("android/media/audiofx/AcousticEchoCanceler");
	if (aecClass==NULL){
		log(ANDROID_LOG_ERROR, "Couldn't find android/media/audiofx/AcousticEchoCanceler class !");
		env->ExceptionClear(); //very important.
		return NULL;
	}
	//aecClass= (jclass)env->NewGlobalRef(aecClass);
	jmethodID isAvailableID = env->GetStaticMethodID(aecClass,"isAvailable","()Z");
	if (isAvailableID!=NULL){
		jboolean ret=env->CallStaticBooleanMethod(aecClass,isAvailableID);
		if (ret){
			jmethodID createID = env->GetStaticMethodID(aecClass,"create","(I)Landroid/media/audiofx/AcousticEchoCanceler;");
			if (createID!=NULL){
				aec=env->CallStaticObjectMethod(aecClass,createID,sessionId);
				if (aec){
					aec=env->NewGlobalRef(aec);
					log(ANDROID_LOG_DEBUG, "AcousticEchoCanceler successfully created.");
					jclass effectClass=env->FindClass("android/media/audiofx/AudioEffect");
					if (effectClass){
						//effectClass=(jclass)env->NewGlobalRef(effectClass);
						jmethodID isEnabledID = env->GetMethodID(effectClass,"getEnabled","()Z");
						jmethodID setEnabledID = env->GetMethodID(effectClass,"setEnabled","(Z)I");
						if (isEnabledID && setEnabledID){
							jboolean enabled=env->CallBooleanMethod(aec,isEnabledID);
							log(ANDROID_LOG_DEBUG, "AcousticEchoCanceler enabled: %i",(int)enabled);
							if (!enabled){
								int ret=env->CallIntMethod(aec,setEnabledID,1);
								if (ret!=0){
									log(ANDROID_LOG_ERROR, "Could not enable AcousticEchoCanceler: %i",ret);
								} else {
									log(ANDROID_LOG_DEBUG, "AcousticEchoCanceler enabled");
								}
							} else {
								log(ANDROID_LOG_WARN, "AcousticEchoCanceler already enabled");
							}
						} else {
							log(ANDROID_LOG_ERROR,"Couldn't find either getEnabled or setEnabled method in AudioEffect class for AcousticEchoCanceler !");
						}
						env->DeleteLocalRef(effectClass);
					} else {
						log(ANDROID_LOG_ERROR,"Couldn't find android/media/audiofx/AudioEffect class !");
					}
				}else{
					log(ANDROID_LOG_ERROR,"Failed to create AcousticEchoCanceler !");
				}
			}else{
				log(ANDROID_LOG_ERROR, "create() not found in class AcousticEchoCanceler !");
				env->ExceptionClear(); //very important.
			}
		} else {
			log(ANDROID_LOG_ERROR, "AcousticEchoCanceler isn't available !");
		}
	}else{
		log(ANDROID_LOG_ERROR, "isAvailable() not found in class AcousticEchoCanceler !");
		env->ExceptionClear(); //very important.
	}
	env->DeleteLocalRef(aecClass);
	return aec;
}

void delete_hardware_echo_canceller(JNIEnv *env, jobject aec) {
	env->DeleteGlobalRef(aec);
}


extern "C" {

JNIEXPORT void JNICALL
Java_com_example_said_myapplication_MainActivity_stop(JNIEnv *env, jobject instance) {
	recording = false;
}

JNIEXPORT void JNICALL Java_com_example_said_myapplication_MainActivity_start(JNIEnv *jni_env, jobject obj) {
	jmethodID constructor_id=0;
	jmethodID min_buff_size_id;
	int buff_size, read_chunk_size, rate = 48000;
	jbyteArray read_buff;
	jobject audio_record;
	jclass audioRecordClass =  (jclass) jni_env->NewGlobalRef(jni_env->FindClass("android/media/AudioRecord"));
	if (audioRecordClass == 0) {
		log(ANDROID_LOG_ERROR, "audioRecordClass == 0");
		return;
	}
	constructor_id = jni_env->GetMethodID(audioRecordClass, "<init>", "(IIIII)V");
	if (constructor_id == 0) {
		log(ANDROID_LOG_ERROR, "constructor_id == 0");
		return;
	}
	min_buff_size_id = jni_env->GetStaticMethodID(audioRecordClass, "getMinBufferSize",
												  "(III)I");
	if (min_buff_size_id == 0) {
		log(ANDROID_LOG_ERROR, "min_buff_size_id == 0");
		return;
	}
	buff_size = jni_env->CallStaticIntMethod(audioRecordClass, min_buff_size_id, rate,
											 2/*CHANNEL_CONFIGURATION_MONO*/,
											 2/*  ENCODING_PCM_16BIT */);
	read_chunk_size = buff_size / 4;
	buff_size *= 2;/*double the size for configuring the recorder: this does not affect latency but prevents "AudioRecordThread: buffer overflow"*/
	if (buff_size > 0) {
		log(ANDROID_LOG_VERBOSE,
							"Configuring recorder with [%i] bits  rate [%i] nchanels [%i] buff size [%i], chunk size [%i]",
							16, rate, 1, buff_size, read_chunk_size);
	} else {
		log(ANDROID_LOG_VERBOSE,
							"Cannot configure recorder with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]",
							16, rate, 1, buff_size, read_chunk_size);
		return;
	}
	read_buff = jni_env->NewByteArray(buff_size);
//	read_buff = (jbyte*) jni_env->NewGlobalRef((jbyte*)  read_buff);
	if (read_buff == 0) {
		log(ANDROID_LOG_ERROR, "cannot instanciate read buff");
		return;
	}
	audio_record = jni_env->NewObject(audioRecordClass, constructor_id
			, 7/*VOICE_COMMUNICATION*/
			, rate
			, 2/*CHANNEL_CONFIGURATION_MONO*/
			, 2/*  ENCODING_PCM_16BIT */
			, buff_size);
	log(ANDROID_LOG_VERBOSE,  "Actual samplerate: %i, buff_size %i", rate, buff_size);
	audio_record = jni_env->NewGlobalRef(audio_record);
	if (audio_record == 0) {
		log(ANDROID_LOG_ERROR, "cannot instantiate AudioRecord");
		return;
	}
	log(ANDROID_LOG_ERROR, "You made it");


	jmethodID get_session_id = jni_env->GetMethodID(audioRecordClass, "getAudioSessionId",
												  "()I");
	if (get_session_id == 0) {
		log(ANDROID_LOG_ERROR, "get_session_id == 0");
		return;
	}
	int session_id = jni_env->CallIntMethod(audio_record, get_session_id);
	log(ANDROID_LOG_DEBUG, "session_id=%d", session_id);

	jobject aec = enable_hardware_echo_canceller(jni_env, session_id);

	jmethodID start_recording_id = jni_env->GetMethodID(audioRecordClass, "startRecording", "()V");
	if (start_recording_id == 0) {
		log(ANDROID_LOG_ERROR, "start_recording_id == 0");
		return;
	}
	jmethodID stop_recording_id = jni_env->GetMethodID(audioRecordClass, "stop", "()V");
	if (stop_recording_id == 0) {
		log(ANDROID_LOG_ERROR, "stop_recording_id == 0");
		return;
	}

	jni_env->CallVoidMethod(audio_record, start_recording_id);
	recording = true;

	jmethodID read_id = jni_env->GetMethodID(audioRecordClass, "read", "([BII)I");
	if (read_id == 0) {
		log(ANDROID_LOG_ERROR, "read_id == 0");
		return;
	}

	FILE *file = fopen("/storage/emulated/0/test.pcm", "w+");
	if (file == NULL) {
		log(ANDROID_LOG_ERROR, "file null");
		return;
	}

	int nread = 0;
	jbyte *tmp;
	while (recording && (nread = jni_env->CallIntMethod(audio_record, read_id, read_buff, 0, read_chunk_size)) > 0) {
		tmp = (jbyte*) malloc(nread * sizeof(jbyte));
		jni_env->GetByteArrayRegion(read_buff, 0, nread, tmp);
		fwrite(tmp, sizeof(jbyte), (size_t) nread, file);
		log(ANDROID_LOG_DEBUG, "num read: %i buffsize %i", (size_t) nread, sizeof(tmp));
		free(tmp);
	}

	jni_env->CallVoidMethod(audio_record, stop_recording_id);

	fflush(file);
	fclose(file);

	if (aec) {
		delete_hardware_echo_canceller(jni_env, aec);
		aec = NULL;
	}
}

}