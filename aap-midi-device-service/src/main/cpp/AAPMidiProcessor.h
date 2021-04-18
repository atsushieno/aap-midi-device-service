
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H

#include <oboe/Oboe.h>

class AAPOboeAudioCallback : public oboe::AudioStreamDataCallback {
public:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
};

enum AAPMidiProcessorState {
    AAP_MIDI_PROCESSOR_STATE_CREATED,
    AAP_MIDI_PROCESSOR_STATE_STARTED,
    AAP_MIDI_PROCESSOR_STATE_STOPPED,
    AAP_MIDI_PROCESSOR_STATE_ERROR
};

class AAPMidiProcessor {
    static std::string convertStateToText(AAPMidiProcessorState state);

    oboe::AudioStreamBuilder builder;
    AAPOboeAudioCallback callback;
    std::shared_ptr<oboe::AudioStream> stream;
    AAPMidiProcessorState state;

public:

    void initialize();

    void start();
};


#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
