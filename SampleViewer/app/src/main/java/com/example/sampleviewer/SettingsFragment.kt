package com.example.sampleviewer // Your package name

import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button // Or MaterialButton
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
// Import your R file if necessary, e.g., com.example.myapplication.R
import com.example.myapplication.R
import com.google.android.material.button.MaterialButton // If using MaterialButton
import com.google.android.material.textfield.TextInputLayout

class SettingsFragment : Fragment() {

    // Views for existing settings
    private lateinit var deviceIdLabel: TextView
    private lateinit var batteryLevelLabel: TextView
    private lateinit var signalStrengthLabel: TextView
    private lateinit var sensitivitySeekBar: SeekBar
    private lateinit var qualitySpinner: Spinner
    private lateinit var intervalInputLayout: TextInputLayout
    private lateinit var saveSettingsButton: MaterialButton

    // New button for Dev Tools
    private lateinit var openDevToolsButton: MaterialButton

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        // Inflate the layout for this fragment
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Find existing views
        deviceIdLabel = view.findViewById(R.id.device_id_label)
        batteryLevelLabel = view.findViewById(R.id.battery_level_label)
        signalStrengthLabel = view.findViewById(R.id.signal_strength_label)
        sensitivitySeekBar = view.findViewById(R.id.seekbar_sensitivity)
        qualitySpinner = view.findViewById(R.id.spinner_quality)
        intervalInputLayout = view.findViewById(R.id.text_input_interval)
        saveSettingsButton = view.findViewById(R.id.button_save_settings)

        // Find the new Dev Tools button
        openDevToolsButton = view.findViewById(R.id.button_open_dev_tools)


        setupButtonClickListeners()
    }

    private fun setupButtonClickListeners() {
        saveSettingsButton.setOnClickListener {
            // TODO: Implement logic to save settings
            Toast.makeText(context, "Settings saving (not implemented)", Toast.LENGTH_SHORT).show()
        }
    }

    // TODO: Add functions to load/save settings values
}