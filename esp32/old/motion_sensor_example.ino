#define PIR_PIN 13  // Define the pin where the PIR sensor is connected

void setup() {
    Serial.begin(115200);  // Start the serial communication
    pinMode(PIR_PIN, INPUT);  // Set the PIR pin as input

    // Configure the PIR_PIN to wake the ESP32 from deep sleep
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);  // Wake up on HIGH signal
}

void loop() {
    Serial.println("Going to sleep...");
    // Go to deep sleep
    //esp_deep_sleep_start();
    // The ESP32 will wake up here when motion is detected
    Serial.println("Woke up!");

    // Optionally, add your motion detection logic here
    int motionDetected = digitalRead(PIR_PIN);  // Read the state of the PIR sensor

    if (motionDetected == HIGH) {
        Serial.println("Motion detected!"); 
        // Perform actions based on motion detection
    } else {
        Serial.println("No motion.");
    }

    delay(1000);  // Delay to stabilize output before going back to sleep
}
