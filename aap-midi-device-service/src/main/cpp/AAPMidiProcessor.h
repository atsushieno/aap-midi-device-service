
#ifndef AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
#define AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H

#include <oboe/Oboe.h>
#include <zix/ring.h>
#include <cmidi2.h>

namespace aapmidideviceservice {

    enum AAPMidiProcessorState {
        AAP_MIDI_PROCESSOR_STATE_CREATED,
        AAP_MIDI_PROCESSOR_STATE_ACTIVE,
        AAP_MIDI_PROCESSOR_STATE_INACTIVE,
        AAP_MIDI_PROCESSOR_STATE_STOPPED,
        AAP_MIDI_PROCESSOR_STATE_ERROR
    };

    struct PluginInstanceData {
        PluginInstanceData(int instanceId, size_t numPorts) : instance_id(instanceId) {
            buffer_pointers.reset((void**) calloc(sizeof(void*), numPorts));
        }

        int instance_id;
        int midi1_in_port{-1};
        int midi2_in_port{-1};
        std::vector<int> audio_out_ports{};
        std::vector<int> portSharedMemoryFDs{};
        std::unique_ptr<AndroidAudioPluginBuffer> plugin_buffer;
        std::unique_ptr<void*> buffer_pointers{nullptr};
    };

    class AAPMidiProcessor {

        class OboeCallback : public oboe::AudioStreamDataCallback {
        public:
            OboeCallback(AAPMidiProcessor *owner) : owner(owner) {}

            AAPMidiProcessor *owner;
            oboe::DataCallbackResult
            onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);
        };

        static std::string convertStateToText(AAPMidiProcessorState state);

        // AAP
        aap::PluginHostManager host_manager{};
        std::unique_ptr<aap::PluginHost> host{nullptr};
        int sample_rate{0};
        int aap_frame_size{4096};
        int channel_count{2};
        std::vector<std::unique_ptr<PluginInstanceData>> instance_data_list{};
        int instrument_instance_id{0};
        int32_t midi_protocol{CMIDI2_PROTOCOL_TYPE_MIDI1};

        PluginInstanceData* getAAPMidiInputData();
        void* getAAPMidiInputBuffer();

        // Oboe
        oboe::AudioStreamBuilder builder{};
        std::unique_ptr<oboe::AudioStreamDataCallback> callback{};
        std::shared_ptr<oboe::AudioStream> stream{};
        AAPMidiProcessorState state{AAP_MIDI_PROCESSOR_STATE_CREATED};

        ZixRing *aap_input_ring_buffer{nullptr};
        float *interleave_buffer{nullptr};
        struct timespec last_aap_process_time{};
    public:
        static AAPMidiProcessor* getInstance();
        static void resetInstance();

        void initialize(int32_t sampleRate, int32_t oboeFrameSize, int32_t channelCount, int32_t aapFrameSize);

        static void registerPluginService(std::unique_ptr<aap::AudioPluginServiceConnection> service);

        void instantiatePlugin(std::string pluginId);

        void activate();

        void processMidiInput(uint8_t* bytes, size_t offset, size_t length, int64_t timestampInNanoseconds);

        void setMidiProtocol(int32_t midiProtocol);

        void callPluginProcess();

        void fillAudioOutput();

        void deactivate();

        void terminate();
    };
}

#endif //AAP_MIDI_DEVICE_SERVICE_AAPMIDIPROCESSOR_H
