// Main Activity

package com.example.myapplication

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Bundle
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.recyclerview.widget.RecyclerView
import androidx.viewpager2.widget.ViewPager2
import com.example.myapplication.ui.BleConnectionListener
import com.example.myapplication.ui.BleDataListener
import java.io.ByteArrayOutputStream
import java.text.SimpleDateFormat
import java.util.*

import com.example.myapplication.ui.BleManager

data class BoundingBox(
    val x: Int,
    val y: Int,
    val width: Int,
    val height: Int,
    val score: Int,
    val target: Int
)

class ImageAdapter(
    private val images: List<Bitmap>,
    private val boundingBoxes: List<List<BoundingBox>> // bounding boxes for each image
) : RecyclerView.Adapter<ImageAdapter.ImageViewHolder>() {

    class ImageViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val imageView: ImageView = itemView.findViewById(R.id.imageView)
        val boundingBoxOverlay: ImageView = itemView.findViewById(R.id.boundingBoxOverlay)
        val scoreTextView: TextView = itemView.findViewById(R.id.scoreTextView)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ImageViewHolder {
        val view =
            LayoutInflater.from(parent.context).inflate(R.layout.item_image, parent, false)
        return ImageViewHolder(view)
    }

    override fun onBindViewHolder(holder: ImageViewHolder, position: Int) {
        val bitmap = images[position]
        holder.imageView.setImageBitmap(bitmap)

        // Get bounding boxes for this image
        val bboxList = boundingBoxes.getOrNull(position) ?: emptyList()
        holder.boundingBoxOverlay.setImageBitmap(drawBoundingBoxes(bitmap, bboxList))

        if (bboxList.isNotEmpty()) {
            val firstBox = bboxList.first()
            holder.scoreTextView.text = "Score: ${firstBox.score}  Target: ${firstBox.target}"
        } else {
            holder.scoreTextView.text = "Score: N/A  Target: N/A"
        }
    }

    override fun getItemCount(): Int = images.size

    private fun drawBoundingBoxes(bitmap: Bitmap, boxes: List<BoundingBox>): Bitmap {
        val mutableBitmap = bitmap.copy(Bitmap.Config.ARGB_8888, true)
        val canvas = Canvas(mutableBitmap)
        val paint = Paint().apply {
            color = Color.RED
            style = Paint.Style.STROKE
            strokeWidth = 5f
        }

        val bitmapWidth = bitmap.width.toFloat()
        val bitmapHeight = bitmap.height.toFloat()

        for (box in boxes) {
            val centerX = box.x.toFloat()
            val centerY = box.y.toFloat()
            val width = box.width.toFloat()
            val height = box.height.toFloat()

            val left = centerX - (width / 2)
            val top = centerY - (height / 2)
            val right = centerX + (width / 2)
            val bottom = centerY + (height / 2)

            val adjustedLeft = left.coerceIn(0f, bitmapWidth)
            val adjustedTop = top.coerceIn(0f, bitmapHeight)
            val adjustedRight = right.coerceIn(0f, bitmapWidth)
            val adjustedBottom = bottom.coerceIn(0f, bitmapHeight)

            canvas.drawRect(adjustedLeft, adjustedTop, adjustedRight, adjustedBottom, paint)
        }
        return mutableBitmap
    }
}

class MainActivity : AppCompatActivity(), BleDataListener, BleConnectionListener {

    // Bluetooth-related variables
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var bluetoothLeScanner: BluetoothLeScanner
    private val devices: MutableList<BluetoothDevice> = mutableListOf()
    private lateinit var adapter: ArrayAdapter<String>

    // Data for image and bounding box transmission
    private val imageBitmaps = mutableListOf<Bitmap>()
    private val boundingBoxes = mutableListOf<List<BoundingBox>>()
    private var receivedImageBuilder = StringBuilder()
    private var receivedBboxBuilder = ByteArrayOutputStream()
    private var processingImage = false
    private var processingBoundingBox = false
    private var currentImagePosition = -1

    // UI Elements
    private lateinit var isConnectedText :TextView
    private lateinit var isConnectedButton: Button

    private lateinit var sendBleData : EditText
    private lateinit var sendBleButton : Button

    private lateinit var viewPager: ViewPager2
    private lateinit var img_adapter: ImageAdapter
    private lateinit var dataTextView: TextView

    private lateinit var cameraButton: Button
    private lateinit var sharedPreferences: SharedPreferences

    // Database helper
    private lateinit var dbHelper: DatabaseHelper

    /*// UUIDs for BLE Service and Characteristic
    private val SERVICE_UUID = "12345678-1234-1234-1234-123456789012"
    private val CHARACTERISTIC_UUID = "87654321-4321-4321-4321-210987654321"*/

    private val PERMISSION_REQUEST_CODE = 100

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        BleManager.getInstance().registerConnectionListener(this)

        // Initialize DatabaseHelper
        dbHelper = DatabaseHelper(this)

        // Shared Preferences and Login Check
        sharedPreferences = getSharedPreferences("AppPrefs", MODE_PRIVATE)
        val userLoggedIn = sharedPreferences.getBoolean("isLoggedIn", false)
        if (!userLoggedIn) {
            startActivity(Intent(this, LoginActivity::class.java))
            finish()
            return
        }

        // Logout Button Setup
        val logoutButton = findViewById<Button>(R.id.logoutButton)
        logoutButton.setOnClickListener { logoutUser() }

        isConnectedText = findViewById(R.id.connectedText)
        isConnectedButton = findViewById(R.id.disconnectButton)
        isConnectedButton.setEnabled(false) // default to set disconnected
        isConnectedButton.setClickable(false)


        dataTextView = findViewById(R.id.dataTextView)
        viewPager = findViewById(R.id.viewPager)
        img_adapter = ImageAdapter(imageBitmaps, boundingBoxes)
        viewPager.adapter = img_adapter

        cameraButton = findViewById(R.id.cameraButton)

        cameraButton.setOnClickListener {
            val deviceList = convertDevicesToStringList(devices)
            //deviceList.add("Test")
            val intent = Intent(this, CameraActivity::class.java)
            intent.putStringArrayListExtra("device_list", deviceList)
            startActivity(intent)
        }



        // UPDATED: Use BluetoothManager to get the adapter
        val bluetoothManager = getSystemService(BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        checkAndRequestPermissions()

        bluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
        val listView: ListView = findViewById(R.id.listView)



        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        listView.adapter = adapter
        listView.setOnItemClickListener { _, _, position, _ ->
            val device = devices[position]
            connectToDevice(device)
            val deviceList = convertDevicesToStringList(devices)

            // Add if needed for testing.
            //deviceList.add("Test")

            /// TODO: Testing
            //val intent = Intent(this, CameraActivity::class.java)
            //intent.putStringArrayListExtra("device_list", deviceList)
            //startActivity(intent)
        }

        val disconnectButton: Button = findViewById(R.id.disconnectButton)
        disconnectButton.setOnClickListener { disconnectFromDevice() }



        sendBleData = findViewById<EditText>(R.id.sendBleData)
        sendBleButton  = findViewById<Button>(R.id.sendBleButton)
        sendBleButton.setOnClickListener {
            val inputText = sendBleData.text.toString()
            // Do something with inputText
            Log.d("MyApp", "User typed: $inputText")
            sendAnimalStatus(0x20, true, true, true, true)
        }

        BleManager.getInstance().setConnected(false)

        BleManager.getInstance().registerDataListener(this)


        startScan()
    }


    override fun onBleDataReceived(data: ByteArray) {
        receiveData(data);
    }


    private fun logoutUser() {
        val editor = sharedPreferences.edit()
        editor.putBoolean("isLoggedIn", false)
        editor.apply()
        startActivity(Intent(this, LoginActivity::class.java))
        finish()
    }
    @SuppressLint("MissingPermission")
    fun convertDevicesToStringList(devices: MutableList<BluetoothDevice>): ArrayList<String> {
        val deviceStrings = ArrayList<String>()
        for (device in devices) {
            // If the device does not contain ESP32 in it then
            // we go to the next and do not display it
            if (!device.name.contains("ESP32")) {
                continue;
            } else {
                // You can choose how to represent each device, for example:
                // "${device.name} - ${device.address}"
                val deviceInfo = "${device.name ?: "Unknown Device"} - ${device.address}"
                deviceStrings.add(deviceInfo)
            }
        }
        return deviceStrings
    }

    // TODO: Scan only once add a button to refresh?
    @SuppressLint("MissingPermission")
    private fun startScan() {
        // For Android 12+, ensure BLUETOOTH_SCAN permission is granted
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(
                    this,
                    android.Manifest.permission.BLUETOOTH_SCAN
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                Log.e("BLE", "Missing BLUETOOTH_SCAN permission!")
                return
            }
        }
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
                // If the device does not contain ESP32 in it then
                // we go to the next and do not display it
                // TODO:
                    if (device.name != null && device.name.contains("ESP32")) {
                        devices.add(device)
                        adapter.add("${device.name ?: "Unknown Device"}\n${device.address}")
                        adapter.notifyDataSetChanged()

                        BleManager.getInstance().addDevice(device)
                    }
                }
        }
    }

    private val scanCallbackEnd = object : ScanCallback() {
    }

    // UPDATED: Use newer connectGatt signature on API 23+ with transport parameter
    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        if (!BleManager.getInstance().isConnected()) {
            val gatt = device.connectGatt(this, false, BleManager.getInstance().gattCallback, BluetoothDevice.TRANSPORT_LE)
            BleManager.getInstance().setGatt(gatt)
            BleManager.getInstance().setCurrentDevice(device)

            Toast.makeText(this, "Connecting to ${device.name}", Toast.LENGTH_SHORT).show()
            // TODO: Update button and text to show where connected to.
            isConnectedText.setText("Connected to: ${device.name}")
            isConnectedButton.setEnabled(true)
            isConnectedButton.setClickable(true)
            BleManager.getInstance().setConnected(true)

            bluetoothLeScanner.stopScan(scanCallbackEnd)
        } else {

            Toast.makeText(this, "Already connected to ${device.name}", Toast.LENGTH_SHORT).show()

        }
    }

    // TODO: This might be dumb haha
    val activity = this  // inside MainActivity

    @SuppressLint("MissingPermission")
    private fun disconnectFromDevice() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val device = BleManager.getInstance().getCurrentDevice()
        val gatt = BleManager.getInstance().getGatt()

        if (device == null || gatt == null) {
            Log.w("BLE", "No device or GATT to disconnect")
            isConnectedText.text = "Not Connected"
            isConnectedButton.isEnabled = false
            isConnectedButton.isClickable = false
            return
        }

        // 1. Attempt standard disconnect
        Log.d("BLE", "Starting disconnect sequence for ${device.address}")
        BleManager.getInstance().disconnect()

        // 2. Schedule a check to verify disconnection
        Handler(Looper.getMainLooper()).postDelayed({
            val connectionState = bluetoothManager.getConnectionState(device, BluetoothProfile.GATT)

            if (connectionState != BluetoothProfile.STATE_DISCONNECTED) {
                Log.w("BLE", "Device appears to still be connected after disconnect attempt! Forcing close...")

                // 3. Force close the GATT connection
                try {
                    gatt.close()
                    BleManager.getInstance().setGatt(null)

                    // 4. Use reflection to clear system cache (requires specific permission)
                    try {
                        val bluetoothGattClass = BluetoothGatt::class.java
                        val refreshMethod = bluetoothGattClass.getMethod("refresh")
                        refreshMethod.invoke(gatt)
                        Log.d("BLE", "Attempted to refresh GATT state via reflection")
                    } catch (e: Exception) {
                        Log.e("BLE", "Could not use reflection to refresh GATT", e)
                    }

                    // 5. Reset Bluetooth adapter if necessary (very aggressive approach)
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        // On Android 13+, we need to be careful about adapter resets
                        Log.w("BLE", "Clearing device cache without adapter reset")
                    } else {
                        val adapter = bluetoothManager.adapter
                        try {
                            Log.w("BLE", "Attempting to reset Bluetooth adapter")
                            adapter.disable()
                            // We'll re-enable it after a delay
                            Handler(Looper.getMainLooper()).postDelayed({
                                adapter.enable()
                                Log.d("BLE", "Bluetooth adapter re-enabled")
                            }, 2000)
                        } catch (e: Exception) {
                            Log.e("BLE", "Failed to reset Bluetooth adapter", e)
                        }
                    }
                } catch (e: Exception) {
                    Log.e("BLE", "Error during force disconnect", e)
                }
            } else {
                Log.d("BLE", "Device successfully disconnected confirmed by system")
            }

            // Update state in BLE manager
            BleManager.getInstance().setConnected(false)
            BleManager.getInstance().setCurrentDevice(null)

            // Always update UI
            isConnectedText.text = "Not Connected"
            isConnectedButton.isEnabled = false
            isConnectedButton.isClickable = false
        }, 1000)

        Toast.makeText(this, "Disconnecting from device...", Toast.LENGTH_SHORT).show()
    }



    private fun ByteArray.startsWith(marker: ByteArray): Boolean =
        this.size >= marker.size && this.copyOfRange(0, marker.size).contentEquals(marker)

    /**
     * Processes incoming BLE data, handling image and bounding box transmissions.
     */
    private fun receiveData(data: ByteArray) {
        // Log the raw bytes in hex form
        val hexData = data.joinToString(" ") { "%02X".format(it) }
        Log.d("BLE", "Raw Received Bytes: $hexData")

        // Marker constants
        val START_IMAGE = "START;".toByteArray(Charsets.UTF_8)
        val END_IMAGE = "END;".toByteArray(Charsets.UTF_8)
        val START_BBOX = "START_BBOX;".toByteArray(Charsets.UTF_8)
        val END_BBOX = "END_BBOX;".toByteArray(Charsets.UTF_8)
        val START_NODE_INFO = "START_NODE_INFO;".toByteArray(Charsets.UTF_8)
        val END_NODE_INFO = "END_NODE_INFO;".toByteArray(Charsets.UTF_8)



        when {
            // Start of image transmission
            data.startsWith(START_IMAGE) -> {
                Log.d("BLE", "Start of image data")
                receivedImageBuilder = StringBuilder()
                processingImage = true
            }

            data.startsWith(END_IMAGE) -> {
                Log.d("BLE", "End of image transmission")
                if (processingImage) {
                    val completeBase64 = receivedImageBuilder.toString()
                    val decodedBytes = Base64.decode(completeBase64, Base64.DEFAULT)
                    val bitmap = BitmapFactory.decodeByteArray(decodedBytes, 0, decodedBytes.size)
                    if (bitmap != null) {
                        runOnUiThread {
                            displayImage(bitmap)
                        }
                    } else {
                        Log.e("BLE", "Failed to decode image data")
                    }
                }
                processingImage = false
            }

            data.startsWith(START_BBOX) -> {
                Log.d("BLE", "Start of bounding box data")
                currentImagePosition = imageBitmaps.size - 1
                receivedBboxBuilder.reset()
                processingBoundingBox = true
            }

            data.startsWith(END_BBOX) -> {
                Log.d("BLE", "End of bounding box transmission")
                if (processingBoundingBox) {
                    Log.d("BLE", "End of bounding box transmission")
                    val bboxData = receivedBboxBuilder.toByteArray()
                    val bboxList = parseBoundingBoxData(bboxData)
                    if (currentImagePosition >= 0) {
                        while (boundingBoxes.size <= currentImagePosition) {
                            boundingBoxes.add(emptyList())
                        }
                        runOnUiThread {
                            boundingBoxes[currentImagePosition] = bboxList
                            img_adapter.notifyItemChanged(currentImagePosition)
                        }
                    }
                }
                processingBoundingBox = false
            }

            // Append incoming data for image
            processingImage -> {
                receivedImageBuilder.append(String(data, Charsets.UTF_8))
                Log.d("BLE", "Appending image data chunk...")
            }
            // Append incoming data for bounding boxes
            processingBoundingBox -> {
                receivedBboxBuilder.write(data)
                Log.d("BLE", "Appending bounding box data chunk...")
            }
            else -> {
                Log.w("BLE", "Unexpected data received (no START/END markers).")
            }
        }
    }

    /**
     * Parses bounding box data. Each bounding box is 10 bytes:
     * 2 bytes each for x, y, width, height, 1 byte for score, 1 byte for target.
     */
    private fun parseBoundingBoxData(bboxData: ByteArray): List<BoundingBox> {
        val bboxList = mutableListOf<BoundingBox>()
        var index = 0
        while (index + 10 <= bboxData.size) {
            val x = ((bboxData[index].toInt() and 0xFF) shl 8) or (bboxData[index + 1].toInt() and 0xFF)
            val y = ((bboxData[index + 2].toInt() and 0xFF) shl 8) or (bboxData[index + 3].toInt() and 0xFF)
            val w = ((bboxData[index + 4].toInt() and 0xFF) shl 8) or (bboxData[index + 5].toInt() and 0xFF)
            val h = ((bboxData[index + 6].toInt() and 0xFF) shl 8) or (bboxData[index + 7].toInt() and 0xFF)
            val score = bboxData[index + 8].toInt() and 0xFF
            val target = bboxData[index + 9].toInt() and 0xFF
            bboxList.add(BoundingBox(x, y, w, h, score, target))
            index += 10
        }
        return bboxList
    }

    /**
     * Adds a new bitmap to the list, updates the ViewPager,
     * saves the image to storage, and records the detection in the database.
     */
    private fun displayImage(bitmap: Bitmap) {
        imageBitmaps.add(bitmap)
        boundingBoxes.add(emptyList()) // Placeholder for bounding boxes
        img_adapter.notifyItemInserted(imageBitmaps.size - 1)
        viewPager.setCurrentItem(imageBitmaps.size - 1, true)
        storeImageRecord(bitmap)
    }

    /**
     * Saves the bitmap as a PNG file in internal storage.
     * Returns the absolute file path or null if saving fails.
     */
    private fun saveImageToInternalStorage(bitmap: Bitmap): String? {
        val filename = "image_${System.currentTimeMillis()}.png"
        return try {
            openFileOutput(filename, Context.MODE_PRIVATE).use { stream ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream)
            }
            "${filesDir.absolutePath}/$filename"
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }


    /**
     * Stores a detection record in the database.
     * The record includes the detected animal, timestamp, camera info, and image file path.
     */
    private fun storeImageRecord(bitmap: Bitmap) {
        val imagePath = saveImageToInternalStorage(bitmap)
        if (imagePath != null) {
            // Use the current radio selection as the detected animal.
            val animal = "UNSET"//getSelectedAnimal()
            // Format the current timestamp (adjust format as needed).
            val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()).format(Date())
            // For this example, we use a constant for camera; replace with real camera info if available.
            val camera = "Default Camera"
            val success = dbHelper.insertDetectionRecord(animal, timestamp, camera, imagePath)
            if (success) {
                Log.i("DB", "Detection record inserted successfully")
            } else {
                Log.e("DB", "Failed to insert detection record")
            }
        } else {
            Log.e("DB", "Failed to save image to internal storage")
        }
    }



    /**
     * Sends animal status data via BLE.
     * The statuses are encoded as 1 for true and 0 for false.
     */
    @SuppressLint("MissingPermission")
    fun sendAnimalStatus(
        packetId: Byte,
        birdEnabled: Boolean,
        catEnabled: Boolean,
        dogEnabled: Boolean,
        squirrelEnabled: Boolean
    ) {
        val data = byteArrayOf(
            packetId,
            if (birdEnabled) 1 else 0,
            if (catEnabled) 1 else 0,
            if (dogEnabled) 1 else 0,
            if (squirrelEnabled) 1 else 0
        )

        val success = BleManager.getInstance().sendData(data)
        if (success) {
            Log.d("BLE", "Animal status sent with packet ID $packetId")
        } else {
            Log.e("BLE", "Failed to send animal status")
        }
    }


    private fun checkAndRequestPermissions() {
        val permissions = mutableListOf(
            android.Manifest.permission.BLUETOOTH,
            android.Manifest.permission.BLUETOOTH_ADMIN
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(android.Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(android.Manifest.permission.BLUETOOTH_CONNECT)
        }
        val missingPermissions = permissions.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missingPermissions.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missingPermissions.toTypedArray(), PERMISSION_REQUEST_CODE)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST_CODE) {
            val deniedPermissions = permissions.zip(grantResults.toTypedArray())
                .filter { it.second != PackageManager.PERMISSION_GRANTED }
            if (deniedPermissions.isNotEmpty()) {
                Toast.makeText(this, "Bluetooth permissions are required!", Toast.LENGTH_LONG).show()
            }
        }
    }

    override fun onDeviceDisconnected(device: BluetoothDevice) {
        Log.d("BLE", "Device disconnect MainActivity")
        runOnUiThread {
            Toast.makeText(this, "Device disconnected", Toast.LENGTH_SHORT).show()
            Toast.makeText(this, "TODO: RESET TO DEFAULT VIEW", Toast.LENGTH_SHORT).show()

        }
    }

    @SuppressLint("MissingPermission")
    override fun onDeviceConnected(device: BluetoothDevice) {
        Log.d("BLE", "Device CONNECTED MainActivity")
        runOnUiThread {
            Toast.makeText(this, "Device connected: ${device.name ?: device.address}", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        BleManager.getInstance().disconnect()
    }

}