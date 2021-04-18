
#include <jni.h>
#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>

#include <aap/audio-plugin-host.h>
#include <aap/audio-plugin-host-android.h>
#include <aap/android-context.h>



// JNI entrypoints


std::vector<aap::AudioPluginServiceConnection> serviceConnections{};

const char *strdup_fromJava(JNIEnv *env, jstring s) {
    jboolean isCopy;
    if (!s)
        return "";
    const char *u8 = env->GetStringUTFChars(s, &isCopy);
    auto ret = strdup(u8);
    env->ReleaseStringUTFChars(s, u8);
    return ret;
}

extern "C" {

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_initializeReceiverNative(
        JNIEnv *env, jobject midiReceiver, jobject applicationContext) {
    aap::set_application_context(env, applicationContext);

}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_addPluginService(
        JNIEnv *env, jobject midiReceiver, jobject binder, jstring packageName, jstring className) {
    // from native/androidaudioplugin/android/src/AudioPluginHost_native.cpp
    const char *packageNameDup = strdup_fromJava(env, packageName);
    const char *classNameDup = strdup_fromJava(env, className);
    auto binderRef = env->NewGlobalRef(binder);
    auto aiBinder = AIBinder_fromJavaBinder(env, binder);
    auto apal = dynamic_cast<aap::AndroidPluginHostPAL *>(aap::getPluginHostPAL());
    apal->serviceConnections.emplace_back(aap::AudioPluginServiceConnection(packageNameDup,
                                                                         classNameDup, binderRef,
                                                                         aiBinder));
    free((void *) packageNameDup);
    free((void *) classNameDup);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_terminateReceiverNative(
        JNIEnv *env, jobject midiReceiver) {
    aap::unset_application_context(env);
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_activate(
        JNIEnv *env, jobject midiReceiver) {
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_deactivate(
        JNIEnv *env, jobject midiReceiver) {
}

JNIEXPORT void JNICALL Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_processMessage(
        JNIEnv *env, jobject midiReceiver, jobject bytes, jint offset, jint length,
        jlong timestamp) {
    printf("!!! TODO: IMPLEMENT processMessage() !!!");
}

} // extern "C"
