package com.example.sampleviewer

import androidx.annotation.DrawableRes

data class Event(
    val nodeId: String,

    val id: String,
    val description: String,
    val timestamp: String, // Ideally pre-formatted for display
    val imagePath: String?, // Nullable: Holds the path to the actual image file if available
    @DrawableRes val fallbackIconResId: Int, // Drawable resource ID to use if imagePath is null/invalid or during loading

    val deviceId: String,
    val bboxX: Int? = null,
    val bboxY: Int? = null,
    val bboxW: Int? = null,
    val bboxH: Int? = null
)