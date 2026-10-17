#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace pokersoc::domain {
struct BlindLevel;
struct TournamentConfig;
}

namespace pokersoc::ui::qt {

/// Live tournament clock: current level, remaining time, start/pause. The
/// host window and the room display share one of these.
class TournamentClock : public QObject {
    Q_OBJECT
public:
    explicit TournamentClock(QObject* parent = nullptr);

    void setConfig(const domain::TournamentConfig* config);
    /// Blinds/prizes were edited: clamp the level and remaining time.
    void configChanged();

    void start();
    void pause();
    void toggle();
    void goToNextLevel();
    void goToPreviousLevel();
    void goToLevel(int index);
    void resetLevel();
    /// Positive adds time; negative takes it away (not below zero).
    void addSeconds(int delta);

    bool running() const noexcept { return running_; }
    int levelIndex() const noexcept { return levelIndex_; }
    int levelCount() const noexcept;
    int remainingSeconds() const noexcept { return remainingSeconds_; }
    QString remainingText() const;

    const domain::BlindLevel* currentLevel() const noexcept;
    const domain::BlindLevel* upcomingLevel() const noexcept;

signals:
    void changed();

private:
    void onTick();
    void loadRemaining();
    void clampLevel();
    int durationSeconds(int index) const noexcept;

    const domain::TournamentConfig* config_ = nullptr;
    QTimer* timer_ = nullptr;
    int levelIndex_ = 0;
    int remainingSeconds_ = 0;
    bool running_ = false;
    bool armed_ = false;
};

} // namespace pokersoc::ui::qt
