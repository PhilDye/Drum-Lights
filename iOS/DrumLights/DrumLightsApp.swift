import SwiftUI

@main
struct DrumLightsApp: App {
    @State private var model: AppModel

    init() {
        // start at launch, not from a view: when iOS relaunches the app in the background
        // to restore its Bluetooth connection there is no window, and the central manager
        // must be recreated with its restore identifier straight away
        let model = AppModel()
        model.start()
        _model = State(initialValue: model)
    }

    var body: some Scene {
        WindowGroup {
            LiveView()
                .environment(model)
                .preferredColorScheme(.dark)
        }
    }
}
