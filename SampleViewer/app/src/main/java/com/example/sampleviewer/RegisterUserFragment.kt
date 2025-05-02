package com.example.sampleviewer // Or your appropriate package

// Import necessary classes
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button // Or MaterialButton if used in layout
import android.widget.EditText
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.example.sampleviewer.database.DatabaseHelper
import com.example.myapplication.R

class RegisterUserFragment : Fragment() {

    private lateinit var emailField: EditText
    private lateinit var passwordField: EditText
    private lateinit var registerButton: Button
    private lateinit var backToLoginButton: Button

    private lateinit var dbHelper: DatabaseHelper

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        dbHelper = DatabaseHelper(requireContext())
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_register_user, container, false)

        emailField = view.findViewById(R.id.editTextEmail)
        passwordField = view.findViewById(R.id.editTextPassword)
        registerButton = view.findViewById(R.id.registerButton)
        backToLoginButton = view.findViewById(R.id.backToLoginButton)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupListeners()
    }

    private fun setupListeners() {
        registerButton.setOnClickListener {
            handleRegistration()
        }

        backToLoginButton.setOnClickListener {
            // Navigate back to the Login fragment by popping the back stack
            activity?.supportFragmentManager?.popBackStack()
        }
    }

    private fun handleRegistration() {
        val email = emailField.text.toString().trim()
        val password = passwordField.text.toString().trim()

        if (email.isEmpty() || password.isEmpty()) {
            Toast.makeText(requireContext(), "Please enter email and password", Toast.LENGTH_SHORT).show()
            return
        }

        if (dbHelper.registerUser(email, password)) {
            Toast.makeText(requireContext(), "Registration Successful! Please login.", Toast.LENGTH_LONG).show()
            // Navigate back to Login fragment
            activity?.supportFragmentManager?.popBackStack()
        } else {
            Toast.makeText(requireContext(), "Registration failed (User might already exist).", Toast.LENGTH_SHORT).show()
        }
    }
}

