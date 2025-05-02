// Database Helper
package com.example.myapplication

import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import java.security.MessageDigest



class DatabaseHelper(context: Context) : SQLiteOpenHelper(context, DATABASE_NAME, null, DATABASE_VERSION) {

    companion object {
        private const val DATABASE_NAME = "users.db"
        private const val DATABASE_VERSION = 2

        // Existing user table constants
        private const val TABLE_USERS = "users"
        private const val COLUMN_ID = "id"
        private const val COLUMN_EMAIL = "email"
        private const val COLUMN_PASSWORD = "password"

        // New detections table constants
        private const val TABLE_DETECTIONS = "detections"
        private const val COLUMN_DETECTION_ID = "id"
        private const val COLUMN_DETECTED_ANIMAL = "detected_animal"
        private const val COLUMN_TIMESTAMP = "timestamp"
        private const val COLUMN_CAMERA = "camera"
        private const val COLUMN_IMAGE_PATH = "image_path"
    }

    override fun onCreate(db: SQLiteDatabase) {
        val createUserTable = "CREATE TABLE $TABLE_USERS ($COLUMN_ID INTEGER PRIMARY KEY AUTOINCREMENT, $COLUMN_EMAIL TEXT UNIQUE, $COLUMN_PASSWORD TEXT)"
        db.execSQL(createUserTable)

        // Create detections table
        val createDetectionsTable = "CREATE TABLE $TABLE_DETECTIONS (" +
                "$COLUMN_DETECTION_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                "$COLUMN_DETECTED_ANIMAL TEXT, " +
                "$COLUMN_TIMESTAMP TEXT, " +
                "$COLUMN_CAMERA TEXT, " +
                "$COLUMN_IMAGE_PATH TEXT)"
        db.execSQL(createDetectionsTable)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if (oldVersion < 2) {
            val createDetectionsTable = "CREATE TABLE $TABLE_DETECTIONS (" +
                    "$COLUMN_DETECTION_ID INTEGER PRIMARY KEY AUTOINCREMENT, " +
                    "$COLUMN_DETECTED_ANIMAL TEXT, " +
                    "$COLUMN_TIMESTAMP TEXT, " +
                    "$COLUMN_CAMERA TEXT, " +
                    "$COLUMN_IMAGE_PATH TEXT)"
            db.execSQL(createDetectionsTable)
        }
    }

    // Hash function using SHA-256 for passwords
    private fun hashPassword(password: String): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val hashBytes = digest.digest(password.toByteArray())
        return hashBytes.joinToString("") { "%02x".format(it) }
    }

    // Register a new user (stores hashed password)
    fun registerUser(email: String, password: String): Boolean {
        val db = writableDatabase
        val hashedPassword = hashPassword(password)
        val values = ContentValues().apply {
            put(COLUMN_EMAIL, email)
            put(COLUMN_PASSWORD, hashedPassword)
        }
        val result = db.insert(TABLE_USERS, null, values)
        db.close()
        return result != -1L
    }

    // Validate user login (compares hashed password)
    fun isValidUser(email: String, password: String): Boolean {
        val db = readableDatabase
        val hashedPassword = hashPassword(password)
        val query = "SELECT * FROM $TABLE_USERS WHERE $COLUMN_EMAIL = ? AND $COLUMN_PASSWORD = ?"
        val cursor = db.rawQuery(query, arrayOf(email, hashedPassword))
        val isValid = cursor.count > 0
        cursor.close()
        db.close()
        return isValid
    }

    // Insert a detection record into the detections table.
    // The record contains the detected animal, timestamp, camera, and image path.
    fun insertDetectionRecord(animal: String, timestamp: String, camera: String, imagePath: String): Boolean {
        val db = writableDatabase
        val values = ContentValues().apply {
            put(COLUMN_DETECTED_ANIMAL, animal)
            put(COLUMN_TIMESTAMP, timestamp)
            put(COLUMN_CAMERA, camera)
            put(COLUMN_IMAGE_PATH, imagePath)
        }
        val result = db.insert(TABLE_DETECTIONS, null, values)
        db.close()
        return result != -1L
    }

    fun isValidUserHashed(email: String, passwordHash: String): Boolean {
        val db = readableDatabase
        val query = "SELECT * FROM users WHERE email = ? AND password = ?"
        val cursor = db.rawQuery(query, arrayOf(email, passwordHash))
        val isValid = cursor.count > 0
        cursor.close()
        db.close()
        return isValid
    }

    fun getHashedPassword(email: String): String? {
        val db = readableDatabase
        val cursor = db.rawQuery("SELECT password FROM users WHERE email = ?", arrayOf(email))
        val result = if (cursor.moveToFirst()) cursor.getString(0) else null
        cursor.close()
        db.close()
        return result
    }

    fun getAllDetections(): Cursor {
        val db = readableDatabase
        return db.rawQuery("SELECT * FROM detections ORDER BY timestamp DESC", null)
    }

    // Test Data
    fun populateTestData() {
        val testData = listOf(
            Triple("Cat", "2024-04-01 10:30:00", "Front Camera"),
            Triple("Dog", "2024-04-03 15:45:00", "Back Camera"),
            Triple("Squirrel", "2024-04-10 08:12:00", "Tree Cam"),
            Triple("Bird", "2024-04-14 09:00:00", "Bird Feeder"),
            Triple("Cat", "2024-04-16 12:00:00", "Front Camera")
        )

        val db = writableDatabase
        for ((animal, timestamp, camera) in testData) {
            val values = ContentValues().apply {
                put(COLUMN_DETECTED_ANIMAL, animal)
                put(COLUMN_TIMESTAMP, timestamp)
                put(COLUMN_CAMERA, camera)
                put(COLUMN_IMAGE_PATH, "test_path_${animal.lowercase()}.png")
            }
            db.insert(TABLE_DETECTIONS, null, values)
        }
        db.close()
    }
}