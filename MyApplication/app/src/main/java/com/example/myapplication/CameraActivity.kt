
package com.example.myapplication
import android.content.SharedPreferences
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.ImageButton
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.viewpager.widget.ViewPager
import androidx.viewpager2.adapter.FragmentStateAdapter
import androidx.viewpager2.widget.ViewPager2
import com.example.myapplication.databinding.ActivityCameraSettingsBinding
import com.example.myapplication.ui.main.SectionsPagerAdapter
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator

class MyPagerAdapter(fragmentActivity: FragmentActivity, private val items: List<String>) : FragmentStateAdapter(fragmentActivity) {
    override fun getItemCount(): Int = items.size

    override fun createFragment(position: Int): Fragment {
        val fragment = MyFragment()
        fragment.arguments = Bundle().apply {
            putString("data", items[position])
        }
        return fragment
    }
}


class MyFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        val view = inflater.inflate(R.layout.fragment_camera_activity2, container, false)
        val data = arguments?.getString("data")
        view.findViewById<TextView>(R.id.section_label).text = data
        return view
    }
}


class CameraActivity : AppCompatActivity() {

    private lateinit var binding: ActivityCameraSettingsBinding
    private lateinit var CameraText: TextView
    private lateinit var tabLayout: TabLayout
    private lateinit var viewPager: ViewPager2
    private lateinit var adapter: DevicePagerAdapter
    private var deviceList: MutableList<String> = mutableListOf()
    private lateinit var prefs: SharedPreferences
    private lateinit var originalList: ArrayList<String>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_camera_settings)

        val resetButton = findViewById<Button>(R.id.resetAnimalsButton)
        resetButton.setOnClickListener {
            val prefs = getSharedPreferences("SwitchPrefs", MODE_PRIVATE)
            prefs.edit().clear().apply()

            // Reset switches in all loaded fragments
            for (i in 0 until adapter.itemCount) {
                val fragment = adapter.getFragment(i)
                fragment?.resetSwitchStates()
            }
        }

        val backButton = findViewById<Button>(R.id.backButton)
        backButton.setOnClickListener {
            finish() // Returns to MainActivity
        }

        tabLayout = findViewById(R.id.tabLayout)
        viewPager = findViewById(R.id.viewPager)

       // val deviceList = intent.getStringArrayListExtra("device_list")?.toMutableList() ?: mutableListOf()
        prefs = getSharedPreferences("DevicePrefs", MODE_PRIVATE)

        originalList = intent.getStringArrayListExtra("device_list") ?: arrayListOf()

        deviceList = originalList.mapIndexed { index, name ->
            prefs.getString("device_$index", name) ?: name
        }.toMutableList()
        deviceList.add("Test")
        adapter = DevicePagerAdapter(this, deviceList)
        viewPager.adapter = adapter


        TabLayoutMediator(tabLayout, viewPager) { tab, position ->
            tab.customView = createCustomTabView(position)
        }.attach()
    }
    private fun createCustomTabView(position: Int): View {
        val view = layoutInflater.inflate(R.layout.custom_tab, null)
        val title = view.findViewById<TextView>(R.id.tabTitle)
        val button = view.findViewById<ImageButton>(R.id.renameButton)

        title.text = deviceList[position]

        button.setOnClickListener {
            showRenameDialog(position)
        }

        return view
    }

    private fun showRenameDialog(position: Int) {
        val currentName = deviceList[position]
        val editText = EditText(this).apply {
            setText(currentName)
        }

        AlertDialog.Builder(this)
            .setTitle("Rename Device")
            .setView(editText)
            .setPositiveButton("Rename") { _, _ ->
                val newName = editText.text.toString().trim()
                if (newName.isNotEmpty()) {
                    deviceList[position] = newName

                    // Update SharedPreferences
                    prefs.edit().putString("device_$position", newName).apply()

                    // Update tab title
                    val tab = tabLayout.getTabAt(position)
                    val customView = tab?.customView
                    val title = customView?.findViewById<TextView>(R.id.tabTitle)
                    title?.text = newName

                    // Optionally: notify fragment to refresh content
                    adapter.notifyItemChanged(position)
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }


}


