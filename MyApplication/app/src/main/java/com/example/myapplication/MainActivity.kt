package com.example.myapplication

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.util.UUID

class MainActivity : AppCompatActivity() {
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var bluetoothLeScanner: BluetoothLeScanner
    private val devices: MutableList<BluetoothDevice> = mutableListOf()
    private lateinit var adapter: ArrayAdapter<String>
    private lateinit var bluetoothGatt: BluetoothGatt
    private lateinit var characteristic: BluetoothGattCharacteristic

    // Replace with your service and characteristic UUIDs
    private val SERVICE_UUID = "12345678-1234-1234-1234-123456789012"
    private val CHARACTERISTIC_UUID = "87654321-4321-4321-4321-210987654321"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Initialize Bluetooth adapter
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        // Check for permissions and request if needed
        if (ActivityCompat.checkSelfPermission(this, android.Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(android.Manifest.permission.ACCESS_FINE_LOCATION), 1)
        }

        bluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
        val listView: ListView = findViewById(R.id.listView)

        // Set up the adapter for the ListView
        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        listView.adapter = adapter

        // Handle item clicks
        listView.onItemClickListener = AdapterView.OnItemClickListener { _, _, position, _ ->
            val device = devices[position]
            connectToDevice(device)
        }

        // Set up the disconnect button
        val disconnectButton: Button = findViewById(R.id.disconnectButton)
        disconnectButton.setOnClickListener {
            disconnectFromDevice()
        }

        // Set up the send button
        val sendButton: Button = findViewById(R.id.sendButton)
        val messageEditText: EditText = findViewById(R.id.messageBox) // Assume you have this EditText for input
        sendButton.setOnClickListener {
            val message = messageEditText.text.toString()
            sendMessage(message)
        }

        // Start scanning for devices
        startScan()
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bluetoothLeScanner.startScan(null, settings, scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            if (!devices.contains(device)) {
                devices.add(device)
                adapter.add("${device.name ?: "Unknown Device"}\n${device.address}")
                adapter.notifyDataSetChanged()
            }
        }

        override fun onBatchScanResults(results: List<ScanResult>) {
            @SuppressLint("MissingPermission")
            for (result in results) {
                val device = result.device
                if (!devices.contains(device)) {
                    devices.add(device)
                    adapter.add("${device.name ?: "Unknown Device"}\n${device.address}")
                    adapter.notifyDataSetChanged()
                }
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Toast.makeText(this@MainActivity, "Scan failed: $errorCode", Toast.LENGTH_SHORT).show()
        }
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        bluetoothGatt = device.connectGatt(this, false, gattCallback)
        Toast.makeText(this, "Connecting to ${device.name}", Toast.LENGTH_SHORT).show()
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothGatt.STATE_CONNECTED -> {
                    runOnUiThread {
                        Toast.makeText(this@MainActivity, "Connected to ${gatt.device.name}", Toast.LENGTH_SHORT).show()
                    }
                    gatt.discoverServices() // Discover services after connection
                    Log.i("BLE", "Connected to device: ${gatt.device.address}")
                }
                BluetoothGatt.STATE_DISCONNECTED -> {
                    runOnUiThread {
                        Toast.makeText(this@MainActivity, "Disconnected from ${gatt.device.name}", Toast.LENGTH_SHORT).show()
                    }
                    gatt.close()
                    Log.i("BLE", "Disconnected from device: ${gatt.device.address}")
                }
                else -> {
                    Log.i("BLE", "Connection state changed: $newState")
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                runOnUiThread {
                    Toast.makeText(this@MainActivity, "Services Discovered", Toast.LENGTH_SHORT).show()
                }
                Log.i("BLE", "Services discovered successfully.")

                // Find the service and characteristic
                val service = gatt.getService(UUID.fromString(SERVICE_UUID))
                if (service != null) {
                    characteristic = service.getCharacteristic(UUID.fromString(CHARACTERISTIC_UUID))
                    if (characteristic != null) {
                        Log.i("BLE", "Characteristic found: ${characteristic.uuid}")
                    } else {
                        Log.e("BLE", "Characteristic not found.")
                    }
                } else {
                    Log.e("BLE", "Service not found.")
                }
            } else {
                Log.w("BLE", "onServicesDiscovered received: $status")
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun sendMessage(message: String) {
        if (::characteristic.isInitialized) {
            characteristic.value = message.toByteArray() // Convert message to bytes
            val status = bluetoothGatt.writeCharacteristic(characteristic)
            if (status) {
                Log.d("BLE", "Message sent successfully: $message")
            } else {
                Log.e("BLE", "Failed to send message.")
            }
        } else {
            Log.e("BLE", "Characteristic is not initialized.")
        }
    }

    @SuppressLint("MissingPermission")
    private fun disconnectFromDevice() {
        if (::bluetoothGatt.isInitialized) {
            bluetoothGatt.disconnect()
            bluetoothGatt.close()
            Log.i("BLE", "Disconnected from device.")
            Toast.makeText(this, "Disconnected from device.", Toast.LENGTH_SHORT).show()
        } else {
            Log.w("BLE", "BluetoothGatt is not initialized.")
            Toast.makeText(this, "No device to disconnect from.", Toast.LENGTH_SHORT).show()
        }
    }
}
