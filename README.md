This is an experimental project that implements Android MidiDeviceService API with [Android Audio Plugin](https://github.com/atsushieno/android-audio-plugin-framework) backend.

So far it is nothing more than an experiment and can only handle one message per plugin process cycle. Super low quality hacks with lots of pitfalls and crashes.

It supports MIDI 2.0 UMP messages if there is some implicit agreement with client app and if it sends UMPs. [aap-lv2](https://github.com/atsushieno/aap-lv2) supports such UMPs and converts them into MIDI 1.0 messages, then passed to LV2 instruments as Atom sequences.

To have LV2 plugins that actually support MIDI 2.0, they will have to support some extension for MIDI 2.0. My [lv2-midi2](https://github.com/atsushieno/lv2-midi2) needs some work to make it possible.

## Licensing notice

aap-midi-device-service is distributed under the MIT license.

There are some sources copied from [jalv](https://gitlab.com/drobilla/jalv) project included in aap-midi-device-service module, namely those files under `zix` directory, and they are distributed under the ISC license.

