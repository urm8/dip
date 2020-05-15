package org.megaf.app_client

import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.*
import android.os.Bundle
import android.view.View
import android.view.View.INVISIBLE
import android.view.View.VISIBLE
import android.widget.Button
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import kotlinx.android.synthetic.main.activity_main.*
import java.util.*

class MainActivity : AppCompatActivity() {
    val validNames = setOf("farm", "hc-06")

    lateinit var mainModel: MainViewModel

    lateinit var bluetoothAdapter: BluetoothAdapter
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        mainModel = ViewModelProvider(this).get(MainViewModel::class.java)
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
                mainModel.device.value = device
                break
            }
        }
        this.bluetoothAdapter.startDiscovery()

        findViewById<Button>(R.id.sync).setOnClickListener(View.OnClickListener {
            mainModel.getStats()
        })
        setupBindings()
    }

    private fun setupBindings() {
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
            this.humid.text = "${it.humidity} %"
            this.moisture.text = "${MoistureLevel.values()[it.moistureLevel]}"
            this.tankLevel.progress = it.waterLevel
        })
        mainModel.device.observe(this, androidx.lifecycle.Observer {
            Toast.makeText(this@MainActivity, "found farm (${it.name})", Toast.LENGTH_LONG).show()
            it.setPin(byteArrayOf(1, 2, 3, 4))
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
                        mainModel.device.value = device
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
