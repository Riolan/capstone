package com.example.myapplication

import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class BypassActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Simulate unauthorized login by modifying SharedPreferences
        val sharedPreferences: SharedPreferences = getSharedPreferences("AppPrefs", MODE_PRIVATE)
        val editor = sharedPreferences.edit()
        editor.putBoolean("isLoggedIn", true) // ✅ Faking login
        editor.apply()

        Toast.makeText(this, "🚨 Bypass Attack Executed: Logged in without credentials!", Toast.LENGTH_LONG).show()

        // Redirect to MainActivity
        startActivity(Intent(this, MainActivity::class.java))
        finish()
    }
}