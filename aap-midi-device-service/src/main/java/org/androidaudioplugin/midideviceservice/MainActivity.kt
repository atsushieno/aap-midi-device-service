package org.androidaudioplugin.midideviceservice

import android.app.Service
import android.media.midi.MidiDevice
import android.media.midi.MidiDeviceInfo
import android.media.midi.MidiDeviceInfo.PortInfo
import android.media.midi.MidiManager
import android.media.midi.MidiManager.OnDeviceOpenedListener
import android.os.Bundle
import android.os.Handler
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.tooling.preview.Preview
import kotlinx.coroutines.*
import org.androidaudioplugin.midideviceservice.ui.theme.AapmidideviceserviceTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AapmidideviceserviceTheme {
                // A surface container using the 'background' color from the theme
                Surface(color = MaterialTheme.colors.background) {
                    Greeting("Android")
                }
            }
        }
        val midiManager = getSystemService(Service.MIDI_SERVICE) as MidiManager
        val deviceInfo: MidiDeviceInfo = midiManager.devices.first {d ->
            d.properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) == "androidaudioplugin.org" &&
            d.properties.getString(MidiDeviceInfo.PROPERTY_NAME) == "AAPMidi"
        }
        midiManager.openDevice(deviceInfo, object:OnDeviceOpenedListener {
            override fun onDeviceOpened(device: MidiDevice?) {
                if (device == null) return
                // FIXME: this is failing to open a port: "portNumber out of range in openInputPort: 0"
                val input = device.openInputPort(0) ?: throw Exception("failed to open input port")
                GlobalScope.launch {
                    input.send(byteArrayOf(0x90.toByte(), 0x36, 100), 0, 3)
                    delay(1000)
                    input.send(byteArrayOf(0x80.toByte(), 0x36, 0), 0, 3)
                }
            }
        }, null)
    }
}

@Composable
fun Greeting(name: String) {
    Text(text = "Hello $name!")
}

@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    AapmidideviceserviceTheme {
        Greeting("Android")
    }
}