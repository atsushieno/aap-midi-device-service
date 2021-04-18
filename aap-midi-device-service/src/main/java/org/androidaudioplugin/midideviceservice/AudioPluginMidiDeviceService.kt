package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import android.media.midi.MidiDeviceService
import android.media.midi.MidiDeviceStatus
import android.media.midi.MidiReceiver
import android.os.IBinder
import org.androidaudioplugin.*

class AudioPluginMidiDeviceService : MidiDeviceService() {

    // The number of ports is not simply adjustable in one code. device_info.xml needs updates too.
    private lateinit var midiReceivers: Array<AudioPluginMidiReceiver>

    override fun onCreate() {
        midiReceivers = arrayOf(AudioPluginMidiReceiver(this))
    }

    override fun onGetInputPortReceivers(): Array<MidiReceiver> = midiReceivers.map { e -> e as MidiReceiver }.toTypedArray()

    override fun onDeviceStatusChanged(status: MidiDeviceStatus?) {
        if (status == null) return
        if (status.isInputPortOpen(0))
            midiReceivers[0].start()
        else if (!status.isInputPortOpen(0))
            midiReceivers[0].stop()
    }
}

class AudioPluginMidiReceiver(private val service: AudioPluginMidiDeviceService) : MidiReceiver(), AutoCloseable {
    companion object {
        init {
            System.loadLibrary("aapmidideviceservice")
        }
    }

    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val serviceConnector = AudioPluginServiceConnector(service.applicationContext)

    override fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) =
        processMessage(msg, offset, count, timestamp)

    override fun close() = terminateReceiverNative()

    fun start() = activate()

    fun stop() = deactivate()

    // FIXME: pass best sampleRate by device
    private val sampleRate: Int

    private fun connectService(packageName: String, className: String) {
        if (!serviceConnector.connectedServices.any { s -> s.serviceInfo.packageName == packageName && s.serviceInfo.className == className })
            serviceConnector.bindAudioPluginService(AudioPluginServiceInformation("", packageName, className), sampleRate)
    }

    private fun serviceHasInstrument(service: AudioPluginServiceInformation) =
        service.plugins.any { p -> isInstrument(p) }

    private fun isInstrument(info: PluginInformation) =
        // FIXME: use const
//        info.category.contains(PluginInformation.PRIMARY_CATEGORY_INSTRUMENT)
        info.category?.contains("Instrument") ?: info.category?.contains("Synth") ?: false

    private fun setupDefaultPlugins() {
        val allAAPs = AudioPluginHostHelper.queryAudioPluginServices(service.applicationContext)
        val service =
            allAAPs.firstOrNull { s -> s.packageName == service.packageName && serviceHasInstrument(s) }
            ?: allAAPs.firstOrNull { s -> serviceHasInstrument(s) }
        if (service != null)
            connectService(service.packageName, service.className)
    }

    private val pendingInstantiationList = mutableListOf<PluginInformation>()

    init {
        serviceConnector.serviceConnectedListeners.add { connection ->
            addPluginService(connection.binder!!, connection.serviceInfo.packageName, connection.serviceInfo.className)
            for (i in pendingInstantiationList)
                if (i.packageName == connection.serviceInfo.packageName && i.localName == connection.serviceInfo.className)
                    instantiatePlugin(i.pluginId!!)
        }
        initializeReceiverNative(service.applicationContext)

        val audioManager = service.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100

        setupDefaultPlugins()
    }

    private external fun initializeReceiverNative(applicationContext: Context)
    private external fun terminateReceiverNative()
    private external fun addPluginService(binder: IBinder, packageName: String, className: String)
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestamp: Long)
    private external fun activate()
    private external fun deactivate()
}
