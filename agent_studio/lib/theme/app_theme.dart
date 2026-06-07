import 'package:flutter/material.dart';

class AppColors {
  static const background   = Color(0xFF0A0E1A);
  static const surface      = Color(0xFF111827);
  static const surfaceAlt   = Color(0xFF1A2235);
  static const card         = Color(0xFF1E2D40);
  static const cardHover    = Color(0xFF243348);
  static const border       = Color(0xFF2A3A52);
  static const borderLight  = Color(0xFF3A4F6A);

  static const primary      = Color(0xFF4F8EF7);
  static const primaryDark  = Color(0xFF2D6EE8);
  static const secondary    = Color(0xFF8B5CF6);
  static const accent       = Color(0xFF06D6A0);
  static const warning      = Color(0xFFF59E0B);
  static const error        = Color(0xFFEF4444);

  static const textPrimary   = Color(0xFFE8EDF5);
  static const textSecondary = Color(0xFF8CA3BC);
  static const textMuted     = Color(0xFF4F6580);

  static const agentColors = [
    Color(0xFF4F8EF7),
    Color(0xFF8B5CF6),
    Color(0xFF06D6A0),
    Color(0xFFF59E0B),
    Color(0xFFEF4444),
    Color(0xFF10B981),
    Color(0xFFF97316),
    Color(0xFF3B82F6),
  ];

  static const statusIdle      = Color(0xFF4B5563);
  static const statusRunning   = Color(0xFF4F8EF7);
  static const statusWaiting   = Color(0xFFF59E0B);
  static const statusDone      = Color(0xFF06D6A0);
  static const statusError     = Color(0xFFEF4444);
  static const statusCancelled = Color(0xFF6B7280);
}

class AppTheme {
  static ThemeData get dark {
    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      scaffoldBackgroundColor: AppColors.background,
      colorScheme: const ColorScheme.dark(
        background: AppColors.background,
        surface: AppColors.surface,
        primary: AppColors.primary,
        secondary: AppColors.secondary,
        error: AppColors.error,
        onPrimary: Colors.white,
        onSecondary: Colors.white,
        onSurface: AppColors.textPrimary,
        onBackground: AppColors.textPrimary,
        surfaceVariant: AppColors.surfaceAlt,
        outline: AppColors.border,
      ),
      cardTheme: const CardThemeData(
        color: AppColors.card,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.all(Radius.circular(12)),
          side: BorderSide(color: AppColors.border, width: 1),
        ),
      ),
      appBarTheme: const AppBarTheme(
        backgroundColor: AppColors.surface,
        foregroundColor: AppColors.textPrimary,
        elevation: 0,
        scrolledUnderElevation: 0,
        surfaceTintColor: Colors.transparent,
        titleTextStyle: TextStyle(
          color: AppColors.textPrimary,
          fontSize: 18,
          fontWeight: FontWeight.w600,
          letterSpacing: 0.3,
        ),
      ),
      dividerTheme: const DividerThemeData(
        color: AppColors.border,
        thickness: 1,
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: AppColors.surfaceAlt,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: const BorderSide(color: AppColors.border),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: const BorderSide(color: AppColors.border),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(10),
          borderSide: const BorderSide(color: AppColors.primary, width: 1.5),
        ),
        labelStyle: const TextStyle(color: AppColors.textSecondary),
        hintStyle: const TextStyle(color: AppColors.textMuted),
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      ),
      elevatedButtonTheme: ElevatedButtonThemeData(
        style: ElevatedButton.styleFrom(
          backgroundColor: AppColors.primary,
          foregroundColor: Colors.white,
          elevation: 0,
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
          textStyle: const TextStyle(fontSize: 14, fontWeight: FontWeight.w600),
        ),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
          foregroundColor: AppColors.primary,
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          foregroundColor: AppColors.textPrimary,
          side: const BorderSide(color: AppColors.border),
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        ),
      ),
      textTheme: const TextTheme(
        displayLarge:  TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w700),
        displayMedium: TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w700),
        headlineLarge: TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w700, fontSize: 28),
        headlineMedium: TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w600, fontSize: 22),
        headlineSmall:  TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w600, fontSize: 18),
        titleLarge:  TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w600, fontSize: 16),
        titleMedium: TextStyle(color: AppColors.textPrimary, fontWeight: FontWeight.w500, fontSize: 14),
        titleSmall:  TextStyle(color: AppColors.textSecondary, fontWeight: FontWeight.w500, fontSize: 12),
        bodyLarge:   TextStyle(color: AppColors.textPrimary,   fontSize: 15),
        bodyMedium:  TextStyle(color: AppColors.textPrimary,   fontSize: 14),
        bodySmall:   TextStyle(color: AppColors.textSecondary, fontSize: 12),
        labelLarge:  TextStyle(color: AppColors.textPrimary,   fontWeight: FontWeight.w600, fontSize: 14),
        labelSmall:  TextStyle(color: AppColors.textMuted,     fontSize: 11, letterSpacing: 0.8),
      ),
      chipTheme: ChipThemeData(
        backgroundColor: AppColors.surfaceAlt,
        selectedColor: AppColors.primary.withOpacity(0.3),
        labelStyle: const TextStyle(color: AppColors.textPrimary, fontSize: 12),
        side: const BorderSide(color: AppColors.border),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
      ),
      switchTheme: SwitchThemeData(
        thumbColor: MaterialStateProperty.resolveWith((s) =>
          s.contains(MaterialState.selected) ? AppColors.primary : AppColors.textMuted),
        trackColor: MaterialStateProperty.resolveWith((s) =>
          s.contains(MaterialState.selected) ? AppColors.primary.withOpacity(0.4) : AppColors.border),
      ),
      scrollbarTheme: ScrollbarThemeData(
        thumbColor: MaterialStateProperty.all(AppColors.borderLight),
        thickness: MaterialStateProperty.all(4),
        radius: const Radius.circular(4),
      ),
      tooltipTheme: TooltipThemeData(
        decoration: BoxDecoration(
          color: AppColors.card,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: AppColors.border),
        ),
        textStyle: const TextStyle(color: AppColors.textPrimary, fontSize: 12),
      ),
    );
  }
}
