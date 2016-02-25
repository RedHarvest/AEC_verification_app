#include <jni.h>
#include<string.h>

JNIEXPORT jstring JNICALL Java_com_example_said_myapplication_MainActivity_test(JNIEnv* env, jobject obj){



	return (*env)->NewStringUTF(env,"Hello world");

}
