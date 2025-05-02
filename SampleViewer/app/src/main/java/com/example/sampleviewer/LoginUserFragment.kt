package com.example.sampleviewer // Or your appropriate package


import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.example.sampleviewer.database.DatabaseHelper // Corrected package name
import com.example.myapplication.R // Corrected package name

class LoginUserFragment : Fragment() {

    private lateinit var emailField: EditText
    private lateinit var passwordField: EditText
    private lateinit var loginButton: Button
    private lateinit var signUpButton: Button

    private lateinit var dbHelper: DatabaseHelper
    private lateinit var sharedPreferences: SharedPreferences

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        dbHelper = DatabaseHelper(requireContext())
        sharedPreferences = requireActivity().getSharedPreferences("AppPrefs", Context.MODE_PRIVATE)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_login_user, container, false)

        emailField = view.findViewById(R.id.editTextEmail)
        passwordField = view.findViewById(R.id.editTextPassword)
        loginButton = view.findViewById(R.id.loginButton)
        signUpButton = view.findViewById(R.id.signupButton)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupListeners()
        // Note: The check for already logged in is now handled by AuthActivity
    }

    private fun setupListeners() {
        loginButton.setOnClickListener {
            handleLogin()
        }

        signUpButton.setOnClickListener {
            // Navigate to Register Fragment within the same AuthActivity
            (activity as? AuthActivity)?.loadFragment(RegisterUserFragment())
        }
    }

    private fun handleLogin() {
        val email = emailField.text.toString().trim()
        val password = passwordField.text.toString().trim()

        if (email.isEmpty() || password.isEmpty()) {
            Toast.makeText(requireContext(), "Please enter email and password", Toast.LENGTH_SHORT).show()
            return
        }

        if (dbHelper.isValidUser(email, password)) {
            sharedPreferences.edit().putBoolean("isLoggedIn", true).apply()
                //.putBoolean("isLoggedIn", true)
//                .putString("email", email)
//                .putString("password", dbHelper.getHashedPassword(email)) // Ensure this returns SHA-256 hash
//                .apply()
            Toast.makeText(requireContext(), "Login Successful!", Toast.LENGTH_SHORT).show()
            // Tell AuthActivity to navigate to Main
            (activity as? AuthActivity)?.navigateToMain()
        } else {
            Toast.makeText(requireContext(), "Invalid email or password", Toast.LENGTH_SHORT).show()
        }
    }
    // navigateToMain is now handled by AuthActivity
}