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

    void AAPMidiProcessor::initialize(int32_t sampleRate) {
        // AAP settings
        host = std::make_unique<aap::PluginHost>(&host_manager);
        sample_rate = sampleRate;

        // Oboe configuration
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(oboe::ChannelCount::Stereo);

        builder.setDataCallback(&callback);
    }

    void AAPMidiProcessor::terminate() {
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

        auto instanceId = host->createInstance(pluginId, sample_rate);
        instance_ids.emplace_back(instanceId);
    }

    void AAPMidiProcessor::activate() {
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

        state = AAP_MIDI_PROCESSOR_STATE_STARTED;
    }

    void AAPMidiProcessor::deactivate() {
        stream->stop();
        stream->close();
        stream.reset();

        state = AAP_MIDI_PROCESSOR_STATE_STOPPED;
    }

    void AAPMidiProcessor::processMessage(uint8_t* bytes, size_t offset, size_t length, uint64_t timestamp) {
        aap::aprintf("!!! TODO: IMPLEMENT processMessage() !!!: %d %d %d", bytes[0], bytes [1], length > 2 ? bytes[2] : 0);
    }
}
