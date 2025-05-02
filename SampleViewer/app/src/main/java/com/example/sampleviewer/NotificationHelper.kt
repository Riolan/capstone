package com.example.sampleviewer // Or your common utils package

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color // If you customize channel appearance
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.example.myapplication.R // CHANGE to your actual R file import
// CHANGE to your actual MainActivity import (or the desired target activity)
import com.example.sampleviewer.MainActivity

// Using 'object' creates a singleton instance automatically
object NotificationHelper {

    // --- Configuration ---
    // IMPORTANT: Define your Channel ID. Use a descriptive unique string.
    // This MUST match the ID used when creating the channel.
    private const val CHANNEL_ID = "YOUR_APP_DEFAULT_CHANNEL" // <<< CHANGE THIS

    // Define your notification icon
    private val DEFAULT_NOTIFICATION_ICON = R.drawable.stc_logo_nobg // <<< CHANGE THIS (mandatory)

    private const val TAG = "NotificationHelper"

    /**
     * Creates the notification channel required for Android Oreo (API 26) and above.
     * Call this ONCE when your application starts, typically in your Application class's onCreate().
     * It's safe to call this multiple times; creating an existing channel performs no operation.
     *
     * @param context Application context is usually sufficient.
     */
    fun createNotificationChannel(context: Context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val name = context.getString(R.string.default_notification_channel_name) // Define in strings.xml
            val descriptionText = context.getString(R.string.default_notification_channel_description) // Define in strings.xml
            val importance = NotificationManager.IMPORTANCE_HIGH // Choose importance level
            val channel = NotificationChannel(CHANNEL_ID, name, importance).apply {
                description = descriptionText
                // Optional: Configure other channel properties (lights, vibration, etc.)
                // enableLights(true)
                // lightColor = Color.RED
                // enableVibration(true)
                // vibrationPattern = longArrayOf(100, 200, 300, 400, 500)
            }
            // Register the channel with the system
            val notificationManager: NotificationManager? =
                context.getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager

            if (notificationManager == null) {
                Log.e(TAG, "Failed to get NotificationManager system service.")
                return
            }

            try {
                notificationManager.createNotificationChannel(channel)
                Log.i(TAG, "Notification channel '$CHANNEL_ID' created or already exists.")
            } catch (e: Exception) {
                Log.e(TAG, "Error creating notification channel '$CHANNEL_ID'", e)
            }
        }
    }

    /**
     * Sends an Android notification using the default channel.
     * Make sure [createNotificationChannel] has been called at least once before using this.
     * Handles the POST_NOTIFICATIONS permission check for Android 13 (API 33)+.
     *
     * @param context Context (e.g., from Fragment's requireContext() or Activity)
     * @param title Notification title.
     * @param message Notification body text.
     * @param targetActivity Optional: Specify the Activity class to launch on tap. Defaults to MainActivity.
     */
    fun sendNotification(
        context: Context,
        title: String,
        message: String,
        targetActivity: Class<*> = MainActivity::class.java // Default target
    ) {
        // Intent to launch when notification is tapped
        val intent = Intent(context, targetActivity).apply {
            // Example flags, adjust if needed (e.g., if passing data)
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            // If you need to pass data:
            // putExtra("key", "value")
        }

        // Use FLAG_IMMUTABLE for security unless FLAG_UPDATE_CURRENT is truly needed
        // If using FLAG_UPDATE_CURRENT, ensure the request code is unique if the intent extras differ
        val pendingIntentFlag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT // Update if intent extras change
        } else {
            PendingIntent.FLAG_UPDATE_CURRENT
        }
        val pendingIntent = PendingIntent.getActivity(
            context,
            0 /* Request code */,
            intent,
            pendingIntentFlag
        )

        val builder = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(DEFAULT_NOTIFICATION_ICON) // Mandatory
            .setContentTitle(title)
            .setContentText(message)
            // Style for potentially longer text
            .setStyle(NotificationCompat.BigTextStyle().bigText(message))
            .setPriority(NotificationCompat.PRIORITY_HIGH) // For heads-up notification visibility
            .setContentIntent(pendingIntent) // Intent to fire on tap
            .setAutoCancel(true) // Removes notification on tap
        // Optional: Add actions, progress bar, etc.
        // .addAction(R.drawable.ic_action, "Action Text", actionPendingIntent)

        val notificationManager = NotificationManagerCompat.from(context)

        // --- Permission Check for Android 13+ ---
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) { // API 33+
            if (ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) ==
                PackageManager.PERMISSION_GRANTED
            ) {
                postNotification(notificationManager, builder)
            } else {
                // Permission not granted. Cannot post notification.
                Log.w(TAG, "POST_NOTIFICATIONS permission not granted. Cannot show notification.")
                // Consider alternative feedback or logging. You typically request permission elsewhere.
            }
        } else {
            // Pre-Android 13: Permission granted at install time
            postNotification(notificationManager, builder, isPreTiramisu = true)
        }
    }

    // Helper function to avoid code duplication for posting
    private fun postNotification(
        manager: NotificationManagerCompat,
        builder: NotificationCompat.Builder,
        isPreTiramisu: Boolean = false
    ) {
        // notificationId needs to be unique for each notification you want to display simultaneously
        // Using timestamp is simple but can lead to collisions if called rapidly.
        // Consider a more robust ID generation if needed (e.g., based on content or a counter).
        val notificationId = System.currentTimeMillis().toInt()
        try {
            manager.notify(notificationId, builder.build())
            val versionInfo = if (isPreTiramisu) "(pre-Tiramisu)" else ""
            Log.d(TAG, "Notification posted $versionInfo (ID: $notificationId).")
        } catch (e: SecurityException) {
            // This can happen if permissions change unexpectedly, though less common with the checks above.
            Log.e(TAG, "SecurityException posting notification (ID: $notificationId). Check permissions.", e)
        } catch (e: Exception) {
            // Catch other potential exceptions during notification posting
            Log.e(TAG, "Exception posting notification (ID: $notificationId).", e)
        }
    }
}