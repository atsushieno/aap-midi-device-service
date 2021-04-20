
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <aap/audio-plugin-host.h>
#include <aap/audio-plugin-host-android.h>
#include <aap/android-context.h>
#include <aap/logging.h>
#include "AAPMidiProcessor.h"


// JNI entrypoints

// This returns std::string by value. Do not use for large chunk of strings.
std::string stringFromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return "";
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    auto ret = std::string{u8};
    env->ReleaseStringUTFChars(s, u8);
    return ret;
}

extern "C" {

#define AAPMIDIDEVICE_INSTANCE aapmidideviceservice::AAPMidiProcessor::getInstance()

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_initializeReceiverNative(
        JNIEnv *env, jobject midiReceiver, jobject applicationContext, jint sampleRate, jint frameSize, jint audioOutChannelCount) {
    AAPMIDIDEVICE_INSTANCE->initialize(sampleRate, frameSize, audioOutChannelCount);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_registerPluginService(
        JNIEnv *env, jobject midiReceiver, jobject binder, jstring packageName, jstring className) {
    std::string packageNameString = stringFromJava(env, packageName);
    std::string classNameString = stringFromJava(env, className);
    auto binderRef = env->NewGlobalRef(binder);
    auto aiBinder = AIBinder_fromJavaBinder(env, binder);

    AAPMIDIDEVICE_INSTANCE->registerPluginService(
            aap::AudioPluginServiceConnection(packageNameString, classNameString, binderRef, aiBinder));
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_terminateReceiverNative(
        JNIEnv *env, jobject midiReceiver) {
    aap::unset_application_context(env);

    AAPMIDIDEVICE_INSTANCE->terminate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_activate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->activate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_deactivate(
        JNIEnv *env, jobject midiReceiver) {
    AAPMIDIDEVICE_INSTANCE->deactivate();
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_instantiatePlugin(
        JNIEnv *env, jobject midiReceiver, jstring pluginId) {
    std::string pluginIdString = stringFromJava(env, pluginId);

    AAPMIDIDEVICE_INSTANCE->instantiatePlugin(pluginIdString);
}

jbyte jni_midi_buffer[1024]{};

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_processMessage(
        JNIEnv *env, jobject midiReceiver, jbyteArray bytes, jint offset, jint length,
        jlong timestampInNanoseconds) {
    env->GetByteArrayRegion(bytes, offset, length, jni_midi_buffer);
    AAPMIDIDEVICE_INSTANCE->processMidiInput(
            reinterpret_cast<uint8_t *>(jni_midi_buffer), 0, length, timestampInNanoseconds);
}

} // extern "C"
