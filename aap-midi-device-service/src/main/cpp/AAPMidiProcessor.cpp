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
        owner->callPluginProcess();

        owner->fillAudioOutput(static_cast<float *>(audioData), numFrames);

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

        callback = std::make_unique<AAPOboeAudioCallback>(this);
        builder.setDataCallback(callback.get());
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

    // Instantiate AAP plugin and proceed up to prepare().
    void AAPMidiProcessor::instantiatePlugin(std::string pluginId) {
        if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
            aap::aprintf("Unexpected call to start() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        auto pluginInfo = host_manager.getPluginInformation(pluginId);
        int32_t numPorts = pluginInfo->getNumPorts();
        if (pluginInfo->isInstrument())
            if (instrument_instance_id != 0) {
                const auto& info = host->getInstance(instrument_instance_id)->getPluginInformation();
                aap::aprintf("Instrument instance %s is already assigned.",
                             info->getDisplayName().c_str());
                state = AAP_MIDI_PROCESSOR_STATE_ERROR;
                return;
            }

        auto instanceId = host->createInstance(pluginId, sample_rate);
        auto instance = host->getInstance(instanceId);

        if (pluginInfo->isInstrument())
            instrument_instance_id = instanceId;

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

            auto port = pluginInfo->getPort(i);
            if (port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO && port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT)
                data->audio_out_ports.emplace_back(i);
            else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                data->midi2_in_port = i;
            else if (port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI && port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                data->midi1_in_port = i;
        }

        instance->prepare(plugin_frame_size, data->plugin_buffer.get());

        instance_data_list.emplace_back(std::move(data));
    }

    // Activate audio processing. CPU-intensive operations happen from here.
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

    // Deactivate audio processing. CPU-intensive operations stop here.
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

    // Called by Oboe audio callback implementation. It calls process.
    void AAPMidiProcessor::callPluginProcess() {
        for (auto &data : instance_data_list)
            if (data->instance_id == instrument_instance_id)
                host->getInstance(instrument_instance_id)->process(data->plugin_buffer.get(), 1000000000);
    }

    // Called by Oboe audio callback implementation. It is called after AAP processing, and
    //  fill the audio outputs, interleaving the results.
    void AAPMidiProcessor::fillAudioOutput(float* outputData, size_t numFramesOnAllChannels) {
        // FIXME: the final processing result should not be the instrument instance output buffer
        //  but should be chained output result. Right now we don't support chaining.
        for (auto &data : instance_data_list) {
            if (data->instance_id == instrument_instance_id) {
                int numChannels = data->audio_out_ports.size();
                size_t numFramesByOboe = numFramesOnAllChannels / numChannels;
                size_t numFramesByAAP = data->plugin_buffer->num_frames;
                size_t numFrames =
                        numFramesByOboe > numFramesByAAP ? numFramesByAAP : numFramesByOboe;
                int numPorts = data->audio_out_ports.size();
                for (int p = 0; p < numPorts; p++) {
                    int portIndex = data->audio_out_ports[p];
                    auto src = (float*) data->plugin_buffer->buffers[portIndex];
                    // We have to interleave separate port outputs to copy...
                    for (int i = 0; i < numFrames; i++)
                        outputData[i * numChannels + p] = src[i];
                }
            }
        }
    }

    int getTicksFromNanoseconds(int deltaTimeSpec, uint64_t value) {
        // BPM 120 = 120 quarter notes per minute.
        // 96 ticks per second when deltaTimeSpec is 192
        int tempo = 500000; // BPM 120
        double seconds = value / 1000000000.0;
        auto ticksPerNanoSecond = seconds * (deltaTimeSpec / 4) * tempo / 60;
        return (int) ticksPerNanoSecond;
    }

    int set7BitEncodedLength(uint8_t* buffer, int value) {
        int pos = 0;
        do {
            buffer[pos] = value % 80;
            value /= 0x80;
            if (value > 0)
                buffer[pos] &= 0x80;
            pos++;
        } while (value != 0);
        return pos;
    }

    void* AAPMidiProcessor::getAAPMidiInputBuffer() {
        for (auto &data : instance_data_list)
            if (data->instance_id == instrument_instance_id)
                return data->plugin_buffer->buffers[data->midi1_in_port];
        return nullptr;
    }

    void AAPMidiProcessor::processMidiInput(uint8_t* bytes, size_t offset, size_t length, uint64_t timestampInNanoseconds) {
        //aap::aprintf("!!! AAPMIDI: processMessage() !!!: %d %d %d", bytes[0], bytes [1], length > 2 ? bytes[2] : 0);
        int ticks = getTicksFromNanoseconds(192, timestampInNanoseconds);

        auto dst = (uint8_t*) getAAPMidiInputBuffer();
        if (dst != nullptr) {
            auto intBuffer = (int32_t *) (void *) dst;
            intBuffer[0] = 192; // FIXME: assign DeltaTimeSpec from somewhere.
            intBuffer[1] = length;
            int aapBufferOffset = sizeof(int) + sizeof(int);
            aapBufferOffset += set7BitEncodedLength(dst + aapBufferOffset, ticks);
            memcpy(dst + aapBufferOffset, bytes + offset, length);
        }
    }
}
