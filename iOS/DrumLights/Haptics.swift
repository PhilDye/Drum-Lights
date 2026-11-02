import DrumLightsKit
import UIKit

/// Plays the controller's feedback as haptics, when Settings allows
@MainActor
final class Haptics: FeedbackPlayer {
    private let settings: AppSettings
    private let light = UIImpactFeedbackGenerator(style: .light)
    private let heavy = UIImpactFeedbackGenerator(style: .heavy)
    private let notification = UINotificationFeedbackGenerator()

    init(settings: AppSettings) {
        self.settings = settings
    }

    func play(_ event: FeedbackEvent) {
        guard settings.hapticsEnabled else { return }
        switch event {
        case .acknowledged:
            light.impactOccurred()
        case .notAcknowledged:
            notification.notificationOccurred(.warning)
        case .strobe:
            heavy.impactOccurred()
        case .holdCompleted:
            heavy.impactOccurred(intensity: 1)
        case .reconnected:
            notification.notificationOccurred(.success)
        }
        // keep the Taptic Engine warm for the next tap
        light.prepare()
        heavy.prepare()
    }
}
