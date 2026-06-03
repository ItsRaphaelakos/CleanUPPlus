import Foundation

@MainActor
final class CleanerStore: ObservableObject {
    @Published var settings = CleanerSettings()
    @Published var payload = ScanPayload()
    @Published var selectedPaths: Set<String> = []
    @Published var selectedCategory: CleanerCategory?
    @Published var selectedTab: CleanerTab = .home
    @Published var isScanning = false
    @Published var isDeleting = false
    @Published var logLines: [String] = [
        "CleanUp+ iOS ready.",
        "iOS can scan app-accessible storage. Full-device scanning is not allowed by iOS.",
    ]

    var filteredItems: [CleanerItem] {
        guard let selectedCategory else { return payload.items }
        return payload.items.filter { $0.categories.contains(selectedCategory) }
    }

    var potentialCleanupBytes: UInt64 {
        payload.items
            .filter {
                $0.recommendedForKeepNewest ||
                    $0.categories.contains(.largeFiles) ||
                    $0.categories.contains(.screenshots) ||
                    $0.categories.contains(.downloads) ||
                    $0.categories.contains(.emptyFolders)
            }
            .reduce(0) { $0 + $1.sizeBytes }
    }

    func scan() {
        guard !isScanning else { return }
        isScanning = true
        selectedTab = .scan
        selectedPaths.removeAll()
        appendLog("Starting scan...")

        Task {
            let settings = settings
            let result = await Task.detached(priority: .userInitiated) {
                let nativeSettings = CUPScanSettings()
                nativeSettings.largeFileThresholdBytes = settings.largeFileThresholdBytes
                nativeSettings.includeScreenshots = settings.includeScreenshots
                nativeSettings.includeDownloads = settings.includeDownloads

                let roots = Self.defaultScanRoots()
                return CUPScannerBridge().scanRoots(roots, settings: nativeSettings)
            }.value

            payload = Self.parseScanPayload(result)
            isScanning = false
            selectedTab = .results
            appendLog("Scan complete: \(payload.items.count) candidate(s).")

            for error in payload.errors.prefix(3) {
                appendLog("Warning: \(error)")
            }
        }
    }

    func keepNewest() {
        let duplicateCopyPaths = payload.items
            .filter(\.recommendedForKeepNewest)
            .map(\.path)
        selectedPaths.formUnion(duplicateCopyPaths)
        selectedCategory = .duplicatePhotos
        appendLog("Keep newest selected \(duplicateCopyPaths.count) duplicate copy item(s).")
    }

    func toggleSelection(_ item: CleanerItem) {
        if selectedPaths.contains(item.path) {
            selectedPaths.remove(item.path)
            appendLog("Unselected \(item.name).")
        } else {
            selectedPaths.insert(item.path)
            appendLog("Selected \(item.name).")
        }
    }

    func deleteSelected() {
        guard !selectedPaths.isEmpty else {
            appendLog("Delete ignored: no selected items.")
            return
        }

        isDeleting = true
        let paths = Array(selectedPaths)
        appendLog("Deleting \(paths.count) selected item(s)...")

        Task {
            let result = await Task.detached(priority: .userInitiated) {
                CUPScannerBridge().deletePaths(paths, confirmed: true)
            }.value

            let deletePayload = Self.parseDeletePayload(result)
            let deletedSet = Set(deletePayload.deletedPaths)
            payload.items.removeAll { deletedSet.contains($0.path) }
            selectedPaths.subtract(deletedSet)
            isDeleting = false

            appendLog("Deleted \(deletePayload.deletedPaths.count) item(s), \(formatBytes(deletePayload.deletedBytes)).")
            for failure in deletePayload.failures.prefix(3) {
                appendLog("Delete failed: \(failure)")
            }
        }
    }

    func appendLog(_ line: String) {
        logLines.append(line)
        if logLines.count > 80 {
            logLines.removeFirst(logLines.count - 80)
        }
    }

    private static func defaultScanRoots() -> [String] {
        let manager = FileManager.default
        var roots: [String] = []

        if let documents = manager.urls(for: .documentDirectory, in: .userDomainMask).first {
            roots.append(documents.path)
        }
        roots.append(manager.temporaryDirectory.path)

        if roots.allSatisfy({ !manager.fileExists(atPath: $0) }) {
            roots.append(NSHomeDirectory())
        }
        return roots
    }

    private static func parseScanPayload(_ dictionary: [AnyHashable: Any]) -> ScanPayload {
        let summaryDictionary = dictionary["summary"] as? [AnyHashable: Any] ?? [:]
        let itemDictionaries = dictionary["items"] as? [[AnyHashable: Any]] ?? []
        let errors = dictionary["errors"] as? [String] ?? []

        let summary = ScanSummary(
            totalSpaceBytes: uint64(summaryDictionary["totalSpaceBytes"]),
            availableSpaceBytes: uint64(summaryDictionary["availableSpaceBytes"]),
            scannedBytes: uint64(summaryDictionary["scannedBytes"]),
            reclaimableBytes: uint64(summaryDictionary["reclaimableBytes"]),
            duplicateBytes: uint64(summaryDictionary["duplicateBytes"]),
            largeFileBytes: uint64(summaryDictionary["largeFileBytes"]),
            screenshotBytes: uint64(summaryDictionary["screenshotBytes"]),
            downloadBytes: uint64(summaryDictionary["downloadBytes"]),
            filesScanned: uint64(summaryDictionary["filesScanned"]),
            foldersScanned: uint64(summaryDictionary["foldersScanned"]),
            duplicateGroups: uint64(summaryDictionary["duplicateGroups"]),
            emptyFolders: uint64(summaryDictionary["emptyFolders"])
        )

        let items = itemDictionaries.map { item in
            let categoryKeys = item["categories"] as? [String] ?? []
            let categories = Set(categoryKeys.compactMap(CleanerCategory.init(rawValue:)))
            return CleanerItem(
                id: string(item["id"]),
                name: string(item["name"]),
                path: string(item["path"]),
                sizeBytes: uint64(item["sizeBytes"]),
                modifiedMillis: int64(item["modifiedMillis"]),
                isDirectory: bool(item["isDirectory"]),
                isImage: bool(item["isImage"]),
                duplicateGroupId: string(item["duplicateGroupId"]),
                recommendedForKeepNewest: bool(item["recommendedForKeepNewest"]),
                categories: categories
            )
        }

        return ScanPayload(summary: summary, items: items, errors: errors)
    }

    private static func parseDeletePayload(_ dictionary: [AnyHashable: Any]) -> DeletePayload {
        let deletedPaths = dictionary["deletedPaths"] as? [String] ?? []
        let failureDictionaries = dictionary["failures"] as? [[AnyHashable: Any]] ?? []
        let failures = failureDictionaries.map {
            "\(string($0["path"])) - \(string($0["reason"]))"
        }
        return DeletePayload(
            deletedBytes: uint64(dictionary["deletedBytes"]),
            deletedPaths: deletedPaths,
            failures: failures
        )
    }

    private static func string(_ value: Any?) -> String {
        value as? String ?? ""
    }

    private static func bool(_ value: Any?) -> Bool {
        value as? Bool ?? false
    }

    private static func uint64(_ value: Any?) -> UInt64 {
        if let value = value as? UInt64 { return value }
        if let value = value as? NSNumber { return value.uint64Value }
        return 0
    }

    private static func int64(_ value: Any?) -> Int64 {
        if let value = value as? Int64 { return value }
        if let value = value as? NSNumber { return value.int64Value }
        return 0
    }
}
