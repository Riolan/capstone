package com.example.sampleviewer


import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.example.myapplication.R

class EventAdapter(private val onItemClicked: (Event) -> Unit) :
    ListAdapter<Event, EventAdapter.EventViewHolder>(EventDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): EventViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.list_item_event, parent, false)
        return EventViewHolder(view, onItemClicked)
    }

    override fun onBindViewHolder(holder: EventViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    class EventViewHolder(
        itemView: View,
        private val onItemClicked: (Event) -> Unit
    ) : RecyclerView.ViewHolder(itemView) {

        private val descriptionTextView: TextView = itemView.findViewById(R.id.event_description)
        private val timestampTextView: TextView = itemView.findViewById(R.id.event_timestamp)
        private val thumbnailImageView: ImageView = itemView.findViewById(R.id.event_thumbnail)

        private var currentEvent: Event? = null

        init {
            itemView.setOnClickListener {
                currentEvent?.let { onItemClicked(it) }
            }
        }

        fun bind(event: Event) {
            currentEvent = event
            descriptionTextView.text = event.description
            timestampTextView.text = event.timestamp
            // Set the image resource using the ID (which is now an @android:drawable ID)
            thumbnailImageView.setImageResource(event.
            fallbackIconResId)
        }
    }

    class EventDiffCallback : DiffUtil.ItemCallback<Event>() {
        override fun areItemsTheSame(oldItem: Event, newItem: Event): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: Event, newItem: Event): Boolean {
            return oldItem == newItem
        }
    }
}
