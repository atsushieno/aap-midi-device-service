package org.androidaudioplugin.midideviceservice

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
        var midiManagerInitialized by remember { mutableStateOf(model.midiManagerInitialized) }

        Surface(color = MaterialTheme.colors.background) {
            Column {
                AvailablePlugins(onItemClick = { plugin -> model.instrument = plugin }, plugins)
                Row {
                    if (midiManagerInitialized)
                        Button(modifier = Modifier.padding(2.dp),
                            onClick = {
                                // FIXME: currently something is incorrectly reused and caused crash,
                                //  so I explicitly keep it non-reusable. This should be fixed.
                                //midiManagerInitialized = false
                                model.terminateMidi()
                            }) {
                            Text("Stop MIDI Service")
                        }
                    else
                        Button(modifier = Modifier.padding(2.dp),
                            onClick = {
                                midiManagerInitialized = true
                                model.initializeMidi()
                            }) {
                            Text("Start MIDI Service")
                        }
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
    var selectedIndex by remember { mutableStateOf(-1) }

    LazyColumn(state = state) {
        itemsIndexed(instrumentPlugnis, itemContent = { index, plugin ->
            Row(modifier = Modifier
                .padding(start = 16.dp, end = 16.dp)
            ) {
                Column(modifier = Modifier
                    .clickable {
                        selectedIndex = index
                        onItemClick(plugin)
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
