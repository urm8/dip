package org.megaf.app_client

import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.*
import android.graphics.Color
import android.os.Bundle
import android.view.View.INVISIBLE
import android.view.View.VISIBLE
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import kotlinx.android.synthetic.main.activity_main.*
import java.util.*
import kotlin.math.min

class MainActivity : AppCompatActivity() {
    val validNames = setOf("farm", "hc-06")

    lateinit var mainModel: MainViewModel

    lateinit var bluetoothAdapter: BluetoothAdapter
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        mainModel = ViewModelProvider(this).get(MainViewModel::class.java)
        setupBindings()
        findDevices()
    }

    private fun findDevices() {
        val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null) {
            AlertDialog.Builder(this).setMessage("devices without bluetooth module not available")
                .setOnDismissListener(object : DialogInterface.OnDismissListener {
                    override fun onDismiss(dialog: DialogInterface?) {
                        finish()
                    }

                })
                .create().show()
            return
        }
        this.bluetoothAdapter = bluetoothAdapter
        if (!bluetoothAdapter.isEnabled) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            startActivityForResult(enableBtIntent, 0)
        }
        val filter = IntentFilter(BluetoothDevice.ACTION_FOUND)
        registerReceiver(receiver, filter)
        for (device in bluetoothAdapter.bondedDevices) {
            if (device.name.toLowerCase(Locale.getDefault()) in validNames) {
                mainModel.bt_device = device
                break
            }
        }
        this.bluetoothAdapter.startDiscovery()
    }

    private fun setupBindings() {
        targetTempSeekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                targetTemp.text = "$progress C"
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {

            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
            }
        })

        mainModel.notice.observe(this, androidx.lifecycle.Observer {
            Toast.makeText(this, it, Toast.LENGTH_LONG).show()
        })
        mainModel.contentLoading.observe(this, androidx.lifecycle.Observer {
            this.progress.visibility = when (it) {
                true -> VISIBLE
                else -> INVISIBLE
            }
        })
        mainModel.currentState.observe(this, androidx.lifecycle.Observer {
            this.temp.text = "${it.temp} C"
            this.moisture.text =
                "${MoistureLevel.values()[min(it.moistureLevel, MoistureLevel.values().size - 1)]}"
            this.tankLevel.text = "${it.waterLevel}%"
        })
        mainModel.device.observe(this, androidx.lifecycle.Observer {
            if (it != null) {
                Toast.makeText(
                    this@MainActivity,
                    "connected to farm (${it.name})",
                    Toast.LENGTH_SHORT
                ).show()
                it.setPin(byteArrayOf(1, 2, 3, 4))
                this.isConnected.text = "connected to ${it.name}"
                this.isConnected.setTextColor(Color.GREEN)
            } else {
                Toast.makeText(this@MainActivity, "disconnected from farm", Toast.LENGTH_SHORT)
                    .show()
                this.isConnected.text = "disconnected"
                this.isConnected.setTextColor(Color.RED)
            }
        })
        mainModel.targetState.observe(this, androidx.lifecycle.Observer {
            this.targetTemp.text = "%d C".format(it.temp)
            this.targetMoistureLevel.selectedItem
        })
    }

    // Create a BroadcastReceiver for ACTION_FOUND.
    private val receiver = object : BroadcastReceiver() {

        override fun onReceive(context: Context, intent: Intent) {

            val action: String = intent.action!!
            when (action) {
                BluetoothDevice.ACTION_FOUND -> {
                    // Discovery has found a device. Get the BluetoothDevice
                    // object and its info from the Intent.
                    val device: BluetoothDevice =
                        intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)!!
                    if (device.name in validNames) {
                        mainModel.bt_device = device
                    }
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
    }

}
