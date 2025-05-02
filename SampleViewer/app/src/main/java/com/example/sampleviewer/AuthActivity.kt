package com.example.sampleviewer


import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import com.example.myapplication.R
import com.example.sampleviewer.database.DatabaseHelper

class AuthActivity : AppCompatActivity() {

    private lateinit var sharedPreferences: SharedPreferences

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        sharedPreferences = getSharedPreferences("AppPrefs", Context.MODE_PRIVATE)

        // Check if user is already logged in
        if (sharedPreferences.getBoolean("isLoggedIn", false)) {
            navigateToMain()
            return // Finish AuthActivity, don't load login fragment
        }

//        val email = sharedPreferences.getString("email", null)
//        val passwordHash = sharedPreferences.getString("password", null)
//        val dbHelper = DatabaseHelper(this)
//
//        if (email != null && passwordHash != null && dbHelper.isValidUserHashed(email, passwordHash)) {
//            navigateToMain()
//            return
//        }

        // Set the content view for the AuthActivity
        setContentView(R.layout.activity_auth)

        // Load the Login fragment initially if not logged in
        if (savedInstanceState == null) {
            loadFragment(LoginUserFragment(), false) // Don't add initial fragment to back stack
        }
    }


    // Function to load fragments into the container
    fun loadFragment(fragment: Fragment, addToBackStack: Boolean = true) {
        val transaction = supportFragmentManager.beginTransaction()
            .replace(R.id.auth_fragment_container, fragment)

        if (addToBackStack) {
            transaction.addToBackStack(null) // Add to back stack so user can go back
        }

        transaction.commit()
    }

    // Function to navigate to MainActivity after successful login/registration
    fun navigateToMain() {
        val intent = Intent(this, MainActivity::class.java)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        startActivity(intent)
        finish() // Finish AuthActivity
    }
}