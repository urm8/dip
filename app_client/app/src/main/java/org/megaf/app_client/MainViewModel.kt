package org.megaf.app_client

import android.bluetooth.BluetoothDevice
import android.util.Log
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.UnstableDefault
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonDecodingException
import java.io.IOException
import java.lang.StringBuilder
import java.util.*

class MainViewModel : ViewModel() {
    // serial uuid ???
    val uuid: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    val notice: MutableLiveData<String> = MutableLiveData()

    val contentLoading: MutableLiveData<Boolean> = MutableLiveData()

    val currentState: MutableLiveData<StatePoko> = MutableLiveData()

    val targetState: MutableLiveData<StatePoko> = MutableLiveData()

    val progress: MutableLiveData<Int> = MutableLiveData()

    val device: MutableLiveData<BluetoothDevice> = MutableLiveData()

    private val _bt_device: BluetoothDevice?
        get() = device.value


    enum class _actions {
        JSON,
        STATS,
    }

    @OptIn(UnstableDefault::class)
    fun getStats() {
        var response = StringBuilder()
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                contentLoading.postValue(true)
                val socket = _bt_device!!.createRfcommSocketToServiceRecord(uuid)
                try {
                    socket.use {
                        it.connect()
                        val bufWriter = socket.outputStream.bufferedWriter(Charsets.US_ASCII)
                        bufWriter.write("${_actions.STATS.ordinal}\n\r")
                        bufWriter.flush()
                        //
                        val bufReader = socket.inputStream.bufferedReader(Charsets.US_ASCII)
                        for (attempt in 1..3)  {
                            while (socket.inputStream.available() > 0) {
                                response.append(socket.inputStream.read().toChar())
                                delay(50)
                            }
                            delay(300)
                        }
                        val arr = CharArray(socket.inputStream.available())
                        bufReader.read(arr)
                        currentState.postValue(Json.parse(StatePoko.serializer(), response.toString()))
                        Log.i("vm", "dude this shit returned $response")
                    }
                } catch (e: IOException) {
                    notice.postValue("Failed to connect to farm")
                } catch (e: JsonDecodingException) {
                    notice.postValue("Failed to parse response from farm:\n $response")
                } finally {
                    contentLoading.postValue(false)
                }
            }
        }
    }
}