import SwiftUI

enum CleanerTab: String, CaseIterable, Identifiable {
    case home = "Home"
    case scan = "Scan"
    case results = "Results"
    case settings = "Settings"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .home: return "house.fill"
        case .scan: return "magnifyingglass"
        case .results: return "list.bullet.rectangle.fill"
        case .settings: return "gearshape.fill"
        }
    }
}

enum CleanerCategory: String, CaseIterable, Identifiable {
    case duplicatePhotos = "duplicate_photos"
    case largeFiles = "large_files"
    case screenshots = "screenshots"
    case downloads = "downloads"
    case emptyFolders = "empty_folders"

    var id: String { rawValue }

    var label: String {
        switch self {
        case .duplicatePhotos: return "Duplicate Photos"
        case .largeFiles: return "Large Files"
        case .screenshots: return "Screenshots"
        case .downloads: return "Downloads"
        case .emptyFolders: return "Empty Folders"
        }
    }

    var icon: String {
        switch self {
        case .duplicatePhotos: return "photo.on.rectangle.angled"
        case .largeFiles: return "doc.fill"
        case .screenshots: return "camera.viewfinder"
        case .downloads: return "arrow.down.circle.fill"
        case .emptyFolders: return "folder.fill"
        }
    }
}

enum ThemeMode: String, CaseIterable, Identifiable {
    case dark = "Dark"
    case light = "Light"
    case system = "System"

    var id: String { rawValue }

    var colorScheme: ColorScheme? {
        switch self {
        case .dark: return .dark
        case .light: return .light
        case .system: return nil
        }
    }
}

struct CleanerSettings {
    var largeFileThresholdBytes: UInt64 = 100 * 1024 * 1024
    var includeScreenshots = true
    var includeDownloads = true
    var theme: ThemeMode = .dark
}

struct CleanerItem: Identifiable, Hashable {
    var id: String
    var name: String
    var path: String
    var sizeBytes: UInt64
    var modifiedMillis: Int64
    var isDirectory: Bool
    var isImage: Bool
    var duplicateGroupId: String
    var recommendedForKeepNewest: Bool
    var categories: Set<CleanerCategory>
}

struct ScanSummary {
    var totalSpaceBytes: UInt64 = 0
    var availableSpaceBytes: UInt64 = 0
    var scannedBytes: UInt64 = 0
    var reclaimableBytes: UInt64 = 0
    var duplicateBytes: UInt64 = 0
    var largeFileBytes: UInt64 = 0
    var screenshotBytes: UInt64 = 0
    var downloadBytes: UInt64 = 0
    var filesScanned: UInt64 = 0
    var foldersScanned: UInt64 = 0
    var duplicateGroups: UInt64 = 0
    var emptyFolders: UInt64 = 0
}

struct ScanPayload {
    var summary = ScanSummary()
    var items: [CleanerItem] = []
    var errors: [String] = []
}

struct DeletePayload {
    var deletedBytes: UInt64
    var deletedPaths: [String]
    var failures: [String]
}

func formatBytes(_ bytes: UInt64) -> String {
    guard bytes > 0 else { return "0 B" }
    let units = ["B", "KB", "MB", "GB", "TB"]
    var value = Double(bytes)
    var index = 0
    while value >= 1024, index < units.count - 1 {
        value /= 1024
        index += 1
    }
    return value >= 10 || index == 0
        ? String(format: "%.0f %@", value, units[index])
        : String(format: "%.1f %@", value, units[index])
}
