#include <sys/mman.h>
#include <android/sharedmem.h>
#include <aap/logging.h>
#include <aap/audio-plugin-host-android.h>
#include "AAPMidiProcessor.h"

namespace aapmidideviceservice {

    AAPMidiProcessor processor{};

    AAPMidiProcessor* AAPMidiProcessor::getInstance() {
        return &processor;
    }

    oboe::DataCallbackResult AAPOboeAudioCallback::onAudioReady(
            oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
        auto *outputData = static_cast<float *>(audioData);

        // FIXME: replace this oboe sample stub with the actual AAP processing results.
        const float amplitude = 0.2f;
        for (int i = 0; i < numFrames; ++i) {
            outputData[i] = ((float) drand48() - 0.5f) * 2 * amplitude;
        }



        return oboe::DataCallbackResult::Continue;
    }

    void AAPMidiProcessor::initialize(int32_t sampleRate, int32_t pluginFrameSize) {
        // AAP settings
        host = std::make_unique<aap::PluginHost>(&host_manager);
        sample_rate = sampleRate;
        plugin_frame_size = pluginFrameSize;

        // Oboe configuration
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(oboe::ChannelCount::Stereo);

        builder.setDataCallback(&callback);
    }

    void AAPMidiProcessor::terminate() {

        // free shared memory buffers and close FDs for the instances.
        // FIXME: shouldn't androidaudioplugin implement this functionality so that we don't have
        //  to manage it everywhere? It is also super error prone.
        for (auto& data : instance_data_list) {
            int numBuffers = data->plugin_buffer->num_buffers;
            for (int n = 0; n < numBuffers; n++) {
                munmap(data->buffer_pointers.get()[n], data->plugin_buffer->num_frames * sizeof(float));
                int fd = data->portSharedMemoryFDs[n];
                if (fd != 0)
                    close(fd);
            }
        }

        host.reset();
    }

    std::string AAPMidiProcessor::convertStateToText(AAPMidiProcessorState stateValue) {
        switch (stateValue) {
            case AAP_MIDI_PROCESSOR_STATE_CREATED:
                return "CREATED";
            case AAP_MIDI_PROCESSOR_STATE_STARTED:
                return "STARTED";
            case AAP_MIDI_PROCESSOR_STATE_STOPPED:
                return "STOPPED";
            case AAP_MIDI_PROCESSOR_STATE_ERROR:
                return "ERROR";
        }
        return "(UNKNOWN)";
    }

    void AAPMidiProcessor::registerPluginService(const aap::AudioPluginServiceConnection service) {
        // FIXME: it's better to use dynamic_cast but the actual entity is not a pointer,
        //  a reference to a field, dynamic_cast<>() returns NULL and this it fails.
        auto pal = (aap::AndroidPluginHostPAL*) aap::getPluginHostPAL();
        pal->serviceConnections.emplace_back(service);
    }

    void AAPMidiProcessor::instantiatePlugin(std::string pluginId) {
        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::aprintf("Unexpected call to start() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto pluginInfo = host_manager.getPluginInformation(pluginId);
        int32_t numPorts = pluginInfo->getNumPorts();

        auto instanceId = host->createInstance(pluginId, sample_rate);
        auto instance = host->getInstance(instanceId);

        auto data = std::make_unique<PluginInstanceData>(instanceId, numPorts);

        instance->completeInstantiation();

        auto sharedMemoryExtension = (aap::SharedMemoryExtension*) instance->getExtension(aap::SharedMemoryExtension::URI);

        auto buffer = std::make_unique<AndroidAudioPluginBuffer>();
        buffer->num_buffers = numPorts;
        buffer->num_frames = plugin_frame_size;

        size_t memSize = buffer->num_frames * sizeof(float);

        data->instance_id = instanceId;
        data->plugin_buffer = std::move(buffer);
        data->plugin_buffer->buffers = data->buffer_pointers.get();


        for (int i = 0; i < numPorts; i++) {
            int fd = ASharedMemory_create(nullptr, memSize);
            data->portSharedMemoryFDs.emplace_back(fd);
            sharedMemoryExtension->getPortBufferFDs().emplace_back(fd);
            data->plugin_buffer->buffers[i] = mmap(nullptr, memSize,
                                                         PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        }

        instance->prepare(plugin_frame_size, data->plugin_buffer.get());

        instance_data_list.emplace_back(std::move(data));
    }

    void AAPMidiProcessor::activate() {
        // start Oboe stream.
        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::aprintf("Unexpected call to start() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        oboe::Result result = builder.openStream(stream);
        if (result != oboe::Result::OK) {
            aap::aprintf("Failed to create Oboe stream: %s", oboe::convertToText(result));
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        stream->requestStart();

        // activate instances
        for (int i = 0; i < host->getInstanceCount(); i++)
            host->getInstance(i)->activate();

        state = AAP_MIDI_PROCESSOR_STATE_STARTED;
    }

    void AAPMidiProcessor::deactivate() {

        // deactivate instances
        for (int i = 0; i < host->getInstanceCount(); i++)
            host->getInstance(i)->deactivate();

        // close Oboe stream.
        stream->stop();
        stream->close();
        stream.reset();

        state = AAP_MIDI_PROCESSOR_STATE_STOPPED;
    }

    void AAPMidiProcessor::processMessage(uint8_t* bytes, size_t offset, size_t length, uint64_t timestamp) {
        aap::aprintf("!!! TODO: IMPLEMENT processMessage() !!!: %d %d %d", bytes[0], bytes [1], length > 2 ? bytes[2] : 0);
    }
}
