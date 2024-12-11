package com.example.myapplication

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Bundle
import android.util.Log
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.util.*
import android.widget.ImageView
import android.util.Base64
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import androidx.viewpager2.widget.ViewPager2
import java.nio.charset.Charset
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import java.io.ByteArrayOutputStream

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
    private val boundingBoxes: List<List<BoundingBox>> // List of bounding boxes for each image
) : RecyclerView.Adapter<ImageAdapter.ImageViewHolder>() {

    class ImageViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val imageView: ImageView = itemView.findViewById(R.id.imageView)
        val boundingBoxOverlay: ImageView = itemView.findViewById(R.id.boundingBoxOverlay)
        val scoreTextView: TextView = itemView.findViewById(R.id.scoreTextView) // New TextView for score
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ImageViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_image, parent, false)
        return ImageViewHolder(view)
    }

    override fun onBindViewHolder(holder: ImageViewHolder, position: Int) {
        val bitmap = images[position]
        holder.imageView.setImageBitmap(bitmap)

        // Get bounding boxes for this image position if they exist
        val bboxList = boundingBoxes.getOrNull(position) ?: emptyList()
        holder.boundingBoxOverlay.setImageBitmap(drawBoundingBoxes(bitmap, bboxList))

        if (bboxList.isNotEmpty()) {
            // Assuming you're only interested in the first bounding box for score and target display
            val firstBox = bboxList.first()
            holder.scoreTextView.text = "Score: ${firstBox.score}" + "Target: ${firstBox.target}"
        } else {
            holder.scoreTextView.text = "Score: N/A"  + "Target: N/A"
        }

    }

    override fun getItemCount(): Int {
        return images.size
    }

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
            // Convert YOLO format (center_x, center_y, width, height) back to pixel values
            val centerX = box.x.toFloat() // center_x in pixels
            val centerY = box.y.toFloat() // center_y in pixels
            val width = box.width.toFloat() // width in pixels
            val height = box.height.toFloat() // height in pixels

            // Calculate the corner coordinates for the rectangle
            val left = centerX - (width / 2)
            val top = centerY - (height / 2)
            val right = centerX + (width / 2)
            val bottom = centerY + (height / 2)

            // Ensure coordinates are within bounds
            val adjustedLeft = left.coerceIn(0f, bitmapWidth)
            val adjustedTop = top.coerceIn(0f, bitmapHeight)
            val adjustedRight = right.coerceIn(0f, bitmapWidth)
            val adjustedBottom = bottom.coerceIn(0f, bitmapHeight)
            Log.i("BLE", "Coordinates are:" + adjustedLeft + " " + adjustedTop  + " " + adjustedRight  + " " + adjustedBottom)

            // Draw the rectangle on the canvas
            canvas.drawRect(adjustedLeft, adjustedTop, adjustedRight, adjustedBottom, paint)
        }

        return mutableBitmap
    }

}


class MainActivity : AppCompatActivity() {
    private lateinit var bluetoothAdapter: BluetoothAdapter
    private lateinit var bluetoothLeScanner: BluetoothLeScanner
    private val devices: MutableList<BluetoothDevice> = mutableListOf()
    private lateinit var adapter: ArrayAdapter<String>
    private lateinit var bluetoothGatt: BluetoothGatt
    private lateinit var characteristic: BluetoothGattCharacteristic
    private val imageData = mutableListOf<Byte>() // List to store received image data chunks
    private var receivedImageBuilder = StringBuilder()
   // private var receivedBboxBuilder = StringBuilder()
   private var receivedBboxBuilder = ByteArrayOutputStream() // Use ByteArrayOutputStream for bounding box data
    private lateinit var imageInfoTextView: TextView

    private var currentImagePosition = -1
    private val boundingBoxes = mutableListOf<List<BoundingBox>>() // List of bounding boxes for each image


    private lateinit var viewPager: ViewPager2
    private lateinit var img_adapter: ImageAdapter
    private val imageBitmaps = mutableListOf<Bitmap>() // List to hold the received bitmaps

    private lateinit var viewNotifications: ViewPager2



    // Replace with your service and characteristic UUIDs
    private val SERVICE_UUID = "12345678-1234-1234-1234-123456789012"
    private val CHARACTERISTIC_UUID = "87654321-4321-4321-4321-210987654321"

    private val PERMISSION_REQUEST_CODE = 100
    private lateinit var imageView: ImageView

    private lateinit var dataTextView: TextView // To display received data

    private lateinit var radioCat: RadioButton
    private lateinit var radioDog: RadioButton
    private lateinit var radioSquirrel: RadioButton
    private lateinit var radioBird: RadioButton
    private lateinit var submitButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        dataTextView = findViewById(R.id.dataTextView) // TextView to display data
        //imageView = findViewById(R.id.imageView)

        viewPager = findViewById(R.id.viewPager)
        img_adapter = ImageAdapter(imageBitmaps, boundingBoxes)
        viewPager.adapter = img_adapter

        // Initialize your RadioButtons and Submit Button
        radioCat = findViewById(R.id.radioCat)
        radioDog = findViewById(R.id.radioDog)
        radioSquirrel = findViewById(R.id.radioSquirrel)
        radioBird = findViewById(R.id.radioBird)
        submitButton = findViewById(R.id.submitButton)

        // Set OnClickListener for the submit button
        submitButton.setOnClickListener {
            // Get the selected animal status
            val birdEnabled = radioBird.isChecked
            val catEnabled = radioCat.isChecked
            val dogEnabled = radioDog.isChecked
            val squirrelEnabled = radioSquirrel.isChecked

            // Assign a packet ID, for example, 1
            val packetId: Byte = 0x20

            // Call the function to send the animal status
            sendAnimalStatus(packetId, birdEnabled, catEnabled, dogEnabled, squirrelEnabled)
        }

        // Initialize Bluetooth adapter
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth not supported", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        // Check and request permissions
        checkAndRequestPermissions()

        bluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
        val listView: ListView = findViewById(R.id.listView)

        // Set up the adapter for the ListView
        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        listView.adapter = adapter

        // Handle item clicks
        listView.setOnItemClickListener { _, _, position, _ ->
            val device = devices[position]
            connectToDevice(device)
        }


        var disconnectButton : Button = findViewById(R.id.disconnectButton)
        disconnectButton.setOnClickListener {
            disconnectFromDevice()
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
    }



    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        bluetoothGatt = device.connectGatt(this, false, gattCallback)
        Toast.makeText(this, "Connecting to ${device.name}", Toast.LENGTH_SHORT).show()
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



    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                gatt.discoverServices()
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                Log.i("BLE", "Disconnected from device.")
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(UUID.fromString(SERVICE_UUID))
                if (service != null) {
                    characteristic = service.getCharacteristic(UUID.fromString(CHARACTERISTIC_UUID))
                    if (characteristic != null) {
                        enableNotifications(gatt, characteristic)
                    } else {
                        Log.e("BLE", "Characteristic not found.")
                    }
                } else {
                    Log.e("BLE", "Service not found.")
                }
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = characteristic.value?.let { String(it) } ?: "Unknown"
            Log.i("BLE", "Received Data: $value")
            receiveData(characteristic.value)
            //val chunk = characteristic.value
            //imageData.addAll(chunk.toList())
            //reassembleImage();
            runOnUiThread {
                dataTextView.text = "Received: $value"
            }
        }
    }


    /*private fun receiveData(data: ByteArray) {
        val receivedString = String(data, Charset.forName("UTF-8"))

        if (receivedString.startsWith("START;")) {
/*            val parts = receivedString.split(";")
            if (parts.size >= 2) {
                val imageId = parts[1].toInt()
                Log.d("BLE", "Receiving image: ID = $imageId")
                receivedImageBuilder = StringBuilder() // Prepare to collect image data
            }*/
            receivedImageBuilder = StringBuilder()
        } else if (receivedString.startsWith("END;")) {
            Log.d("BLE", "End of image transmission.")
            val completeBase64ImageData = receivedImageBuilder.toString()
            val imageData = Base64.decode(completeBase64ImageData, Base64.DEFAULT)
            val bitmap: Bitmap? = BitmapFactory.decodeByteArray(imageData, 0, imageData.size)
            if (bitmap != null) {
                runOnUiThread {
                    // This ensures that the UI update happens on the main thread
                    displayImage(bitmap) // Call displayImage to add the new image
                }

            } else {
                Log.d("BLE", "Failed to decode image.")
                runOnUiThread {
                    dataTextView.text = "Failed to decode image."
                }
            }
        } else {
            receivedImageBuilder.append(receivedString) // Collect image data
        }
    }*/


    fun cleanAndDecodeBase64(base64Data: String, width: Int, height: Int): ByteArray? {
        // Base64 regex to identify valid characters (letters, numbers, +, /, and padding =)
        val base64Regex = Regex("^[A-Za-z0-9+/=]+\$")

        // Split the string into valid and invalid parts
        val cleanedBase64 = StringBuilder()
        base64Data.chunked(4).forEach { chunk ->
            if (chunk.matches(base64Regex)) {
                cleanedBase64.append(chunk)
            } else {
                // Replace invalid chunks with Base64 encoding of a black pixel
                // Here, `blackPixel` is a placeholder for a small black JPEG image
                val blackPixel = ByteArray(width * height * 3) { 0.toByte() } // A black RGB array
                val encodedBlack = Base64.encodeToString(blackPixel, Base64.NO_WRAP)
                cleanedBase64.append(encodedBlack)
            }
        }

        return try {
            // Decode the cleaned Base64 string
            Base64.decode(cleanedBase64.toString(), Base64.DEFAULT)
        } catch (e: IllegalArgumentException) {
            e.printStackTrace()
            null
        }
    }



    private var processingImage = false
    private var processingBoundingBox = false


    private fun receiveData(data: ByteArray) {
        val hexData = data.joinToString(" ") { "%02X".format(it) }
        Log.d("BLE", "Raw Received Bytes: $hexData")

        val startBboxBytes = "START_BBOX;".toByteArray(Charsets.UTF_8)
        val endBboxBytes = "END_BBOX;".toByteArray(Charsets.UTF_8)
        val startImageBytes = "START;".toByteArray(Charsets.UTF_8)
        val endImageBytes = "END;".toByteArray(Charsets.UTF_8)

        when {
            data.size >= startImageBytes.size && data.copyOfRange(0, startImageBytes.size).contentEquals(startImageBytes) -> {
                // Start receiving image data
                receivedImageBuilder = StringBuilder() // Reset builder for image data
                processingImage = true
            }

            data.size >= endImageBytes.size && data.copyOfRange(0, endImageBytes.size).contentEquals(endImageBytes) -> {
                // End of image transmission
                if (processingImage) {
                    Log.d("BLE", "End of image transmission.")
                    val completeBase64ImageData = receivedImageBuilder.toString()
                    val imageData = Base64.decode(completeBase64ImageData, Base64.DEFAULT)

                    //val imageData = cleanAndDecodeBase64(completeBase64ImageData, 480, 480)
                    val bitmap: Bitmap? =
                        imageData?.let { BitmapFactory.decodeByteArray(imageData, 0, it.size) }
                    Log.d("BLE", "Length of received image is: ${completeBase64ImageData.length}")

                    if (bitmap != null) {
                        runOnUiThread {
                            displayImage(bitmap) // Update UI on main thread
                        }
                    } else {
                        Log.d("BLE", "Failed to decode image.")
                        runOnUiThread {
                            dataTextView.text = "Failed to decode image."
                        }
                    }
                }
                processingImage = false // Reset flag
            }

            data.size >= startBboxBytes.size && data.copyOfRange(0, startBboxBytes.size).contentEquals(startBboxBytes) -> {
                // Start receiving bounding boxes
                Log.d("BLE", "Receiving bounding boxes for the last image received.")
                currentImagePosition = imageBitmaps.size - 1 // Last received image position
                receivedBboxBuilder.reset() // Reset the byte builder for bounding boxes
                processingBoundingBox = true
            }

            data.size >= endBboxBytes.size && data.copyOfRange(0, endBboxBytes.size).contentEquals(endBboxBytes) -> {
                // End of bounding box transmission
                if (processingBoundingBox) {
                    Log.d("BLE", "End of bounding box transmission.")
                    val bboxData = receivedBboxBuilder.toByteArray() // Convert ByteArrayOutputStream to ByteArray
                    val bboxList = parseBoundingBoxData(bboxData)

                    // Ensure the boundingBoxes list is large enough and assign the received bounding boxes
                    if (currentImagePosition >= 0) {
                        while (boundingBoxes.size <= currentImagePosition) {
                            boundingBoxes.add(emptyList())
                        }

                        // Update the bounding boxes on the main thread
                        runOnUiThread {
                            boundingBoxes[currentImagePosition] = bboxList
                            img_adapter.notifyItemChanged(currentImagePosition) // Notify the adapter to refresh the bounding boxes
                        }
                    }
                }
                processingBoundingBox = false // Reset flag
            }

            processingImage -> {
                // If currently receiving image data
                receivedImageBuilder.append(String(data, Charsets.UTF_8)) // Keep using StringBuilder for image data
                Log.d("BLE", "Appending to image data...")
            }

            processingBoundingBox -> {
                // If currently receiving bounding box data
                receivedBboxBuilder.write(data) // Write the received bytes to the ByteArrayOutputStream
                Log.d("BLE", "Appending to bounding box data...")
            }

            else -> {
                Log.w("BLE", "Unexpected data received.")
            }
        }
    }



    private fun logReceivedBytes(data: ByteArray) {
        val hexString = data.joinToString(separator = " ") { byte -> "%02X".format(byte) }
        Log.d("BLE", "Received Bytes: $hexString")
    }


    // Function to parse bounding box data from a byte array
    private fun parseBoundingBoxData(bboxData: ByteArray): List<BoundingBox> {
        val bboxList = mutableListOf<BoundingBox>()
        var index = 0
        logReceivedBytes(bboxData) // Log the raw received data


        // Each bounding box uses 10 bytes: 2 bytes each for x, y, w, h, and 1 byte each for score and target
        while (index + 10 <= bboxData.size) { // Ensure there are enough bytes left for a full bounding box
            // Convert bytes to unsigned integers
            val x = ((bboxData[index].toInt() and 0xFF shl 8) or (bboxData[index + 1].toInt() and 0xFF))
            val y = ((bboxData[index + 2].toInt() and 0xFF shl 8) or (bboxData[index + 3].toInt() and 0xFF))
            val w = ((bboxData[index + 4].toInt() and 0xFF shl 8) or (bboxData[index + 5].toInt() and 0xFF))
            val h = ((bboxData[index + 6].toInt() and 0xFF shl 8) or (bboxData[index + 7].toInt() and 0xFF))
            val score = bboxData[index + 8].toInt() and 0xFF
            val target = bboxData[index + 9].toInt() and 0xFF

            // Create a BoundingBox object and add it to the list
            bboxList.add(BoundingBox(x, y, w, h, score, target))
            Log.d("BLE", "Deserialized BoundingBox: x=$x, y=$y, w=$w, h=$h, score=$score, target=$target")

            // Move the index forward by 10 bytes for the next bounding box
            index += 10
        }

        return bboxList
    }





    private fun displayImage(bitmap: Bitmap) {
        imageBitmaps.add(bitmap) // Add the new bitmap to the list
        boundingBoxes.add(emptyList()) // Add an empty list for bounding boxes initially

        img_adapter.notifyItemInserted(imageBitmaps.size - 1) // Notify the adapter that a new item has been added

        // Optional: Set the current item to the newly added image
        viewPager.setCurrentItem(imageBitmaps.size - 1, true)
    }




    @SuppressLint("MissingPermission")
    private fun enableNotifications(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        gatt.setCharacteristicNotification(characteristic, true)

        val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
        descriptor?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        gatt.writeDescriptor(descriptor)

        Log.i("BLE", "Notifications enabled for ${characteristic.uuid}")
    }


    // Function to send animal status with a packet ID via BLE
    @SuppressLint("MissingPermission")
    fun sendAnimalStatus(packetId: Byte, birdEnabled: Boolean, catEnabled: Boolean, dogEnabled: Boolean, squirrelEnabled: Boolean) {
        if (!this::bluetoothGatt.isInitialized) {
            Log.i("BLE", "Have not connected to a device yet... not sending.")
            return

        }

        if (bluetoothGatt == null) {
            Log.e("BLE", "BluetoothGatt is null. Cannot send animal status.")
            return
        }

        // Discover services if not already done
        val services = bluetoothGatt.services
        if (services.isEmpty()) {
            Log.e("BLE", "No services found. Ensure the connection is established.")
            return
        }

        // Use the correct UUID for your service and characteristic
        val characteristicUUID = UUID.fromString(CHARACTERISTIC_UUID) // Replace with your characteristic UUID
        val serviceUUID = UUID.fromString(SERVICE_UUID) // Replace with your service UUID

        // Get the service
        val service = bluetoothGatt.getService(serviceUUID)
        if (service == null) {
            Log.e("BLE", "Service not found.")
            return
        }

        // Get the characteristic
        val characteristic = service.getCharacteristic(characteristicUUID)
        if (characteristic == null) {
            Log.e("BLE", "Characteristic not found.")
            return
        }

        if (characteristic.properties and BluetoothGattCharacteristic.PROPERTY_WRITE == 0) {
            Log.e("BLE", "Characteristic does not have write property.")
            return
        }
        // Create a byte array to hold the packet ID and statuses
        val animalStatus = ByteArray(5) // 1 byte for packet ID + 4 bytes for animal statuses

        // Set the packet ID (you can choose an appropriate value for your use case)
        animalStatus[0] = packetId // Set the packet ID

        // Convert Boolean values to byte values (1 for true, 0 for false)
        animalStatus[1] = if (birdEnabled) 1 else 0 // bBird
        animalStatus[2] = if (catEnabled) 1 else 0  // bCat
        animalStatus[3] = if (dogEnabled) 1 else 0  // bDog
        animalStatus[4] = if (squirrelEnabled) 1 else 0 // bSquirrel

        var hello : String = "hello"
        characteristic.setValue(hello);
        val success = bluetoothGatt.writeCharacteristic(characteristic) // Send the data

        if (success) {
            Log.d("BLE", "Animal status sent successfully with packet ID $packetId")
        } else {
            Log.e("BLE", "Failed to send animal status")
        }
    }

    private fun checkAndRequestPermissions() {
        val permissions = arrayOf(
            android.Manifest.permission.BLUETOOTH_SCAN,
            android.Manifest.permission.BLUETOOTH_CONNECT,
            android.Manifest.permission.ACCESS_FINE_LOCATION
        )

        val missingPermissions = permissions.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missingPermissions.toTypedArray(), PERMISSION_REQUEST_CODE)
        }
    }
}
