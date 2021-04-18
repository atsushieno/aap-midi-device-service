
#include <aap/audio-plugin-host.h>
#include <aap/android-context.h>




// JNI entrypoints

extern "C" {

void Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_initializeReceiverNative(
        JNIEnv *env, jobject midiReceiver, jobject applicationContext) {
    aap::set_application_context(env, applicationContext);
}

void Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_terminateReceiverNative(
        JNIEnv *env, jobject midiReceiver) {
    aap::unset_application_context(env);
}

void Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_activate(
        JNIEnv *env, jobject midiReceiver) {
}

void Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_deactivate(
        JNIEnv *env, jobject midiReceiver) {
}

void Java_org_androidaudioplugin_midideviceservice_AudioPluginMidiReceiver_processMessage(
        JNIEnv *env, jobject midiReceiver, jobject bytes, jint offset, jint length,
        jlong timestamp) {
}

} // extern "C"
