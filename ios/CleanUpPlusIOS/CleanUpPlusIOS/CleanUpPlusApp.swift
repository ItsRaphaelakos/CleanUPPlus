import SwiftUI

@main
struct CleanUpPlusApp: App {
    @StateObject private var store = CleanerStore()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(store)
                .preferredColorScheme(store.settings.theme.colorScheme)
        }
    }
}
