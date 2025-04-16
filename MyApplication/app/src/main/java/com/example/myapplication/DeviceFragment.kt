package com.example.myapplication

import android.content.Context
import android.content.SharedPreferences
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import androidx.fragment.app.Fragment

class DeviceFragment : Fragment() {
    private var deviceName: String? = null
    private var switchStates: MutableMap<String, Boolean> = mutableMapOf(
        "cat" to false,
        "dog" to false,
        "bird" to false,
        "squirrel" to false
    )
    private lateinit var prefs: SharedPreferences


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        deviceName = arguments?.getString("device_name")
        prefs = requireContext().getSharedPreferences("SwitchPrefs", Context.MODE_PRIVATE)
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_device, container, false)

        view.findViewById<TextView>(R.id.deviceNameTextView).text = deviceName
        val animalKeys = listOf("cat", "dog", "bird", "squirrel")
        // Link switches
        val switches = mapOf(
            "cat" to view.findViewById<Switch>(R.id.switchCat),
            "dog" to view.findViewById<Switch>(R.id.switchDog),
            "bird" to view.findViewById<Switch>(R.id.switchBird),
            "squirrel" to view.findViewById<Switch>(R.id.switchSquirrel)
        )

        for (animal in animalKeys) {
            val switch = switches[animal]!!
            val prefKey = "${deviceName}_${animal}"
            val savedState = prefs.getBoolean(prefKey, false)

            switch.isChecked = savedState
            switch.setOnCheckedChangeListener { _, isChecked ->
                prefs.edit().putBoolean(prefKey, isChecked).apply()
                switchStates[animal] = isChecked
                Log.d("SwitchState", "$prefKey set to $isChecked")
            }

            // Also update local cache
            switchStates[animal] = savedState
        }
        return view
    }

    fun getSwitchStates(): Map<String, Boolean> = switchStates.toMap()

    fun resetSwitchStates() {
        val switches = mapOf(
            "cat" to view?.findViewById<Switch>(R.id.switchCat),
            "dog" to view?.findViewById<Switch>(R.id.switchDog),
            "bird" to view?.findViewById<Switch>(R.id.switchBird),
            "squirrel" to view?.findViewById<Switch>(R.id.switchSquirrel)
        )

        for ((animal, switch) in switches) {
            val key = "${deviceName}_$animal"
            prefs.edit().putBoolean(key, false).apply()
            switch?.isChecked = false
            switchStates[animal] = false
        }
    }
}