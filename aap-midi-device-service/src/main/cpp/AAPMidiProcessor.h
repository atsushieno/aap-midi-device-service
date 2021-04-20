
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

    struct PluginInstanceData {
        PluginInstanceData(int instanceId, size_t numPorts) : instance_id(instanceId) {
            buffer_pointers.reset((void**) calloc(sizeof(void*), numPorts));
        }

        int instance_id;
        std::vector<int> portSharedMemoryFDs{};
        std::unique_ptr<AndroidAudioPluginBuffer> plugin_buffer;
        std::unique_ptr<void*> buffer_pointers{nullptr};
    };

    class AAPMidiProcessor {
        static std::string convertStateToText(AAPMidiProcessorState state);

        // AAP
        aap::PluginHostManager host_manager{};
        std::unique_ptr<aap::PluginHost> host{nullptr};
        int sample_rate{0};
        int plugin_frame_size{1024};
        std::vector<std::unique_ptr<PluginInstanceData>> instance_data_list{};

        // Oboe
        oboe::AudioStreamBuilder builder;
        AAPOboeAudioCallback callback;
        std::shared_ptr<oboe::AudioStream> stream;
        AAPMidiProcessorState state;

    public:
        static AAPMidiProcessor* getInstance();

        void initialize(int32_t sampleRate, int32_t frameSize);

        static void registerPluginService(const aap::AudioPluginServiceConnection service);

        void instantiatePlugin(std::string pluginId);

        void activate();

        void processMessage(uint8_t* bytes, size_t offset, size_t length, uint64_t timestamp);

        void deactivate();

        void terminate();
    };
}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
