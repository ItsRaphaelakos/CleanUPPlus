package com.cleanup.plus.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import com.cleanup.plus.AppThemeMode

private val DarkColors = darkColorScheme(
    primary = Color(0xFF22D3EE),
    onPrimary = Color(0xFF062B36),
    secondary = Color(0xFF60A5FA),
    onSecondary = Color(0xFF07111F),
    tertiary = Color(0xFF34D399),
    background = Color(0xFF080B10),
    onBackground = Color(0xFFE6EDF3),
    surface = Color(0xFF10151C),
    onSurface = Color(0xFFE6EDF3),
    surfaceVariant = Color(0xFF18212B),
    onSurfaceVariant = Color(0xFFB8C4CF),
    error = Color(0xFFF87171),
)

private val LightColors = lightColorScheme(
    primary = Color(0xFF0284C7),
    onPrimary = Color.White,
    secondary = Color(0xFF0891B2),
    onSecondary = Color.White,
    tertiary = Color(0xFF059669),
    background = Color(0xFFF8FAFC),
    onBackground = Color(0xFF0F172A),
    surface = Color.White,
    onSurface = Color(0xFF0F172A),
    surfaceVariant = Color(0xFFE2E8F0),
    onSurfaceVariant = Color(0xFF475569),
    error = Color(0xFFDC2626),
)

@Composable
fun CleanUpTheme(
    themeMode: AppThemeMode,
    content: @Composable () -> Unit,
) {
    val dark = when (themeMode) {
        AppThemeMode.Dark -> true
        AppThemeMode.Light -> false
        AppThemeMode.System -> isSystemInDarkTheme()
    }

    MaterialTheme(
        colorScheme = if (dark) DarkColors else LightColors,
        content = content,
    )
}
