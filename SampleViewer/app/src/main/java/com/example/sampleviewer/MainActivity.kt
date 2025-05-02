package com.example.sampleviewer

import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.example.myapplication.R
import com.google.android.material.bottomnavigation.BottomNavigationView
import android.Manifest
import androidx.core.app.NotificationManagerCompat


class MainActivity : AppCompatActivity() {

    private val homeFragment = HomeFragment()
    private val nodesFragment = NodesFragment()
    private val settingsFragment = SettingsFragment()
    private val devToolsFragment = DevToolsFragment()
    private val imageGalleryFragment = ImageGalleryFragment()

    private val NOTIFICATION_PERMISSION_REQUEST_CODE = 1001

    companion object {
        // This flag is now primarily set by HomeFragment's connect button
        var isMainConnected: Boolean = false
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Create the notification channel ONCE when the app starts
        NotificationHelper.createNotificationChannel(this)
        checkAndRequestNotificationPermission()

        val bottomNavigation = findViewById<BottomNavigationView>(R.id.bottom_navigation)

        if (savedInstanceState == null) {
            loadFragment(homeFragment)
            // Default selection is handled by the menu item ID if needed,
            // or set explicitly if required after loading the first fragment.
            // bottomNavigation.selectedItemId = R.id.navigation_home
        }

        bottomNavigation.setOnItemSelectedListener { item ->
            when (item.itemId) {
                R.id.navigation_home -> {
                    loadFragment(homeFragment)
                    true
                }
                R.id.navigation_nodes -> {
                    loadFragment(nodesFragment)
                    true
                }
                //R.id.navigation_settings -> {
                //    loadFragment(settingsFragment)
                //    true
                //}
                R.id.navigation_dev_tools -> {
                    loadFragment(devToolsFragment)
                    true
                }
                R.id.navigation_image_library -> {
                loadFragment(imageGalleryFragment)
                true
                 }
                else -> false
            }
        }
        // Ensure the correct fragment is selected if the activity is recreated
        // Example: Select the home item if no specific item is selected
        if (bottomNavigation.selectedItemId == 0 && savedInstanceState != null) {
            bottomNavigation.selectedItemId = R.id.navigation_home
        } else if (savedInstanceState == null) {
            bottomNavigation.selectedItemId = R.id.navigation_home // Ensure initial selection
        }
    }




    private fun loadFragment(fragment: Fragment) {
        // Check if the fragment is already added to prevent overlap/multiple instances
        if (!fragment.isAdded) {
            supportFragmentManager.beginTransaction()
                .replace(R.id.fragment_container, fragment)
                // Optional: Add transition animations
                // .setTransition(FragmentTransaction.TRANSIT_FRAGMENT_FADE)
                .commit()
        }
    }







    private fun checkAndRequestNotificationPermission() {
        // Only necessary for Android 13 (API 33) and above
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) { // TIRAMISU is API 33
            when {
                ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.POST_NOTIFICATIONS
                ) == PackageManager.PERMISSION_GRANTED -> {
                    // Permission is already granted, you can show notifications
                    // Log.d("PermissionCheck", "POST_NOTIFICATIONS permission granted.")
                    // proceedToShowNotification() // Your function to show notification
                }
                shouldShowRequestPermissionRationale(Manifest.permission.POST_NOTIFICATIONS) -> {
                    // Explain to the user why you need the permission
                    // Show a dialog or SnackBar explaining the need for notifications
                    // After explaining, you can request again or guide them to settings
                    // Log.d("PermissionCheck", "Showing rationale for POST_NOTIFICATIONS.")
                    requestNotificationPermission() // Or show UI explaining why
                }
                else -> {
                    // Permission has not been granted yet, request it directly
                    // Log.d("PermissionCheck", "Requesting POST_NOTIFICATIONS permission.")
                    requestNotificationPermission()
                }
            }
        } else {
            // No runtime permission needed for versions below Android 13
            // Log.d("PermissionCheck", "No runtime notification permission needed for this API level.")
            // proceedToShowNotification() // Your function to show notification
        }
    }


    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.POST_NOTIFICATIONS),
                NOTIFICATION_PERMISSION_REQUEST_CODE
            )
        }
    }

    // Handle the result of the permission request
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        when (requestCode) {
            NOTIFICATION_PERMISSION_REQUEST_CODE -> {
                if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    // Permission was granted by the user
                    // Log.d("PermissionResult", "POST_NOTIFICATIONS permission granted by user.")
                    // proceedToShowNotification() // Your function to show notification
                } else {
                    // Permission was denied by the user
                    // Log.d("PermissionResult", "POST_NOTIFICATIONS permission denied by user.")
                    // Handle the denial gracefully (e.g., explain that notifications won't work)
                    // Maybe show a message indicating features are limited without the permission.
                }
                return
            }
            // Handle other permission requests if you have them
        }
    }
}

