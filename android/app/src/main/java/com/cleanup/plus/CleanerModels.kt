package com.cleanup.plus

import org.json.JSONArray
import org.json.JSONObject

enum class CleanerCategory(val key: String, val label: String) {
    DuplicatePhotos("duplicate_photos", "Duplicate Photos"),
    LargeFiles("large_files", "Large Files"),
    Screenshots("screenshots", "Screenshots"),
    Downloads("downloads", "Downloads"),
    EmptyFolders("empty_folders", "Empty Folders");

    companion object {
        fun fromKey(key: String): CleanerCategory? = entries.firstOrNull { it.key == key }
    }
}

enum class AppThemeMode(val label: String) {
    Dark("Dark"),
    Light("Light"),
    System("System"),
}

data class CleanerSettings(
    val largeFileThresholdBytes: Long = 100L * 1024L * 1024L,
    val includeScreenshots: Boolean = true,
    val includeDownloads: Boolean = true,
    val themeMode: AppThemeMode = AppThemeMode.Dark,
)

data class CleanerItem(
    val id: String,
    val path: String,
    val name: String,
    val sizeBytes: Long,
    val modifiedMillis: Long,
    val isDirectory: Boolean,
    val isImage: Boolean,
    val hash: String,
    val duplicateGroupId: String,
    val recommendedForKeepNewest: Boolean,
    val categories: Set<CleanerCategory>,
)

data class DuplicateGroup(
    val id: String,
    val keepPath: String,
    val reclaimableBytes: Long,
    val paths: List<String>,
)

data class ScanSummary(
    val totalSpaceBytes: Long = 0L,
    val availableSpaceBytes: Long = 0L,
    val scannedBytes: Long = 0L,
    val reclaimableBytes: Long = 0L,
    val duplicateBytes: Long = 0L,
    val largeFileBytes: Long = 0L,
    val screenshotBytes: Long = 0L,
    val downloadBytes: Long = 0L,
    val filesScanned: Long = 0L,
    val foldersScanned: Long = 0L,
    val duplicateGroups: Long = 0L,
    val emptyFolders: Long = 0L,
)

data class ScanPayload(
    val summary: ScanSummary = ScanSummary(),
    val items: List<CleanerItem> = emptyList(),
    val duplicateGroups: List<DuplicateGroup> = emptyList(),
    val errors: List<String> = emptyList(),
)

data class DeleteFailure(val path: String, val reason: String)

data class DeletePayload(
    val deletedBytes: Long,
    val deletedPaths: List<String>,
    val failures: List<DeleteFailure>,
)

object NativeCleanerBridge {
    init {
        System.loadLibrary("cleanupplus")
    }

    private external fun scanStorage(
        rootPaths: Array<String>,
        largeFileThresholdBytes: Long,
        includeScreenshots: Boolean,
        includeDownloads: Boolean,
    ): String

    private external fun deletePaths(paths: Array<String>, confirmed: Boolean): String

    fun scan(rootPaths: List<String>, settings: CleanerSettings): ScanPayload {
        val json = scanStorage(
            rootPaths.toTypedArray(),
            settings.largeFileThresholdBytes,
            settings.includeScreenshots,
            settings.includeDownloads,
        )
        return parseScanPayload(json)
    }

    fun deleteSelected(paths: List<String>, confirmed: Boolean): DeletePayload {
        return parseDeletePayload(deletePaths(paths.toTypedArray(), confirmed))
    }
}

fun parseScanPayload(json: String): ScanPayload {
    val root = JSONObject(json)
    val summaryJson = root.optJSONObject("summary") ?: JSONObject()
    val itemsJson = root.optJSONArray("items") ?: JSONArray()
    val duplicateGroupsJson = root.optJSONArray("duplicateGroups") ?: JSONArray()
    val errorsJson = root.optJSONArray("errors") ?: JSONArray()

    val summary = ScanSummary(
        totalSpaceBytes = summaryJson.optLong("totalSpaceBytes"),
        availableSpaceBytes = summaryJson.optLong("availableSpaceBytes"),
        scannedBytes = summaryJson.optLong("scannedBytes"),
        reclaimableBytes = summaryJson.optLong("reclaimableBytes"),
        duplicateBytes = summaryJson.optLong("duplicateBytes"),
        largeFileBytes = summaryJson.optLong("largeFileBytes"),
        screenshotBytes = summaryJson.optLong("screenshotBytes"),
        downloadBytes = summaryJson.optLong("downloadBytes"),
        filesScanned = summaryJson.optLong("filesScanned"),
        foldersScanned = summaryJson.optLong("foldersScanned"),
        duplicateGroups = summaryJson.optLong("duplicateGroups"),
        emptyFolders = summaryJson.optLong("emptyFolders"),
    )

    val items = buildList {
        for (index in 0 until itemsJson.length()) {
            val itemJson = itemsJson.getJSONObject(index)
            val categoriesJson = itemJson.optJSONArray("categories") ?: JSONArray()
            val categories = buildSet {
                for (categoryIndex in 0 until categoriesJson.length()) {
                    CleanerCategory.fromKey(categoriesJson.optString(categoryIndex))?.let(::add)
                }
            }

            add(
                CleanerItem(
                    id = itemJson.optString("id"),
                    path = itemJson.optString("path"),
                    name = itemJson.optString("name"),
                    sizeBytes = itemJson.optLong("sizeBytes"),
                    modifiedMillis = itemJson.optLong("modifiedMillis"),
                    isDirectory = itemJson.optBoolean("isDirectory"),
                    isImage = itemJson.optBoolean("isImage"),
                    hash = itemJson.optString("hash"),
                    duplicateGroupId = itemJson.optString("duplicateGroupId"),
                    recommendedForKeepNewest = itemJson.optBoolean("recommendedForKeepNewest"),
                    categories = categories,
                ),
            )
        }
    }

    val duplicateGroups = buildList {
        for (index in 0 until duplicateGroupsJson.length()) {
            val groupJson = duplicateGroupsJson.getJSONObject(index)
            val pathsJson = groupJson.optJSONArray("paths") ?: JSONArray()
            val paths = buildList {
                for (pathIndex in 0 until pathsJson.length()) {
                    add(pathsJson.optString(pathIndex))
                }
            }
            add(
                DuplicateGroup(
                    id = groupJson.optString("id"),
                    keepPath = groupJson.optString("keepPath"),
                    reclaimableBytes = groupJson.optLong("reclaimableBytes"),
                    paths = paths,
                ),
            )
        }
    }

    return ScanPayload(
        summary = summary,
        items = items,
        duplicateGroups = duplicateGroups,
        errors = errorsJson.toStringList(),
    )
}

fun parseDeletePayload(json: String): DeletePayload {
    val root = JSONObject(json)
    val failuresJson = root.optJSONArray("failures") ?: JSONArray()
    val failures = buildList {
        for (index in 0 until failuresJson.length()) {
            val item = failuresJson.getJSONObject(index)
            add(DeleteFailure(item.optString("path"), item.optString("reason")))
        }
    }

    return DeletePayload(
        deletedBytes = root.optLong("deletedBytes"),
        deletedPaths = (root.optJSONArray("deletedPaths") ?: JSONArray()).toStringList(),
        failures = failures,
    )
}

private fun JSONArray.toStringList(): List<String> = buildList {
    for (index in 0 until length()) {
        add(optString(index))
    }
}
