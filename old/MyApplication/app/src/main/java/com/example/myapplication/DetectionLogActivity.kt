package com.example.myapplication

import android.os.Bundle
import android.widget.ArrayAdapter
import android.widget.ListView
import android.widget.SearchView
import androidx.appcompat.app.AppCompatActivity

class DetectionLogActivity : AppCompatActivity() {

    private lateinit var dbHelper: DatabaseHelper
    private lateinit var listView: ListView
    private lateinit var searchView: SearchView
    private lateinit var adapter: ArrayAdapter<String>
    private val allItems = mutableListOf<String>() // Full unfiltered list

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_detection_log)

        dbHelper = DatabaseHelper(this)
        listView = findViewById(R.id.detectionLogListView)
        searchView = findViewById(R.id.searchView)

        val cursor = dbHelper.getAllDetections()
        while (cursor.moveToNext()) {
            val animal = cursor.getString(cursor.getColumnIndexOrThrow("detected_animal"))
            val time = cursor.getString(cursor.getColumnIndexOrThrow("timestamp"))
            val camera = cursor.getString(cursor.getColumnIndexOrThrow("camera"))
            allItems.add("$time - $animal @ $camera")
        }
        cursor.close()

        adapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, ArrayList(allItems))
        listView.adapter = adapter

        // 🔍 Handle live search filtering
        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query: String?): Boolean = false

            override fun onQueryTextChange(newText: String?): Boolean {
                val filtered = if (newText.isNullOrEmpty()) {
                    allItems
                } else {
                    allItems.filter {
                        it.contains(newText, ignoreCase = true)
                    }
                }
                adapter.clear()
                adapter.addAll(filtered)
                adapter.notifyDataSetChanged()
                return true
            }
        })
    }
}