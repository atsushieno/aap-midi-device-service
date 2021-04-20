package org.androidaudioplugin.midideviceservice

import android.app.Service
import android.content.Context
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiInputPort
import android.media.midi.MidiManager
import android.media.midi.MidiManager.OnDeviceOpenedListener
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation


lateinit var model: ApplicationModel

class ApplicationModel(context: Context) {
    val midiManager = context.getSystemService(Service.MIDI_SERVICE) as MidiManager
    lateinit var midiInput: MidiInputPort
    val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(context.applicationContext)

    var midiManagerInitialized = false
    var instrument: PluginInformation? = null

    // it is not implemented to be restartable yet
    fun terminateMidi() {
        if (!midiManagerInitialized)
            return
        midiInput.close()
        midiManagerInitialized = false
    }

    fun initializeMidi() {
        if (midiManagerInitialized)
            return

        val deviceInfo: MidiDeviceInfo = midiManager.devices.first { d ->
            d.properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) == "androidaudioplugin.org" &&
                    d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == "AAPMidi"
        }
        midiManager.openDevice(deviceInfo, object: OnDeviceOpenedListener {
            override fun onDeviceOpened(device: MidiDevice?) {
                if (device == null)
                    return
                midiInput = device.openInputPort(0) ?: throw Exception("failed to open input port")
                midiManagerInitialized = true
            }
        }, null)
    }

    fun playNote() {
        if (!midiManagerInitialized)
            return

        GlobalScope.launch {
            midiInput.send(byteArrayOf(0x90.toByte(), 60, 100), 0, 3)
            delay(1000)
            midiInput.send(byteArrayOf(0x80.toByte(), 60, 0), 0, 3)
        }
    }
}
