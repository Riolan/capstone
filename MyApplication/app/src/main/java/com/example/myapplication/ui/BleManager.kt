package com.example.myapplication.ui
import android.annotation.SuppressLint
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.util.Log
import java.util.UUID
import android.os.Handler
import android.os.Looper

interface BleConnectionListener {
    fun onDeviceConnected(device: BluetoothDevice)
    fun onDeviceDisconnected(device: BluetoothDevice)
}

interface BleDataListener {
    fun onBleDataReceived(data: ByteArray)
}


class BleManager private constructor() {
    private var bluetoothGatt: BluetoothGatt? = null
    private var characteristic: BluetoothGattCharacteristic? = null
    private var isConnected = false
    private var currentDevice: BluetoothDevice? = null
    // UUIDs for BLE Service and Characteristic
    private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789012")
    private val CHARACTERISTIC_UUID = UUID.fromString("87654321-4321-4321-4321-210987654321")

    // Store all discovered devices
    private val discoveredDevices = mutableMapOf<String, BluetoothDevice>()

    // Store device-specific information
    private val deviceInfoMap = mutableMapOf<String, DeviceInfo>()
    private val connectionListeners = mutableListOf<BleConnectionListener>()
    private val dataListeners = mutableListOf<BleDataListener>()

    fun registerDataListener(listener: BleDataListener) {
        if (!dataListeners.contains(listener)) {
            dataListeners.add(listener)
        }
    }

    fun unregisterDataListener(listener: BleDataListener) {
        dataListeners.remove(listener)
    }

    fun notifyDataReceived(data: ByteArray) {
        dataListeners.forEach { it.onBleDataReceived(data) }
    }

    fun registerConnectionListener(listener: BleConnectionListener) {
        if (!connectionListeners.contains(listener)) {
            connectionListeners.add(listener)
        }
    }

    fun unregisterConnectionListener(listener: BleConnectionListener) {
        connectionListeners.remove(listener)
    }

    // Call these methods when connection state changes
    private fun notifyDeviceConnected(device: BluetoothDevice) {
        connectionListeners.forEach { it.onDeviceConnected(device) }
    }

    private fun notifyDeviceDisconnected(device: BluetoothDevice) {
        connectionListeners.forEach { it.onDeviceDisconnected(device) }
    }

    data class DeviceInfo(
        val deviceAddress: String,
        val deviceName: String?,
        var animalSettings: ByteArray = byteArrayOf(0, 0, 0, 0),
        var lastImageTimestamp: Long = 0
    ) {
        // Override equals and hashCode for proper ByteArray comparison
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false

            other as DeviceInfo

            if (deviceAddress != other.deviceAddress) return false
            if (deviceName != other.deviceName) return false
            if (!animalSettings.contentEquals(other.animalSettings)) return false

            return true
        }

        override fun hashCode(): Int {
            var result = deviceAddress.hashCode()
            result = 31 * result + (deviceName?.hashCode() ?: 0)
            result = 31 * result + animalSettings.contentHashCode()
            return result
        }
    }

    // Add a new device to our tracked devices
    @SuppressLint("MissingPermission")
    fun addDevice(device: BluetoothDevice) {
        val address = device.address
        discoveredDevices[address] = device

        if (!deviceInfoMap.containsKey(address)) {
            deviceInfoMap[address] = DeviceInfo(
                deviceAddress = address,
                deviceName = device.name ?: "Unknown Device"
            )
        }
        Log.d("BleManager", "Added device: ${device.name} (${device.address})")
    }

    // Get all discovered devices
    fun getDiscoveredDevices(): List<BluetoothDevice> {
        return discoveredDevices.values.toList()
    }

    // Get device info for all devices
    fun getAllDeviceInfo(): List<DeviceInfo> {
        return deviceInfoMap.values.toList()
    }

    // Get a specific device by address
    fun getDevice(address: String): BluetoothDevice? {
        return discoveredDevices[address]
    }

    // Get a specific device info by address
    fun getDeviceInfo(address: String): DeviceInfo? {
        return deviceInfoMap[address]
    }

    // Set current device and GATT when connecting
    fun setCurrentDevice(device: BluetoothDevice?) {
        currentDevice = device
        if (device != null) {
            notifyDeviceConnected(device)
        }

        if (device != null) {
            addDevice(device)
        } // Ensure the device is in our maps
    }

    // Get the current device
    fun getCurrentDevice(): BluetoothDevice? {
        return currentDevice
    }

    fun setGatt(gatt: BluetoothGatt?) {
        this.bluetoothGatt = gatt
    }

    fun getGatt(): BluetoothGatt? {
        return bluetoothGatt
    }

    fun setCharacteristic(characteristic: BluetoothGattCharacteristic?) {
        this.characteristic = characteristic
    }

    fun getCharacteristic(): BluetoothGattCharacteristic? {
        return characteristic
    }

    fun setConnected(connected: Boolean) {
        if (!connected) {
            currentDevice?.let { notifyDeviceDisconnected(it) }
        }
        isConnected = connected
    }

    fun isConnected(): Boolean {
        return isConnected
    }

    // Method for sending data over BLE
    @SuppressLint("MissingPermission")
    fun sendData(data: ByteArray): Boolean {
        val gatt = bluetoothGatt ?: run {
            Log.e("BLE", "Not connected to a device.")
            return false
        }

        val service = gatt.getService(SERVICE_UUID) ?: run {
            Log.e("BLE", "Service not found: $SERVICE_UUID")
            return false
        }

        val char = service.getCharacteristic(CHARACTERISTIC_UUID) ?: run {
            Log.e("BLE", "Characteristic not found: $CHARACTERISTIC_UUID")
            return false
        }

        if (char.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0) {
            Log.e("BLE", "Characteristic is not writable.")
            return false
        }

        char.value = data
        val success = gatt.writeCharacteristic(char)
        if (success) {
            Log.d("BLE", "Data sent successfully.")
        } else {
            Log.e("BLE", "Failed to send data.")
        }
        return success
    }

    @SuppressLint("MissingPermission")
    fun sendDataChunks(data: ByteArray): Boolean {
        val gatt = bluetoothGatt ?: run {
            Log.e("BLE", "Not connected to a device.")
            return false
        }

        val service = gatt.getService(SERVICE_UUID) ?: run {
            Log.e("BLE", "Service not found: $SERVICE_UUID")
            return false
        }

        val char = service.getCharacteristic(CHARACTERISTIC_UUID) ?: run {
            Log.e("BLE", "Characteristic not found: $CHARACTERISTIC_UUID")
            return false
        }

        if (char.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0) {
            Log.e("BLE", "Characteristic is not writable.")
            return false
        }

        val chunkSize = 20
        var offset = 0

        while (offset < data.size) {
            val end = (offset + chunkSize).coerceAtMost(data.size)
            val chunk = data.copyOfRange(offset, end)

            char.value = chunk
            val success = gatt.writeCharacteristic(char)

            if (!success) {
                Log.e("BLE", "Failed to send chunk at offset $offset.")
                return false
            }

            Log.d("BLE", "Chunk sent: ${chunk.joinToString(" ") { it.toString(16).padStart(2, '0') }}")

            offset += chunkSize

            // Small delay
            Thread.sleep(100)
        }

        Log.d("BLE", "All data chunks sent successfully.")
        return true
    }




    public val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(
            gatt: BluetoothGatt,
            status: Int,
            newState: Int
        ) {
            val deviceAddress = gatt.device.address

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i("BLE", "Connected to GATT server.")
                setConnected(true)
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                /*Log.i("BLE", "Disconnected from GATT server.")

                // Even if we get a disconnect callback, explicitly close the GATT
                try {
                    gatt.close()
                } catch (e: Exception) {
                    Log.e("BLE", "Error closing GATT: ${e.message}")
                }

                // Update internal state
                setConnected(false)
                if (getCurrentDevice()?.address == deviceAddress) {
                    getCurrentDevice()?.let { notifyDeviceDisconnected(it) }
                    setCurrentDevice(null)
                }

                // Clear references
                if (getGatt() == gatt) {
                    setGatt(null)
                    setCharacteristic(null)
                }*/
            }
        }



        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService((SERVICE_UUID))
                if (service != null) {
                    characteristic =
                        service.getCharacteristic((CHARACTERISTIC_UUID))
                    if (characteristic != null) {
                        gatt.setCharacteristicNotification(characteristic, true)
                        val descriptor = characteristic!!.getDescriptor(
                            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
                        )
                        descriptor?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                        Log.i("BLE", "Notifications enabled for ${characteristic!!.uuid}")
                    } else {
                        Log.e("BLE", "Characteristic not found.")
                    }
                } else {
                    Log.e("BLE", "Service not found.")
                }
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = characteristic.value
            Log.d("BleManager", "Data received: ${value.joinToString(" ") { "%02X".format(it) }}")
            notifyDataReceived(value)
        }
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        val device = getCurrentDevice()
        Log.d("BLE", "BleManager disconnect() called")

        try {
            if (bluetoothGatt != null) {
                // Disable notifications first
                characteristic?.let { char ->
                    try {
                        bluetoothGatt?.setCharacteristicNotification(char, false)
                        Log.d("BLE", "Disabled notifications for characteristic")
                    } catch (e: Exception) {
                        Log.e("BLE", "Failed to disable notifications", e)
                    }
                }

                // Disconnect from GATT
                Log.d("BLE", "Disconnecting from GATT server...")
                bluetoothGatt?.disconnect()

                // Schedule cleanup
                Handler(Looper.getMainLooper()).postDelayed({
                    try {
                        bluetoothGatt?.close()
                        Log.d("BLE", "GATT connection closed")
                    } catch (e: Exception) {
                        Log.e("BLE", "Error closing GATT", e)
                    } finally {
                        // Always reset state regardless of success
                        bluetoothGatt = null
                        characteristic = null
                        setConnected(false)
                        device?.let { notifyDeviceDisconnected(it) }
                        currentDevice = null
                        Log.i("BLE", "Disconnected and closed GATT connection")
                    }
                }, 500)
            } else {
                Log.w("BLE", "No GATT connection to disconnect")
                setConnected(false)
                device?.let { notifyDeviceDisconnected(it) }
                currentDevice = null
            }
        } catch (e: Exception) {
            Log.e("BLE", "Error during disconnect: ${e.message}")
            // Force cleanup in case of exception
            bluetoothGatt = null
            characteristic = null
            setConnected(false)
            currentDevice = null
        }
    }



    companion object {
        @Volatile
        private var instance: BleManager? = null

        // Thread-safe singleton implementation using Double-Checked Locking
        fun getInstance(): BleManager {
            return instance ?: synchronized(this) {
                instance ?: BleManager().also { instance = it }
            }
        }
    }
}