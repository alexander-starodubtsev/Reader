package com.goodreader.windows

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val LightColors =
    lightColorScheme(
        primary = Color(0xFF604638),
        onPrimary = Color.White,
        secondary = Color(0xFF52664D),
        background = Color(0xFFF8F6F2),
        surface = Color(0xFFFFFBF7),
        error = Color(0xFFBA1A1A),
    )

private val DarkColors =
    darkColorScheme(
        primary = Color(0xFFE5BFA8),
        onPrimary = Color(0xFF402A1F),
        secondary = Color(0xFFB8CCAE),
        background = Color(0xFF171411),
        surface = Color(0xFF211D19),
        error = Color(0xFFFFB4AB),
    )

internal object GoodReaderSpacing {
    val small = 8.dp
    val medium = 16.dp
    val large = 24.dp
}

@Composable
@Suppress("FunctionNaming", "ktlint:standard:function-naming")
internal fun GoodReaderTheme(content: @Composable () -> Unit) {
    val typography =
        androidx.compose.material3.Typography().copy(
            headlineMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontSize = 28.sp),
            titleMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontSize = 18.sp),
            bodyMedium = TextStyle(fontFamily = FontFamily.SansSerif, fontSize = 16.sp),
        )
    MaterialTheme(
        colorScheme = if (isSystemInDarkTheme()) DarkColors else LightColors,
        typography = typography,
        content = content,
    )
}
