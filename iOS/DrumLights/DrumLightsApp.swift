import SwiftUI

@main
struct DrumLightsApp: App {
    @State private var model = AppModel()

    var body: some Scene {
        WindowGroup {
            LiveView()
                .environment(model)
                .preferredColorScheme(.dark)
                .task { model.start() }
        }
    }
}
