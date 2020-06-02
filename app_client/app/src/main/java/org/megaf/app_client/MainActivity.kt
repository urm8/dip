package org.megaf.app_client

import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.*
import android.graphics.Color
import android.os.Bundle
import android.view.View.INVISIBLE
import android.view.View.VISIBLE
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.ViewModelProvider
import kotlinx.android.synthetic.main.activity_main.*
import java.util.*
import java.util.concurrent.TimeUnit
import kotlin.math.max
import kotlin.math.min

class MainActivity : AppCompatActivity() {
    val validNames = setOf("farm", "hc-06")

    lateinit var mainModel: MainViewModel

    lateinit var bluetoothAdapter: BluetoothAdapter
    lateinit var moistureLvls: Array<String>
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
                .setOnDismissListener { finish() }
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
        this.moistureLvls = resources.getStringArray(R.array.moisture_levels)
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
            temp.text = "${it.temp} C"
            moisture.text =
                "${MoistureLevel.values()[min(it.moistureLevel, MoistureLevel.values().size - 1)]}"
            tankLevel.text = "${it.waterLevel}"
            secondsSinceLastMoisture.text = fmtSeconds(it.secondsSinceLastMoisture)
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
            targetTemp.text = "${it.temp}"
            targetMoistureLevel.text = moistureLvls[min(max(it.moistureLevel, 0), moistureLvls.size - 1)]
        })
        editTarget.setOnClickListener {
            val layout = LinearLayout(this)
            layout.orientation = LinearLayout.VERTICAL
            val seekBar = SeekBar(this)
            seekBar.min = 14
            seekBar.max = 45
            layout.addView(seekBar)
            val tempText = TextView(this)
            seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(
                    seekBar: SeekBar?,
                    progress: Int,
                    fromUser: Boolean
                ) {
                    tempText.text = "Target temp: $progress "
                }

                override fun onStartTrackingTouch(seekBar: SeekBar?) {
                }

                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                }
            })
            try {
                seekBar.progress = mainModel.targetState.value!!.temp.toInt()
            } catch (e: Exception) {
                seekBar.progress = seekBar.min
            }
            layout.addView(tempText)
            val targetMoisture = TextView(this)
            targetMoisture.text = "Target moisture:"
            val spinner = Spinner(this)
            spinner.adapter = ArrayAdapter<String>(
                this,
                android.R.layout.simple_spinner_item,
                resources.getStringArray(R.array.moisture_levels)
            )
            layout.addView(spinner)
            AlertDialog.Builder(this)
                .setTitle(R.string.edit_dialog_title)
                .setView(layout)
                .setPositiveButton(
                    "Save",
                    { dial: DialogInterface, which: Int ->
                        mainModel.setTargetState(
                            seekBar.progress,
                            spinner.selectedItemPosition - 1
                        )
                    })
                .show()
        }
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
                    unregisterReceiver(this)
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(receiver)
    }

    private fun fmtSeconds(uptime: Long): String {
        val days = TimeUnit.SECONDS.toDays(uptime)
        val hours: Long = TimeUnit.SECONDS.toHours(uptime) - TimeUnit.DAYS.toHours(days)
        val minutes: Long = TimeUnit.SECONDS.toMinutes(uptime) - TimeUnit.HOURS.toMinutes(hours)
        val seconds: Long = uptime % 60
        val out = StringBuilder()
        if (days > 0) {
            out.append("$days h")
        }
        out.append(" $hours h")
        out.append(" $minutes m")
        out.append(" $seconds s")
        return out.toString()
    }

}
