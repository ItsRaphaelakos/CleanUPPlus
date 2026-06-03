@file:OptIn(androidx.compose.material3.ExperimentalMaterial3Api::class)

package com.cleanup.plus

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.Crossfade
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Download
import androidx.compose.material.icons.rounded.Folder
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Image
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.InsertDriveFile
import androidx.compose.material.icons.rounded.List
import androidx.compose.material.icons.rounded.Search
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.Storage
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.cleanup.plus.ui.CleanUpTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.roundToInt

private enum class Screen(val label: String, val icon: ImageVector) {
    Home("Home", Icons.Rounded.Home),
    Scan("Scan", Icons.Rounded.Search),
    Results("Results", Icons.Rounded.List),
    Settings("Settings", Icons.Rounded.Settings),
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            val context = LocalContext.current
            val lifecycleOwner = LocalLifecycleOwner.current
            val scope = rememberCoroutineScope()
            val snackbarHostState = remember { SnackbarHostState() }
            val settingsStore = remember { CleanerSettingsStore(applicationContext) }

            var settings by remember { mutableStateOf(settingsStore.load()) }
            var screen by remember { mutableStateOf(Screen.Home) }
            var selectedCategory by remember { mutableStateOf<CleanerCategory?>(null) }
            var scanPayload by remember { mutableStateOf(ScanPayload()) }
            var selectedPaths by remember { mutableStateOf<Set<String>>(emptySet()) }
            var hasStorageAccess by remember { mutableStateOf(context.hasBroadStorageAccess()) }
            var storageInfo by remember { mutableStateOf(readDeviceStorageInfo()) }
            var isScanning by remember { mutableStateOf(false) }
            var isDeleting by remember { mutableStateOf(false) }
            var showDeleteDialog by remember { mutableStateOf(false) }

            val permissionLauncher = rememberLauncherForActivityResult(
                ActivityResultContracts.RequestMultiplePermissions(),
            ) {
                hasStorageAccess = context.hasBroadStorageAccess()
            }

            fun updateSettings(next: CleanerSettings) {
                settings = next
                settingsStore.save(next)
            }

            fun requestStorageAccess() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    this@MainActivity.openBroadStorageSettings()
                } else {
                    permissionLauncher.launch(mediaReadPermissionsForCurrentSdk())
                }
            }

            fun runScan() {
                hasStorageAccess = context.hasBroadStorageAccess()
                if (!hasStorageAccess) {
                    requestStorageAccess()
                    return
                }

                val roots = defaultScanRoots()
                if (roots.isEmpty()) {
                    scope.launch { snackbarHostState.showSnackbar("No readable storage root found.") }
                    return
                }

                isScanning = true
                screen = Screen.Scan
                selectedPaths = emptySet()

                scope.launch {
                    val payload = withContext(Dispatchers.IO) {
                        runCatching { NativeCleanerBridge.scan(roots, settings) }
                            .getOrElse { error ->
                                ScanPayload(errors = listOf(error.message ?: "Scan failed."))
                            }
                    }
                    scanPayload = payload
                    storageInfo = readDeviceStorageInfo()
                    isScanning = false
                    screen = Screen.Results

                    val message = if (payload.items.isEmpty()) {
                        "Scan complete. No cleanup candidates found."
                    } else {
                        "Scan complete. ${payload.items.size} cleanup candidates found."
                    }
                    snackbarHostState.showSnackbar(message)
                }
            }

            fun deleteSelected() {
                val paths = selectedPaths.toList()
                if (paths.isEmpty()) return

                isDeleting = true
                scope.launch {
                    val deletePayload = withContext(Dispatchers.IO) {
                        NativeCleanerBridge.deleteSelected(paths, confirmed = true)
                    }

                    selectedPaths = emptySet()
                    scanPayload = scanPayload.copy(
                        items = scanPayload.items.filterNot { it.path in deletePayload.deletedPaths.toSet() },
                    )
                    storageInfo = readDeviceStorageInfo()
                    isDeleting = false

                    val message = buildString {
                        append("Deleted ${deletePayload.deletedPaths.size} item(s)")
                        append(" / ${formatBytes(deletePayload.deletedBytes)}")
                        if (deletePayload.failures.isNotEmpty()) {
                            append(". ${deletePayload.failures.size} failed.")
                        }
                    }
                    snackbarHostState.showSnackbar(message)
                }
            }

            DisposableEffect(lifecycleOwner) {
                val observer = LifecycleEventObserver { _, event ->
                    if (event == Lifecycle.Event.ON_RESUME) {
                        hasStorageAccess = context.hasBroadStorageAccess()
                        storageInfo = readDeviceStorageInfo()
                    }
                }
                lifecycleOwner.lifecycle.addObserver(observer)
                onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
            }

            CleanUpTheme(themeMode = settings.themeMode) {
                Scaffold(
                    snackbarHost = { SnackbarHost(snackbarHostState) },
                    bottomBar = {
                        CleanBottomBar(
                            current = screen,
                            onSelect = { screen = it },
                        )
                    },
                ) { padding ->
                    Surface(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(padding),
                        color = MaterialTheme.colorScheme.background,
                    ) {
                        Crossfade(targetState = screen, label = "screen") { target ->
                            when (target) {
                                Screen.Home -> HomeScreen(
                                    storageInfo = storageInfo,
                                    scanPayload = scanPayload,
                                    hasStorageAccess = hasStorageAccess,
                                    onScanClick = ::runScan,
                                    onRequestStorageAccess = ::requestStorageAccess,
                                    onOpenCategory = {
                                        selectedCategory = it
                                        screen = Screen.Results
                                    },
                                )

                                Screen.Scan -> ScanScreen(
                                    hasStorageAccess = hasStorageAccess,
                                    isScanning = isScanning,
                                    settings = settings,
                                    onScanClick = ::runScan,
                                    onRequestStorageAccess = ::requestStorageAccess,
                                )

                                Screen.Results -> ResultsScreen(
                                    scanPayload = scanPayload,
                                    selectedCategory = selectedCategory,
                                    selectedPaths = selectedPaths,
                                    isDeleting = isDeleting,
                                    onCategoryChange = { selectedCategory = it },
                                    onTogglePath = { path ->
                                        selectedPaths = if (path in selectedPaths) {
                                            selectedPaths - path
                                        } else {
                                            selectedPaths + path
                                        }
                                    },
                                    onSelectKeepNewest = {
                                        selectedCategory = CleanerCategory.DuplicatePhotos
                                        selectedPaths = selectedPaths + scanPayload.items
                                            .filter { it.recommendedForKeepNewest }
                                            .map { it.path }
                                            .toSet()
                                    },
                                    onDeleteSelected = { showDeleteDialog = true },
                                    onScanClick = ::runScan,
                                )

                                Screen.Settings -> SettingsScreen(
                                    settings = settings,
                                    onSettingsChange = ::updateSettings,
                                )
                            }
                        }
                    }
                }

                if (showDeleteDialog) {
                    val selectedItems = scanPayload.items.filter { it.path in selectedPaths }
                    ConfirmDeleteDialog(
                        selectedCount = selectedItems.size,
                        selectedBytes = selectedItems.sumOf { it.sizeBytes },
                        onDismiss = { showDeleteDialog = false },
                        onConfirm = {
                            showDeleteDialog = false
                            deleteSelected()
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun CleanBottomBar(current: Screen, onSelect: (Screen) -> Unit) {
    NavigationBar(containerColor = MaterialTheme.colorScheme.surface) {
        Screen.entries.forEach { item ->
            NavigationBarItem(
                selected = current == item,
                onClick = { onSelect(item) },
                icon = { Icon(item.icon, contentDescription = item.label) },
                label = { Text(item.label, maxLines = 1) },
            )
        }
    }
}

@Composable
private fun HomeScreen(
    storageInfo: DeviceStorageInfo,
    scanPayload: ScanPayload,
    hasStorageAccess: Boolean,
    onScanClick: () -> Unit,
    onRequestStorageAccess: () -> Unit,
    onOpenCategory: (CleanerCategory) -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(20.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        item {
            Header(title = "CleanUp+", subtitle = "Storage cleaner and duplicate finder")
        }

        item {
            StorageOverviewCard(storageInfo = storageInfo, onScanClick = onScanClick)
        }

        if (!hasStorageAccess) {
            item {
                PermissionCard(onRequestStorageAccess = onRequestStorageAccess)
            }
        }

        item {
            Text(
                text = "Cleanup targets",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
            )
        }

        items(categoryCards(scanPayload), key = { it.category.key }) { card ->
            CategoryCard(card = card, onClick = { onOpenCategory(card.category) })
        }
    }
}

@Composable
private fun ScanScreen(
    hasStorageAccess: Boolean,
    isScanning: Boolean,
    settings: CleanerSettings,
    onScanClick: () -> Unit,
    onRequestStorageAccess: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(20.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        item {
            Header(title = "Scan Storage", subtitle = "Find duplicates, downloads and old clutter")
        }

        item {
            ElevatedCard(
                colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
                shape = RoundedCornerShape(24.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    verticalArrangement = Arrangement.spacedBy(14.dp),
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = if (hasStorageAccess) Icons.Rounded.CheckCircle else Icons.Rounded.Warning,
                            contentDescription = null,
                            tint = if (hasStorageAccess) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.error,
                        )
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(
                                text = if (hasStorageAccess) "Storage access enabled" else "Storage access required",
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                            )
                            Text(
                                text = if (hasStorageAccess) {
                                    "CleanUp+ can scan the shared storage root."
                                } else {
                                    "Grant all-files access to scan photos, downloads and folders."
                                },
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }

                    if (!hasStorageAccess) {
                        OutlinedButton(onClick = onRequestStorageAccess) {
                            Text("Grant Storage Access")
                        }
                    }
                }
            }
        }

        item {
            ElevatedCard(
                colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
                shape = RoundedCornerShape(24.dp),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Text(
                        "Current scan rules",
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold,
                    )
                    RuleRow("Large files", "Over ${thresholdLabel(settings.largeFileThresholdBytes)}")
                    RuleRow("Screenshots", if (settings.includeScreenshots) "Included" else "Skipped")
                    RuleRow("Downloads", if (settings.includeDownloads) "Included" else "Skipped")
                    RuleRow("Duplicates", "Images grouped by size and content hash")
                }
            }
        }

        item {
            Button(
                onClick = onScanClick,
                enabled = !isScanning,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(56.dp),
                shape = RoundedCornerShape(18.dp),
            ) {
                Icon(Icons.Rounded.Search, contentDescription = null)
                Spacer(Modifier.width(10.dp))
                Text(if (isScanning) "Scanning..." else "Scan Storage")
            }
        }

        item {
            AnimatedVisibility(visible = isScanning) {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    Text(
                        "Hashing duplicate candidates can take a little longer for large photo libraries.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}

@Composable
private fun ResultsScreen(
    scanPayload: ScanPayload,
    selectedCategory: CleanerCategory?,
    selectedPaths: Set<String>,
    isDeleting: Boolean,
    onCategoryChange: (CleanerCategory?) -> Unit,
    onTogglePath: (String) -> Unit,
    onSelectKeepNewest: () -> Unit,
    onDeleteSelected: () -> Unit,
    onScanClick: () -> Unit,
) {
    val filteredItems = remember(scanPayload, selectedCategory) {
        selectedCategory?.let { category ->
            scanPayload.items.filter { category in it.categories }
        } ?: scanPayload.items
    }
    val potentialBytes = remember(scanPayload.items) {
        potentialCleanupBytes(scanPayload.items)
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            Header(
                title = "Results",
                subtitle = "${scanPayload.items.size} candidates / ${formatBytes(potentialBytes)} potential cleanup",
            )
        }

        if (scanPayload.errors.isNotEmpty()) {
            item {
                ErrorCard(errors = scanPayload.errors)
            }
        }

        item {
            LazyRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                item {
                    FilterChip(
                        selected = selectedCategory == null,
                        onClick = { onCategoryChange(null) },
                        label = { Text("All") },
                    )
                }
                items(CleanerCategory.entries, key = { it.key }) { category ->
                    FilterChip(
                        selected = selectedCategory == category,
                        onClick = { onCategoryChange(category) },
                        label = { Text(category.label) },
                    )
                }
            }
        }

        item {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
                OutlinedButton(
                    onClick = onSelectKeepNewest,
                    enabled = scanPayload.items.any { it.recommendedForKeepNewest },
                    modifier = Modifier.weight(1f),
                    shape = RoundedCornerShape(16.dp),
                ) {
                    Text("Keep newest")
                }
                Button(
                    onClick = onDeleteSelected,
                    enabled = selectedPaths.isNotEmpty() && !isDeleting,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                    shape = RoundedCornerShape(16.dp),
                ) {
                    Icon(Icons.Rounded.Delete, contentDescription = null)
                    Spacer(Modifier.width(8.dp))
                    Text("Delete Selected")
                }
            }
        }

        if (filteredItems.isEmpty()) {
            item {
                EmptyResults(onScanClick = onScanClick)
            }
        } else {
            items(filteredItems, key = { it.id }) { item ->
                ResultRow(
                    cleanerItem = item,
                    selected = item.path in selectedPaths,
                    onToggle = { onTogglePath(item.path) },
                )
            }
        }
    }
}

@Composable
private fun SettingsScreen(
    settings: CleanerSettings,
    onSettingsChange: (CleanerSettings) -> Unit,
) {
    val thresholdMb = (settings.largeFileThresholdBytes / 1024L / 1024L).toFloat()

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(20.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        item {
            Header(title = "Settings", subtitle = "Tune scan rules and appearance")
        }

        item {
            ElevatedCard(
                shape = RoundedCornerShape(24.dp),
                colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    verticalArrangement = Arrangement.spacedBy(16.dp),
                ) {
                    Text(
                        "Large file threshold",
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold,
                    )
                    Text(
                        thresholdLabel(settings.largeFileThresholdBytes),
                        style = MaterialTheme.typography.headlineSmall,
                        color = MaterialTheme.colorScheme.primary,
                    )
                    Slider(
                        value = thresholdMb.coerceIn(50f, 2048f),
                        onValueChange = { mb ->
                            val rounded = (mb / 25f).roundToInt() * 25L
                            onSettingsChange(
                                settings.copy(
                                    largeFileThresholdBytes = rounded.coerceIn(50L, 2048L) * 1024L * 1024L,
                                ),
                            )
                        },
                        valueRange = 50f..2048f,
                    )
                }
            }
        }

        item {
            SettingsSwitchCard(
                title = "Include screenshots",
                subtitle = "Detect files in screenshot folders or with screenshot names.",
                checked = settings.includeScreenshots,
                onCheckedChange = { onSettingsChange(settings.copy(includeScreenshots = it)) },
            )
        }

        item {
            SettingsSwitchCard(
                title = "Include downloads",
                subtitle = "Include files under Download and Downloads folders.",
                checked = settings.includeDownloads,
                onCheckedChange = { onSettingsChange(settings.copy(includeDownloads = it)) },
            )
        }

        item {
            ElevatedCard(
                shape = RoundedCornerShape(24.dp),
                colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    Text(
                        "Theme",
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold,
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        AppThemeMode.entries.forEach { mode ->
                            FilterChip(
                                selected = settings.themeMode == mode,
                                onClick = { onSettingsChange(settings.copy(themeMode = mode)) },
                                label = { Text(mode.label) },
                            )
                        }
                    }
                }
            }
        }

        item {
            ElevatedCard(
                shape = RoundedCornerShape(24.dp),
                colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Row(
                    modifier = Modifier.padding(20.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Icon(Icons.Rounded.Info, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    Spacer(Modifier.width(14.dp))
                    Column {
                        Text("About CleanUp+", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                        Text(
                            "Cross-platform storage cleaner with a shared C++ scanner core and native UI wrappers.",
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun Header(title: String, subtitle: String) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(
            text = title,
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
        )
        Text(
            text = subtitle,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun StorageOverviewCard(storageInfo: DeviceStorageInfo, onScanClick: () -> Unit) {
    val animatedProgress by animateFloatAsState(
        targetValue = storageInfo.usedFraction.coerceIn(0f, 1f),
        label = "storage-progress",
    )

    ElevatedCard(
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
        shape = RoundedCornerShape(28.dp),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(18.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(contentAlignment = Alignment.Center, modifier = Modifier.size(112.dp)) {
                    CircularProgressIndicator(
                        progress = { animatedProgress },
                        modifier = Modifier.fillMaxSize(),
                        strokeWidth = 10.dp,
                        color = MaterialTheme.colorScheme.primary,
                        trackColor = MaterialTheme.colorScheme.surfaceVariant,
                    )
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            "${(animatedProgress * 100).roundToInt()}%",
                            style = MaterialTheme.typography.titleLarge,
                            fontWeight = FontWeight.Bold,
                        )
                        Text("used", style = MaterialTheme.typography.bodySmall)
                    }
                }
                Spacer(Modifier.width(20.dp))
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    MetricLine("Total space", formatBytes(storageInfo.totalBytes))
                    MetricLine("Available", formatBytes(storageInfo.availableBytes))
                    MetricLine("Used", formatBytes(storageInfo.usedBytes))
                }
            }

            Button(
                onClick = onScanClick,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(54.dp),
                shape = RoundedCornerShape(18.dp),
            ) {
                Icon(Icons.Rounded.Search, contentDescription = null)
                Spacer(Modifier.width(10.dp))
                Text("Scan Storage")
            }
        }
    }
}

@Composable
private fun PermissionCard(onRequestStorageAccess: () -> Unit) {
    ElevatedCard(
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
        shape = RoundedCornerShape(24.dp),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Rounded.Warning, contentDescription = null, tint = MaterialTheme.colorScheme.onErrorContainer)
                Spacer(Modifier.width(12.dp))
                Text(
                    "All-files access is needed for a full scan.",
                    color = MaterialTheme.colorScheme.onErrorContainer,
                    fontWeight = FontWeight.SemiBold,
                )
            }
            OutlinedButton(onClick = onRequestStorageAccess) {
                Text("Grant Storage Access")
            }
        }
    }
}

private data class CategoryCardModel(
    val category: CleanerCategory,
    val icon: ImageVector,
    val count: Long,
    val bytes: Long,
)

private fun categoryCards(scanPayload: ScanPayload): List<CategoryCardModel> {
    val items = scanPayload.items
    val duplicateCandidates = items.filter { it.recommendedForKeepNewest }
    return listOf(
        CategoryCardModel(
            CleanerCategory.DuplicatePhotos,
            Icons.Rounded.Image,
            duplicateCandidates
                .map { it.duplicateGroupId }
                .filter { it.isNotBlank() }
                .distinct()
                .size
                .toLong(),
            duplicateCandidates.sumOf { it.sizeBytes },
        ),
        CategoryCardModel(
            CleanerCategory.LargeFiles,
            Icons.Rounded.InsertDriveFile,
            items.count { CleanerCategory.LargeFiles in it.categories }.toLong(),
            items.filter { CleanerCategory.LargeFiles in it.categories }.sumOf { it.sizeBytes },
        ),
        CategoryCardModel(
            CleanerCategory.Screenshots,
            Icons.Rounded.Image,
            items.count { CleanerCategory.Screenshots in it.categories }.toLong(),
            items.filter { CleanerCategory.Screenshots in it.categories }.sumOf { it.sizeBytes },
        ),
        CategoryCardModel(
            CleanerCategory.Downloads,
            Icons.Rounded.Download,
            items.count { CleanerCategory.Downloads in it.categories }.toLong(),
            items.filter { CleanerCategory.Downloads in it.categories }.sumOf { it.sizeBytes },
        ),
        CategoryCardModel(
            CleanerCategory.EmptyFolders,
            Icons.Rounded.Folder,
            items.count { CleanerCategory.EmptyFolders in it.categories }.toLong(),
            0L,
        ),
    )
}

private fun potentialCleanupBytes(items: List<CleanerItem>): Long {
    return items
        .asSequence()
        .filter {
            it.recommendedForKeepNewest ||
                CleanerCategory.LargeFiles in it.categories ||
                CleanerCategory.Screenshots in it.categories ||
                CleanerCategory.Downloads in it.categories ||
                CleanerCategory.EmptyFolders in it.categories
        }
        .distinctBy { it.path }
        .sumOf { it.sizeBytes }
}

@Composable
private fun CategoryCard(card: CategoryCardModel, onClick: () -> Unit) {
    ElevatedCard(
        onClick = onClick,
        shape = RoundedCornerShape(22.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier
            .fillMaxWidth()
            .animateContentSize(),
    ) {
        Row(
            modifier = Modifier.padding(18.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Box(
                modifier = Modifier
                    .size(48.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)),
                contentAlignment = Alignment.Center,
            ) {
                Icon(card.icon, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
            }
            Spacer(Modifier.width(16.dp))
            Column(Modifier.weight(1f)) {
                Text(card.category.label, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                Text(
                    if (card.bytes > 0L) "${card.count} found / ${formatBytes(card.bytes)}" else "${card.count} found",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun ResultRow(cleanerItem: CleanerItem, selected: Boolean, onToggle: () -> Unit) {
    ElevatedCard(
        shape = RoundedCornerShape(22.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onToggle),
    ) {
        Row(
            modifier = Modifier.padding(14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Checkbox(checked = selected, onCheckedChange = { onToggle() })
            Spacer(Modifier.width(8.dp))
            FilePreview(item = cleanerItem)
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        cleanerItem.name.ifBlank { cleanerItem.path },
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    Text(
                        formatBytes(cleanerItem.sizeBytes),
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
                Text(
                    cleanerItem.path,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    "Modified ${formatDate(cleanerItem.modifiedMillis)}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                LazyRow(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    items(cleanerItem.categories.toList(), key = { it.key }) { category ->
                        AssistChip(
                            onClick = {},
                            label = { Text(category.label, maxLines = 1) },
                        )
                    }
                    if (cleanerItem.recommendedForKeepNewest) {
                        item {
                            AssistChip(onClick = {}, label = { Text("Delete copy", maxLines = 1) })
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun FilePreview(item: CleanerItem) {
    val bitmap by produceState<Bitmap?>(initialValue = null, key1 = item.path, key2 = item.isImage) {
        value = if (item.isImage && !item.isDirectory) {
            withContext(Dispatchers.IO) { decodePreviewBitmap(item.path, 96, 96) }
        } else {
            null
        }
    }

    Box(
        modifier = Modifier
            .size(56.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant),
        contentAlignment = Alignment.Center,
    ) {
        if (bitmap != null) {
            Image(
                bitmap = bitmap!!.asImageBitmap(),
                contentDescription = item.name,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop,
            )
        } else {
            Icon(
                imageVector = if (item.isDirectory) Icons.Rounded.Folder else Icons.Rounded.InsertDriveFile,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

private fun decodePreviewBitmap(path: String, reqWidth: Int, reqHeight: Int): Bitmap? {
    return runCatching {
        val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(path, options)

        var sampleSize = 1
        if (options.outHeight > reqHeight || options.outWidth > reqWidth) {
            var halfHeight = options.outHeight / 2
            var halfWidth = options.outWidth / 2
            while (halfHeight / sampleSize >= reqHeight && halfWidth / sampleSize >= reqWidth) {
                sampleSize *= 2
            }
        }

        BitmapFactory.decodeFile(
            path,
            BitmapFactory.Options().apply {
                inSampleSize = sampleSize.coerceAtLeast(1)
            },
        )
    }.getOrNull()
}

@Composable
private fun RuleRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun MetricLine(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun SettingsSwitchCard(
    title: String,
    subtitle: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    ElevatedCard(
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Row(
            modifier = Modifier.padding(20.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                Text(subtitle, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Switch(checked = checked, onCheckedChange = onCheckedChange)
        }
    }
}

@Composable
private fun ErrorCard(errors: List<String>) {
    ElevatedCard(
        shape = RoundedCornerShape(22.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.errorContainer),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Rounded.Warning, contentDescription = null, tint = MaterialTheme.colorScheme.onErrorContainer)
                Spacer(Modifier.width(10.dp))
                Text("Scan warnings", color = MaterialTheme.colorScheme.onErrorContainer, fontWeight = FontWeight.SemiBold)
            }
            errors.take(4).forEach { error ->
                Text(error, color = MaterialTheme.colorScheme.onErrorContainer, style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

@Composable
private fun EmptyResults(onScanClick: () -> Unit) {
    ElevatedCard(
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surface),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Icon(Icons.Rounded.Storage, contentDescription = null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(42.dp))
            Text("No results yet", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Text(
                "Run a scan to populate duplicate photos, large files, screenshots, downloads and empty folders.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            OutlinedButton(onClick = onScanClick, shape = RoundedCornerShape(16.dp)) {
                Text("Scan Storage")
            }
        }
    }
}

@Composable
private fun ConfirmDeleteDialog(
    selectedCount: Int,
    selectedBytes: Long,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        icon = { Icon(Icons.Rounded.Delete, contentDescription = null) },
        title = { Text("Delete selected files?") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text("CleanUp+ will permanently delete $selectedCount selected item(s).")
                HorizontalDivider()
                Text(
                    "Space affected: ${formatBytes(selectedBytes)}",
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                Text(
                    "This action only runs after this confirmation.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        },
        confirmButton = {
            Button(
                onClick = onConfirm,
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
            ) {
                Text("Delete")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        },
    )
}
