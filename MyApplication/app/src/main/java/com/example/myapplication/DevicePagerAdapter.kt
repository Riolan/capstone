package com.example.myapplication

import android.os.Bundle
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.viewpager2.adapter.FragmentStateAdapter

class DevicePagerAdapter(
    fa: FragmentActivity,
    private val deviceList: List<String>
) : FragmentStateAdapter(fa) {
    private val fragmentMap = mutableMapOf<Int, DeviceFragment>()
    override fun getItemCount(): Int = deviceList.size

    override fun createFragment(position: Int): Fragment {
        val fragment = DeviceFragment()
        fragment.arguments = Bundle().apply {
            putString("device_name", deviceList[position])
        }
        fragmentMap[position] = fragment
        return fragment
    }
    fun getFragment(position: Int): DeviceFragment? = fragmentMap[position]
}