
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H

#include <oboe/Oboe.h>

namespace aapmidideviceservice {

    class AAPOboeAudioCallback : public oboe::AudioStreamDataCallback {
    public:
        oboe::DataCallbackResult
        onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
    };

    enum AAPMidiProcessorState {
        AAP_MIDI_PROCESSOR_STATE_CREATED,
        AAP_MIDI_PROCESSOR_STATE_STARTED,
        AAP_MIDI_PROCESSOR_STATE_STOPPED,
        AAP_MIDI_PROCESSOR_STATE_ERROR
    };

    class AAPMidiProcessor {
        static std::string convertStateToText(AAPMidiProcessorState state);

        // AAP
        aap::PluginHostManager host_manager{};

        // Oboe
        oboe::AudioStreamBuilder builder;
        AAPOboeAudioCallback callback;
        std::shared_ptr<oboe::AudioStream> stream;
        AAPMidiProcessorState state;

    public:
        static AAPMidiProcessor* getInstance();

        void initialize(int32_t sampleRate);

        void addPluginService(const aap::AudioPluginServiceConnection service);

        void instantiatePlugin(std::string pluginId);

        void activate();

        void processMessage(uint8_t* bytes, size_t offset, size_t length, uint64_t timestamp);

        void deactivate();

        void terminate();
    };
}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
