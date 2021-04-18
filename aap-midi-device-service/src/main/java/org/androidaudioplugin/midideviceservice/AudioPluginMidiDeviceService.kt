package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.media.midi.MidiDeviceService
import android.media.midi.MidiReceiver
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.AudioPluginInstance
import org.androidaudioplugin.PortInformation
import java.nio.ByteBuffer

class AudioPluginMidiDeviceService : MidiDeviceService() {

    //private lateinit var host : AudioPluginHost
    private var midiReceivers: MutableMap<String,MidiReceiver> = mutableMapOf()

    override fun onGetInputPortReceivers(): Array<MidiReceiver> = midiReceivers.values.toTypedArray()

    init {
        val aapService = AudioPluginHostHelper.queryAudioPluginServices(applicationContext)
            .firstOrNull { s -> s.packageName == this.packageName }
        if (aapService == null)
            midiReceivers = mutableMapOf()
        /*
        else {
            host = AudioPluginHost(applicationContext)
            host.pluginInstantiatedListeners.add { instance ->
                midiReceivers[instance.pluginInfo.pluginId!!] = AudioPluginMidiReceiver(this, host, instance)
            }
            host.bindAudioPluginService(aapService)
        }
        */
    }
}

class AudioPluginMidiReceiver(private val service: AudioPluginMidiDeviceService, host: AudioPluginHost, private val plugin: AudioPluginInstance) : MidiReceiver(), AutoCloseable {
    /*
    private lateinit var midiBuffer: ByteBuffer
    private val audioBuffers: Array<ByteBuffer>

    private val bufferSize = AudioTrack.getMinBufferSize(host.sampleRate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_FLOAT)

    private val track: AudioTrack = AudioTrack.Builder()
        .setAudioAttributes(
            AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build())
        .setAudioFormat(
            AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                .setSampleRate(host.sampleRate)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .build())
        .setBufferSizeInBytes(bufferSize)
        .setTransferMode(AudioTrack.MODE_STREAM)
        .build()
     */

    override fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) =
        processMessage(msg, offset, count, timestamp)

    override fun close() = terminateReceiverNative()

    private fun start() = activate()

    private fun stop() = deactivate()

    init {
        /*
        plugin.prepare(host.sampleRate, host.audioBufferSizeInBytes)
        val audioBufferList = mutableListOf<ByteBuffer>()
        for (i in 0 until plugin.pluginInfo.ports.size) {
            val portInfo = plugin.pluginInfo.ports[i]
            if (portInfo.direction == PortInformation.PORT_DIRECTION_INPUT &&
                portInfo.content == PortInformation.PORT_CONTENT_TYPE_MIDI)
                midiBuffer = plugin.getPortBuffer(i)
            else if (portInfo.direction == PortInformation.PORT_DIRECTION_OUTPUT &&
                portInfo.content == PortInformation.PORT_CONTENT_TYPE_AUDIO)
                audioBufferList.add(plugin.getPortBuffer(i))
        }
        audioBuffers = audioBufferList.toTypedArray()
        track.setVolume(5.0f)
        */

        initializeReceiverNative(service.applicationContext)
    }

    private external fun initializeReceiverNative(applicationContext: Context)
    private external fun terminateReceiverNative()
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestamp: Long)
    private external fun activate()
    private external fun deactivate()
}
