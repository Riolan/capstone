// ========================================================================
// File Path: app/build.gradle.kts (or app/build.gradle for Groovy)
// Build script reflecting user-provided configuration.
// NOTE: This assumes you have a libs.versions.toml file defining the aliases used.
// ========================================================================

plugins {
    alias(libs.plugins.android.application) // Using alias syntax from user input
    alias(libs.plugins.kotlin.android)      // Using alias syntax from user input
    alias(libs.plugins.kotlin.compose)      // Using alias syntax from user input (even if Compose UI isn't built yet)
//    id("com.android.application")
    id("com.google.gms.google-services")
}

android {
    namespace = "com.example.myapplication"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.example.myapplication"
        minSdk = 23
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // Vector drawable support (kept from previous version, generally useful)
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        // Removed debug block for brevity, defaults are usually fine
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11 // Changed from user input
        targetCompatibility = JavaVersion.VERSION_11 // Changed from user input
    }
    kotlinOptions {
        jvmTarget = "11" // Changed from user input
    }
    buildFeatures {
        compose = true // Enabled from user input
        viewBinding = true // Kept enabled as fragments use it
    }
    // Removed packaging options block for brevity, add back if needed
}

dependencies {

    // Core & Lifecycle (using alias syntax from user input)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose) // Included from user input

    // Compose (using alias syntax from user input - needed if compose = true in buildFeatures)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3) // Compose Material 3

    // AppCompat & Material (for XML layouts & components)
    implementation(libs.androidx.appcompat)
    implementation(libs.material) // Non-Compose Material Components

    // UI Components (for XML layouts)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.viewpager2)
    implementation(libs.androidx.constraintlayout)
    implementation("androidx.coordinatorlayout:coordinatorlayout:1.2.0") // Explicitly added for CoordinatorLayout in fragment_home

    // Activity & ViewModel/LiveData KTX (using alias syntax from user input)
    implementation(libs.androidx.activity) // Base activity library
    implementation(libs.androidx.lifecycle.livedata.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.ktx)

    // Core & Lifecycle (using alias syntax from user input)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose) // Included from user input

    // Compose (using alias syntax from user input - needed if compose = true in buildFeatures)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3) // Compose Material 3

    // AppCompat & Material (for XML layouts & components)
    implementation(libs.androidx.appcompat)
    implementation(libs.material) // Non-Compose Material Components

    // UI Components (for XML layouts)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.viewpager2)
    implementation(libs.androidx.constraintlayout)
    implementation("androidx.coordinatorlayout:coordinatorlayout:1.2.0") // Explicitly added for CoordinatorLayout in fragment_home

    // Activity & ViewModel/LiveData KTX (using alias syntax from user input)
    implementation(libs.androidx.activity) // Base activity library
    implementation(libs.androidx.lifecycle.livedata.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.ktx)

    // Firebase (using alias syntax & platform from user input)
    implementation(platform("com.google.firebase:firebase-bom:33.9.0"))
    implementation("com.google.firebase:firebase-analytics") // Added from user input
    implementation(libs.firebase.auth.ktx) // Added from user input


    // Testing (using alias syntax from user input)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)
}
