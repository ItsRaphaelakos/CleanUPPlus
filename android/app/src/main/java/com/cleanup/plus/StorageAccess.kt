package com.cleanup.plus

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.StatFs
import android.provider.Settings
import androidx.core.content.ContextCompat
import java.io.File
import java.text.DateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.max

data class DeviceStorageInfo(
    val totalBytes: Long,
    val availableBytes: Long,
) {
    val usedBytes: Long = max(0L, totalBytes - availableBytes)
    val usedFraction: Float = if (totalBytes <= 0L) 0f else usedBytes.toFloat() / totalBytes.toFloat()
}

class CleanerSettingsStore(context: Context) {
    private val prefs = context.getSharedPreferences("cleanup_plus_settings", Context.MODE_PRIVATE)

    fun load(): CleanerSettings {
        val themeName = prefs.getString("themeMode", AppThemeMode.Dark.name) ?: AppThemeMode.Dark.name
        return CleanerSettings(
            largeFileThresholdBytes = prefs.getLong("largeFileThresholdBytes", 100L * 1024L * 1024L),
            includeScreenshots = prefs.getBoolean("includeScreenshots", true),
            includeDownloads = prefs.getBoolean("includeDownloads", true),
            themeMode = runCatching { AppThemeMode.valueOf(themeName) }.getOrDefault(AppThemeMode.Dark),
        )
    }

    fun save(settings: CleanerSettings) {
        prefs.edit()
            .putLong("largeFileThresholdBytes", settings.largeFileThresholdBytes)
            .putBoolean("includeScreenshots", settings.includeScreenshots)
            .putBoolean("includeDownloads", settings.includeDownloads)
            .putString("themeMode", settings.themeMode.name)
            .apply()
    }
}

fun readDeviceStorageInfo(): DeviceStorageInfo {
    val root = Environment.getExternalStorageDirectory()
    val statFs = StatFs(root.absolutePath)
    return DeviceStorageInfo(
        totalBytes = statFs.totalBytes,
        availableBytes = statFs.availableBytes,
    )
}

fun defaultScanRoots(): List<String> {
    val root = Environment.getExternalStorageDirectory()
    return listOf(root.absolutePath).filter { File(it).exists() }
}

fun Context.hasBroadStorageAccess(): Boolean {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        Environment.isExternalStorageManager()
    } else {
        ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.READ_EXTERNAL_STORAGE,
        ) == PackageManager.PERMISSION_GRANTED
    }
}

fun mediaReadPermissionsForCurrentSdk(): Array<String> {
    return when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU -> arrayOf(
            Manifest.permission.READ_MEDIA_IMAGES,
            Manifest.permission.READ_MEDIA_VIDEO,
            Manifest.permission.READ_MEDIA_AUDIO,
        )

        else -> arrayOf(Manifest.permission.READ_EXTERNAL_STORAGE)
    }
}

fun Activity.openBroadStorageSettings() {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        val intent = Intent(
            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
            Uri.parse("package:$packageName"),
        )
        runCatching { startActivity(intent) }
            .recover {
                startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
            }
    }
}

fun formatBytes(bytes: Long): String {
    if (bytes <= 0L) return "0 B"
    val units = arrayOf("B", "KB", "MB", "GB", "TB")
    var value = bytes.toDouble()
    var unitIndex = 0
    while (value >= 1024.0 && unitIndex < units.lastIndex) {
        value /= 1024.0
        unitIndex += 1
    }
    return if (value >= 10 || unitIndex == 0) {
        String.format(Locale.US, "%.0f %s", value, units[unitIndex])
    } else {
        String.format(Locale.US, "%.1f %s", value, units[unitIndex])
    }
}

fun formatDate(millis: Long): String {
    if (millis <= 0L) return "Unknown"
    return DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT).format(Date(millis))
}

fun thresholdLabel(bytes: Long): String = formatBytes(bytes)
