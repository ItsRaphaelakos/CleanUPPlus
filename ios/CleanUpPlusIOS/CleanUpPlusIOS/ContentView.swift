import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var store: CleanerStore
    @State private var showingDeleteConfirmation = false

    var body: some View {
        TabView(selection: $store.selectedTab) {
            NavigationStack {
                HomeScreen()
            }
            .tabItem { Label(CleanerTab.home.rawValue, systemImage: CleanerTab.home.icon) }
            .tag(CleanerTab.home)

            NavigationStack {
                ScanScreen()
            }
            .tabItem { Label(CleanerTab.scan.rawValue, systemImage: CleanerTab.scan.icon) }
            .tag(CleanerTab.scan)

            NavigationStack {
                ResultsScreen(showingDeleteConfirmation: $showingDeleteConfirmation)
            }
            .tabItem { Label(CleanerTab.results.rawValue, systemImage: CleanerTab.results.icon) }
            .tag(CleanerTab.results)

            NavigationStack {
                SettingsScreen()
            }
            .tabItem { Label(CleanerTab.settings.rawValue, systemImage: CleanerTab.settings.icon) }
            .tag(CleanerTab.settings)
        }
        .tint(.cyan)
        .alert("Delete selected files?", isPresented: $showingDeleteConfirmation) {
            Button("Cancel", role: .cancel) {
                store.appendLog("Delete cancelled.")
            }
            Button("Delete", role: .destructive) {
                store.deleteSelected()
            }
        } message: {
            Text("CleanUp+ will permanently delete \(store.selectedPaths.count) selected item(s). This only runs after confirmation.")
        }
    }
}

private struct HomeScreen: View {
    @EnvironmentObject private var store: CleanerStore

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Header(title: "CleanUp+", subtitle: "Storage cleaner and duplicate finder")
                StorageCard()

                Button {
                    store.scan()
                } label: {
                    Label("Scan Storage", systemImage: "magnifyingglass")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .tint(.cyan)

                Text("Cleanup targets")
                    .font(.headline)

                ForEach(CleanerCategory.allCases) { category in
                    CategoryCard(category: category)
                }

                ConsoleCard()
            }
            .padding()
        }
        .navigationTitle("Home")
        .background(Color(.systemBackground))
    }
}

private struct ScanScreen: View {
    @EnvironmentObject private var store: CleanerStore

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Header(title: "Scan Storage", subtitle: "Find accessible clutter and duplicate image files")

                Card {
                    VStack(alignment: .leading, spacing: 12) {
                        Label("iOS sandbox access", systemImage: "lock.shield.fill")
                            .font(.headline)
                            .foregroundStyle(.green)
                        Text("iOS does not allow third-party apps to scan the whole phone. This app scans Documents, temporary files and user-accessible app storage.")
                            .foregroundStyle(.secondary)
                    }
                }

                Card {
                    VStack(alignment: .leading, spacing: 10) {
                        Text("Current scan rules")
                            .font(.headline)
                        RuleRow(label: "Large files", value: "Over \(formatBytes(store.settings.largeFileThresholdBytes))")
                        RuleRow(label: "Screenshots", value: store.settings.includeScreenshots ? "Included" : "Skipped")
                        RuleRow(label: "Downloads", value: store.settings.includeDownloads ? "Included" : "Skipped")
                        RuleRow(label: "Duplicates", value: "Images grouped by size and content hash")
                    }
                }

                Button {
                    store.scan()
                } label: {
                    Label(store.isScanning ? "Scanning..." : "Scan Storage", systemImage: "magnifyingglass")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .tint(.cyan)
                .disabled(store.isScanning)

                if store.isScanning {
                    ProgressView()
                        .progressViewStyle(.linear)
                }

                ConsoleCard()
            }
            .padding()
        }
        .navigationTitle("Scan")
    }
}

private struct ResultsScreen: View {
    @EnvironmentObject private var store: CleanerStore
    @Binding var showingDeleteConfirmation: Bool

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Header(
                    title: "Results",
                    subtitle: "\(store.payload.items.count) candidates / \(formatBytes(store.potentialCleanupBytes)) potential cleanup"
                )

                ScrollView(.horizontal, showsIndicators: false) {
                    HStack {
                        FilterChip(label: "All", selected: store.selectedCategory == nil) {
                            store.selectedCategory = nil
                        }
                        ForEach(CleanerCategory.allCases) { category in
                            FilterChip(label: category.label, selected: store.selectedCategory == category) {
                                store.selectedCategory = category
                            }
                        }
                    }
                }

                HStack {
                    Button("Keep newest") {
                        store.keepNewest()
                    }
                    .buttonStyle(.bordered)
                    .disabled(!store.payload.items.contains { $0.recommendedForKeepNewest })

                    Button(role: .destructive) {
                        showingDeleteConfirmation = true
                    } label: {
                        Label("Delete Selected", systemImage: "trash.fill")
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(store.selectedPaths.isEmpty || store.isDeleting)
                }

                if store.filteredItems.isEmpty {
                    Card {
                        VStack(spacing: 12) {
                            Image(systemName: "externaldrive.fill")
                                .font(.largeTitle)
                                .foregroundStyle(.cyan)
                            Text("No results yet")
                                .font(.headline)
                            Text("Run a scan to populate cleanup candidates.")
                                .foregroundStyle(.secondary)
                        }
                        .frame(maxWidth: .infinity)
                    }
                } else {
                    LazyVStack(spacing: 12) {
                        ForEach(store.filteredItems) { item in
                            ResultRow(item: item)
                        }
                    }
                }

                if !store.payload.errors.isEmpty {
                    Card {
                        VStack(alignment: .leading, spacing: 8) {
                            Label("Scan warnings", systemImage: "exclamationmark.triangle.fill")
                                .foregroundStyle(.orange)
                                .font(.headline)
                            ForEach(store.payload.errors.prefix(4), id: \.self) { error in
                                Text(error)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }

                ConsoleCard()
            }
            .padding()
        }
        .navigationTitle("Results")
    }
}

private struct SettingsScreen: View {
    @EnvironmentObject private var store: CleanerStore

    var thresholdMb: Binding<Double> {
        Binding {
            Double(store.settings.largeFileThresholdBytes / 1024 / 1024)
        } set: { value in
            let rounded = UInt64((value / 25).rounded() * 25)
            store.settings.largeFileThresholdBytes = max(50, min(2048, rounded)) * 1024 * 1024
            store.appendLog("Large file threshold changed to \(formatBytes(store.settings.largeFileThresholdBytes)).")
        }
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Header(title: "Settings", subtitle: "Tune scan rules and appearance")

                Card {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Large file threshold")
                            .font(.headline)
                        Text(formatBytes(store.settings.largeFileThresholdBytes))
                            .font(.title2.bold())
                            .foregroundStyle(.cyan)
                        Slider(value: thresholdMb, in: 50...2048, step: 25)
                    }
                }

                Card {
                    Toggle("Include screenshots", isOn: $store.settings.includeScreenshots)
                    Toggle("Include downloads", isOn: $store.settings.includeDownloads)
                }

                Card {
                    VStack(alignment: .leading, spacing: 12) {
                        Text("Theme")
                            .font(.headline)
                        Picker("Theme", selection: $store.settings.theme) {
                            ForEach(ThemeMode.allCases) { mode in
                                Text(mode.rawValue).tag(mode)
                            }
                        }
                        .pickerStyle(.segmented)
                    }
                }

                Card {
                    Label("About CleanUp+", systemImage: "info.circle.fill")
                        .font(.headline)
                    Text("Cross-platform cleaner with shared C++ scanner core and native iOS/Android wrappers.")
                        .foregroundStyle(.secondary)
                }

                ConsoleCard()
            }
            .padding()
        }
        .navigationTitle("Settings")
    }
}

private struct ResultRow: View {
    @EnvironmentObject private var store: CleanerStore
    let item: CleanerItem

    var selected: Bool {
        store.selectedPaths.contains(item.path)
    }

    var body: some View {
        Button {
            store.toggleSelection(item)
        } label: {
            HStack(spacing: 12) {
                Image(systemName: selected ? "checkmark.circle.fill" : "circle")
                    .foregroundStyle(selected ? .cyan : .secondary)
                    .font(.title3)

                RoundedRectangle(cornerRadius: 12)
                    .fill(Color.cyan.opacity(0.12))
                    .frame(width: 52, height: 52)
                    .overlay {
                        Image(systemName: item.isDirectory ? "folder.fill" : item.isImage ? "photo.fill" : "doc.fill")
                            .foregroundStyle(.cyan)
                    }

                VStack(alignment: .leading, spacing: 5) {
                    HStack {
                        Text(item.name.isEmpty ? item.path : item.name)
                            .font(.headline)
                            .lineLimit(1)
                        Spacer()
                        Text(formatBytes(item.sizeBytes))
                            .font(.caption.bold())
                            .foregroundStyle(.cyan)
                    }
                    Text(item.path)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                    HStack {
                        ForEach(Array(item.categories), id: \.self) { category in
                            Text(category.label)
                                .font(.caption2)
                                .padding(.horizontal, 8)
                                .padding(.vertical, 4)
                                .background(Color.cyan.opacity(0.12), in: Capsule())
                        }
                    }
                }
            }
            .padding(14)
            .background(selected ? Color.cyan.opacity(0.12) : Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 20))
        }
        .buttonStyle(.plain)
    }
}

private struct CategoryCard: View {
    @EnvironmentObject private var store: CleanerStore
    let category: CleanerCategory

    private var items: [CleanerItem] {
        store.payload.items.filter { $0.categories.contains(category) }
    }

    var body: some View {
        Button {
            store.selectedCategory = category
            store.selectedTab = .results
        } label: {
            HStack {
                Image(systemName: category.icon)
                    .frame(width: 44, height: 44)
                    .background(Color.cyan.opacity(0.12), in: RoundedRectangle(cornerRadius: 14))
                    .foregroundStyle(.cyan)
                VStack(alignment: .leading) {
                    Text(category.label)
                        .font(.headline)
                    Text("\(items.count) found / \(formatBytes(items.reduce(0) { $0 + $1.sizeBytes }))")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
                Spacer()
            }
            .padding()
            .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 20))
        }
        .buttonStyle(.plain)
    }
}

private struct StorageCard: View {
    @EnvironmentObject private var store: CleanerStore

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 12) {
                Text("Accessible Storage")
                    .font(.headline)
                HStack {
                    Gauge(value: Double(store.payload.summary.scannedBytes), in: 0...max(Double(store.payload.summary.totalSpaceBytes), 1)) {
                        Text("Scanned")
                    }
                    .gaugeStyle(.accessoryCircularCapacity)
                    .tint(.cyan)

                    VStack(alignment: .leading, spacing: 6) {
                        RuleRow(label: "Total", value: formatBytes(store.payload.summary.totalSpaceBytes))
                        RuleRow(label: "Available", value: formatBytes(store.payload.summary.availableSpaceBytes))
                        RuleRow(label: "Scanned", value: formatBytes(store.payload.summary.scannedBytes))
                    }
                }
            }
        }
    }
}

private struct ConsoleCard: View {
    @EnvironmentObject private var store: CleanerStore

    var body: some View {
        Card {
            VStack(alignment: .leading, spacing: 8) {
                Text("Console")
                    .font(.headline)
                ForEach(store.logLines.suffix(6), id: \.self) { line in
                    Text("> \(line)")
                        .font(.system(.caption, design: .monospaced))
                        .foregroundStyle(.secondary)
                }
            }
        }
    }
}

private struct Header: View {
    let title: String
    let subtitle: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(.largeTitle.bold())
            Text(subtitle)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct Card<Content: View>: View {
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            content
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 22))
    }
}

private struct RuleRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .foregroundStyle(.secondary)
            Spacer()
            Text(value)
                .fontWeight(.semibold)
        }
        .font(.subheadline)
    }
}

private struct FilterChip: View {
    let label: String
    let selected: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(label)
                .font(.caption.bold())
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(selected ? Color.cyan.opacity(0.18) : Color(.secondarySystemBackground), in: Capsule())
                .overlay {
                    Capsule().stroke(selected ? Color.cyan : Color.clear, lineWidth: 1)
                }
        }
        .buttonStyle(.plain)
    }
}
