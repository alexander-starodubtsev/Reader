import SwiftUI

/// Shared spacing and semantic colors for the Apple screens.
public enum GoodReaderDesign {
    public static let smallSpacing: CGFloat = 8
    public static let mediumSpacing: CGFloat = 16
    public static let largeSpacing: CGFloat = 24

    public static func accent(for colorScheme: ColorScheme) -> Color {
        colorScheme == .dark
            ? Color(red: 0.90, green: 0.75, blue: 0.66)
            : Color(red: 0.38, green: 0.27, blue: 0.22)
    }

    public static func background(for colorScheme: ColorScheme) -> Color {
        colorScheme == .dark
            ? Color(red: 0.09, green: 0.08, blue: 0.07)
            : Color(red: 0.97, green: 0.96, blue: 0.94)
    }

    public static func cardBackground(for colorScheme: ColorScheme) -> Color {
        colorScheme == .dark
            ? Color(red: 0.13, green: 0.12, blue: 0.11)
            : Color(red: 1.0, green: 0.99, blue: 0.97)
    }

    public static let cardCornerRadius: CGFloat = 16
}
