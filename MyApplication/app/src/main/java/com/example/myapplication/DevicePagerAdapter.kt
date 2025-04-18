package com.example.myapplication

import android.os.Bundle
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.viewpager2.adapter.FragmentStateAdapter
class DevicePagerAdapter(fragmentActivity: FragmentActivity, private var nodes: List<Node>) : FragmentStateAdapter(fragmentActivity) {
    private val fragmentMap = mutableMapOf<Int, DeviceFragment>()

    override fun getItemCount(): Int = nodes.size

    override fun createFragment(position: Int): Fragment {
        val fragment = MyFragment()
        fragment.arguments = Bundle().apply {
            putString("data", nodes[position].name)
        }
        return fragment
    }

    fun getFragment(position: Int): DeviceFragment? = fragmentMap[position]

    fun updateDevices(newNodes: List<Node>) {
        this.nodes = newNodes
        notifyDataSetChanged()
    }
}
