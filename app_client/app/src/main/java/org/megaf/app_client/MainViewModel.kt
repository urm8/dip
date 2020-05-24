package org.megaf.app_client

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.util.Log
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.ticker
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import java.io.IOException
import java.util.*

internal enum class BTCommands {
    GET_TEMP,  //0
    GET_HUMIDITY,  //1
    GET_WATER_LEVEL,  // 2
    GET_MOISTURE_LEVEL,  // 3
    GET_TEMP_THRESHOLD,  // 4
    GET_TARGET_TEMP,  //5
    GET_TARGET_HUMIDITY,  //6
    GET_TARGET_WATER_LVL,  //7
    GET_TARGET_MOISTURE_LEVEL,  //8
    SET_TEMP,  //9
    SET_HUMIDITY,  //10
    SET_WATER_LEVEL,  //11
    SET_MOISTURE_LEVEL,  //12
    SET_TEMP_THRESHOLD //13
}

class MainViewModel : ViewModel() {
    // serial uuid ???
    val uuid: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    val notice: MutableLiveData<String> = MutableLiveData()

    val contentLoading: MutableLiveData<Boolean> = MutableLiveData()

    val currentState: MutableLiveData<StatePoko> = MutableLiveData()

    val targetState: MutableLiveData<StatePoko> = MutableLiveData()

    val progress: MutableLiveData<Int> = MutableLiveData()

    val device: MutableLiveData<BluetoothDevice> = MutableLiveData()

    var periodicTask: Job? = null

    private var _bt_device: BluetoothDevice? = null

    private var _socket: BluetoothSocket? = null

    var bt_device: BluetoothDevice?
        get() = _bt_device
        set(value) {
            if (value == null) {
                return
            }
            device.postValue(value)
            _bt_device = value
            _open_socket(reopen = true)
            periodicTask?.cancel()
            periodicTask = viewModelScope.launch(Dispatchers.IO) {
                for (unit in ticker(3000, 100)) {
                    Log.d("vm", "tick")
                    runBlocking { getStats() }
                }
            }
            periodicTask!!.start()
        }

    private fun _open_socket(reopen: Boolean = false) {
        if (device.value == null || _socket != null && !reopen) {
            return
        }
        viewModelScope.launch(Dispatchers.IO) {
            _socket = _bt_device!!.createRfcommSocketToServiceRecord(uuid)
            try {
                _socket?.connect()
            } catch (e: IOException) {
                notice.postValue("failed to open socket")
            }
        }
    }

    private fun getStats() {
        contentLoading.postValue(true)
        try {
            runBlocking {
                _open_socket()
                val state = getCurrentState()
                currentState.postValue(state)
            }
        } catch (e: IOException) {
            notice.postValue("Failed to get current stats")
            _open_socket(true)
        }
        contentLoading.postValue(false)
    }

    public fun getTargetStats() {
        contentLoading.postValue(true)
        viewModelScope.launch(Dispatchers.IO) {
            try {
                _open_socket()
                targetState.postValue(getTargetState())
            } catch (e: IOException) {
                notice.postValue("Failed to get target stats")
                _open_socket()
            }
        }
        contentLoading.postValue(false)
    }

    private fun getCurrentState() = StatePoko(
        runCmd(BTCommands.GET_TEMP).toDouble(),
        runCmd(BTCommands.GET_HUMIDITY).toInt(),
        runCmd(BTCommands.GET_WATER_LEVEL).toInt(),
        runCmd(BTCommands.GET_MOISTURE_LEVEL).toInt()
    )

    private fun getTargetState() = StatePoko(
        runCmd(BTCommands.GET_TARGET_TEMP).toDouble(),
        runCmd(BTCommands.GET_TARGET_HUMIDITY).toInt(),
        runCmd(BTCommands.GET_TARGET_WATER_LVL).toInt(),
        runCmd(BTCommands.GET_TARGET_MOISTURE_LEVEL).toInt()
    )

    private fun runCmd(btCommand: BTCommands, value: String? = null): String {
        Log.i("vm", "sending command $btCommand")
        if (value == null) {
            _socket!!.outputStream.write("C+${btCommand.ordinal}".toByteArray(Charsets.US_ASCII))
        } else {
            _socket!!.outputStream.write("C+${btCommand.ordinal}=$value".toByteArray(Charsets.US_ASCII))
        }
        _socket!!.outputStream.flush()
        var response: String = "0"
        val buf = StringBuilder()
        var attempt = 3
        while (_socket!!.inputStream.available() > 0 || attempt-- > 0) {
            Log.d("vm", "trying to read from socket")
            Thread.sleep(500)
            while (_socket!!.inputStream.available() > 0) {
                buf.append(_socket!!.inputStream.read().toChar())
            }
            if (buf.isNotEmpty()) {
                Log.i("vm", "msg from socket: $buf")
                response = buf.toString()
                break
            }
        }
        return response
    }

    override fun onCleared() {
        super.onCleared()
        this._socket?.close()
    }
}
