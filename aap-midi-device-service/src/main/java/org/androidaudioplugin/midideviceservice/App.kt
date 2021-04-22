package org.androidaudioplugin.midideviceservice

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.midideviceservice.ui.theme.AAPMidiDeviceServiceTheme

@Composable
fun App() {
    val plugins: List<PluginInformation> = model.pluginServices.flatMap { s -> s.plugins }.toList()
        .filter { p -> p.category?.contains("Instrument") ?: false || p.category?.contains("Synth") ?: false }

    AAPMidiDeviceServiceTheme {
        // FIXME: maybe we should remove this hacky state variable
        var midiManagerInitializedState by remember { mutableStateOf(model.midiManagerInitialized) }

        Surface(color = MaterialTheme.colors.background) {
            Column {
                AvailablePlugins(onItemClick = { plugin -> model.specifiedInstrument = plugin }, plugins)
                Row {
                    if (midiManagerInitializedState)
                        Button(modifier = Modifier.padding(2.dp),
                            onClick = {
                                model.terminateMidi()
                                midiManagerInitializedState = false
                            }) {
                            Text("Stop MIDI Service")
                        }
                    else
                        Button(modifier = Modifier.padding(2.dp),
                            onClick = {
                                midiManagerInitializedState = true
                                model.initializeMidi()
                            }) {
                            Text("Start MIDI Service")
                        }
                }
                Row {
                    Button(modifier = Modifier.padding(2.dp),
                        onClick = { model.playNote() }) {
                        Text("Play")
                    }
                    
                }
            }
        }
    }
}

@Composable
fun AvailablePlugins(onItemClick: (PluginInformation) -> Unit = {}, instrumentPlugnis: List<PluginInformation>) {
    val small = TextStyle(fontSize = 12.sp)

    val state by remember { mutableStateOf(LazyListState()) }
    var selectedIndex by remember { mutableStateOf(if (model.instrument != null) instrumentPlugnis.indexOf(model.instrument) else -1) }

    LazyColumn(state = state) {
        itemsIndexed(instrumentPlugnis, itemContent = { index, plugin ->
            Row(modifier = Modifier
                .padding(start = 16.dp, end = 16.dp)
            ) {
                Column(modifier = Modifier
                    .clickable {
                        onItemClick(plugin)
                        selectedIndex = index

                        // save instrument as the last used one, so that it can be the default.
                        val sp = applicationContextForModel.getSharedPreferences(SHARED_PREFERENCE_KEY, Context.MODE_PRIVATE)
                        sp.edit().putString(PREFERENCE_KRY_PLUGIN_ID, model.instrument.pluginId).apply()
                    }
                    .border(if (index == selectedIndex) 2.dp else 0.dp, MaterialTheme.colors.primary)
                    .weight(1f)) {
                    Text(plugin.displayName)
                    Text(plugin.packageName, style = small)
                }
            }
        })
    }
}
