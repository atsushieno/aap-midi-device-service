#include <sys/mman.h>
#include <android/sharedmem.h>
#include <aap/logging.h>
#include <aap/audio-plugin-host-android.h>
#include "AAPMidiProcessor.h"

namespace aapmidideviceservice {

    std::unique_ptr<AAPMidiProcessor> processor{};

    AAPMidiProcessor* AAPMidiProcessor::getInstance() {
        return processor.get();
    }

    void AAPMidiProcessor::resetInstance() {
        processor = std::make_unique<AAPMidiProcessor>();
    }

    long last_delay_value = 0, worst_delay_value = 0;
    long success_count = 0, failure_count = 0;

    oboe::DataCallbackResult AAPMidiProcessor::OboeCallback::onAudioReady(
            oboe::AudioStream *audioStream, void *audioData, int32_t oboeNumFrames) {

        if (owner->state != AAP_MIDI_PROCESSOR_STATE_ACTIVE)
            // it is not supposed to process audio at this state.
            // It is still possible that it gets called between Oboe requestStart()
            // and updating the state, so do not error out here.
            return oboe::DataCallbackResult::Continue;

        // Oboe audio buffer request can be extremely small (like < 10 frames).
        //  Therefore we don't call plugin process() every time (as it could cost hundreds
        //  of microseconds). Instead, we make use of ring buffer, call plugin process()
        //  only once in a while (when our ring buffer starves).
        //
        //  Each plugin process is still expected to fit within a callback time slice,
        //  so we still call plugin process() within the callback.

        if (zix_ring_read_space(owner->aap_input_ring_buffer) < oboeNumFrames) {
            // observer performance. (start)
            struct timespec ts_start, ts_end;
            clock_gettime(CLOCK_REALTIME, &ts_start);

            owner->callPluginProcess();

            // observer performance. (end)
            clock_gettime(CLOCK_REALTIME, &ts_end);
            long diff = (ts_end.tv_sec - ts_start.tv_sec) * 1000000000 + ts_end.tv_nsec - ts_start.tv_nsec;
            if (diff > 1000000) { // you took 1msec!?
                last_delay_value = diff;
                if (diff > worst_delay_value)
                    worst_delay_value = diff;
                failure_count++;
            } else success_count++;

            owner->fillAudioOutput();

        }

        zix_ring_read(owner->aap_input_ring_buffer, audioData, oboeNumFrames * sizeof(float));

        // FIXME: can we terminate it when it goes quiet?
        return oboe::DataCallbackResult::Continue;
    }

    void AAPMidiProcessor::initialize(int32_t sampleRate, int32_t oboeFrameSize, int32_t audioOutChannelCount, int32_t aapFrameSize) {
        // AAP settings
        host = std::make_unique<aap::PluginHost>(&host_manager);
        sample_rate = sampleRate;
        aap_frame_size = aapFrameSize;
        channel_count = audioOutChannelCount;

        aap_input_ring_buffer = zix_ring_new(aap_frame_size * audioOutChannelCount * sizeof(float) * 2); // twice as much as aap buffer size
        zix_ring_mlock(aap_input_ring_buffer);
        interleave_buffer = (float*) calloc(sizeof(float), aapFrameSize * audioOutChannelCount);

        // Oboe configuration
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(oboe::ChannelCount::Stereo);
        builder.setBufferCapacityInFrames(oboeFrameSize);
        builder.setContentType(oboe::ContentType::Music);

        callback = std::make_unique<OboeCallback>(this);
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
                data->portSharedMemoryFDs[n] = 0;
            }
        }

        zix_ring_free(aap_input_ring_buffer);
        free(interleave_buffer);

        host.reset();
    }

    std::string AAPMidiProcessor::convertStateToText(AAPMidiProcessorState stateValue) {
        switch (stateValue) {
            case AAP_MIDI_PROCESSOR_STATE_CREATED:
                return "CREATED";
            case AAP_MIDI_PROCESSOR_STATE_ACTIVE:
                return "ACTIVE";
            case AAP_MIDI_PROCESSOR_STATE_INACTIVE:
                return "INACTIVE";
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
        buffer->num_frames = aap_frame_size;

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
            else if (port->hasProperty(AAP_PORT_DEFAULT))
                *((float*) data->plugin_buffer->buffers[i]) = port->getDefaultValue();
        }

        instance->prepare(aap_frame_size, data->plugin_buffer.get());

        instance_data_list.emplace_back(std::move(data));
    }

    // Activate audio processing. CPU-intensive operations happen from here.
    void AAPMidiProcessor::activate() {
        // start Oboe stream.
        switch (state) {
            case AAP_MIDI_PROCESSOR_STATE_CREATED:
            case AAP_MIDI_PROCESSOR_STATE_INACTIVE:
                break;
            default:
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

        state = AAP_MIDI_PROCESSOR_STATE_ACTIVE;
    }

    // Deactivate audio processing. CPU-intensive operations stop here.
    void AAPMidiProcessor::deactivate() {
        if (state != AAP_MIDI_PROCESSOR_STATE_ACTIVE) {
            aap::aprintf("Unexpected call to deactivate() at %s state.",
                         convertStateToText(state).c_str());
            state = AAP_MIDI_PROCESSOR_STATE_ERROR;
            return;
        }

        // deactivate AAP instances
        for (int i = 0; i < host->getInstanceCount(); i++)
            host->getInstance(i)->deactivate();

        // close Oboe stream.
        stream->stop();
        stream->close();
        stream.reset();

        state = AAP_MIDI_PROCESSOR_STATE_INACTIVE;
    }

    // Called by Oboe audio callback implementation. It calls process.
    void AAPMidiProcessor::callPluginProcess() {
        for (auto &data : instance_data_list)
            host->getInstance(data->instance_id)->process(data->plugin_buffer.get(), 1000000000);
    }

    // Called by Oboe audio callback implementation. It is called after AAP processing, and
    //  fill the audio outputs into an intermediate buffer, interleaving the results,
    //  then copied into the ring buffer.
    void AAPMidiProcessor::fillAudioOutput() {
        // FIXME: the final processing result should not be the instrument instance output buffer
        //  but should be chained output result. Right now we don't support chaining.

        for (auto &data : instance_data_list) {
            if (data->instance_id == instrument_instance_id) {
                int numPorts = data->audio_out_ports.size();
                for (int p = 0; p < numPorts; p++) {
                    int portIndex = data->audio_out_ports[p];
                    auto src = (float*) data->plugin_buffer->buffers[portIndex];
                    // We have to interleave separate port outputs to copy...
                    for (int i = 0; i < aap_frame_size; i++)
                        interleave_buffer[i * numPorts + p] = src[i];
                }
            }
        }

        zix_ring_write(aap_input_ring_buffer, interleave_buffer, channel_count * aap_frame_size * sizeof(float));
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
            if (data->instance_id == instrument_instance_id) {
                auto portIndex = data->midi2_in_port >= 0 ? data->midi2_in_port : data->midi1_in_port;
                return data->plugin_buffer->buffers[portIndex];
            }
        return nullptr;
    }

    void AAPMidiProcessor::processMidiInput(uint8_t* bytes, size_t offset, size_t length, uint64_t timestampInNanoseconds) {
        // FIXME: we need complete revamp to support MIDI buffering between process() calls, and
        //  support MIDI 2.0 protocols.
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
