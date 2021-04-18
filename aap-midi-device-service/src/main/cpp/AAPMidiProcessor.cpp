#include <aap/logging.h>
#include "AAPMidiProcessor.h"

oboe::DataCallbackResult AAPOboeAudioCallback::onAudioReady(
        oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) {
    auto *outputData = static_cast<float *>(audioData);

    const float amplitude = 0.2f;
    for (int i = 0; i < numFrames; ++i){
        outputData[i] = ((float)drand48() - 0.5f) * 2 * amplitude;
    }

    return oboe::DataCallbackResult::Continue;
}

void AAPMidiProcessor::initialize() {
    builder.setDirection(oboe::Direction::Output);
    builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    builder.setSharingMode(oboe::SharingMode::Exclusive);
    builder.setFormat(oboe::AudioFormat::Float);
    builder.setChannelCount(oboe::ChannelCount::Stereo);

    builder.setDataCallback(&callback);
}

std::string AAPMidiProcessor::convertStateToText(AAPMidiProcessorState stateValue) {
    switch (stateValue) {
    case AAP_MIDI_PROCESSOR_STATE_CREATED: return "CREATED";
    case AAP_MIDI_PROCESSOR_STATE_STARTED: return "STARTED";
    case AAP_MIDI_PROCESSOR_STATE_STOPPED: return "STOPPED";
    case AAP_MIDI_PROCESSOR_STATE_ERROR: return "ERROR";
    }
    return "(UNKNOWN)";
}

void AAPMidiProcessor::start() {
    if (state != AAP_MIDI_PROCESSOR_STATE_CREATED) {
        aap::aprintf("Unexpected call to start() at %s state.", convertStateToText(state).c_str());
        state = AAP_MIDI_PROCESSOR_STATE_ERROR;
        return;
    }

    oboe::Result result = builder.openStream(stream);
    if (result != oboe::Result::OK) {
        aap::aprintf("Failed to create Oboe stream: %s", oboe::convertToText(result));
        state = AAP_MIDI_PROCESSOR_STATE_ERROR;
        return;
    }

    state = AAP_MIDI_PROCESSOR_STATE_STARTED;
}

