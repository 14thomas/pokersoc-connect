#include "ui/qt/TournamentClock.h"

#include <algorithm>

#include <QTimer>

#include "domain/session/TournamentConfig.h"

namespace pokersoc::ui::qt {

using domain::BlindLevel;
using domain::TournamentConfig;

TournamentClock::TournamentClock(QObject* parent) : QObject(parent) {
    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &TournamentClock::onTick);
}

void TournamentClock::setConfig(const TournamentConfig* config) {
    config_ = config;
    if (!armed_) {
        armed_ = true;
        levelIndex_ = 0;
        loadRemaining();
    }
    configChanged();
}

void TournamentClock::configChanged() {
    clampLevel();
    const int duration = durationSeconds(levelIndex_);
    if (remainingSeconds_ > duration) remainingSeconds_ = duration;
    if (remainingSeconds_ < 0) remainingSeconds_ = 0;
    emit changed();
}

void TournamentClock::start() {
    if (running_ || levelCount() == 0) return;
    if (remainingSeconds_ <= 0 && upcomingLevel()) goToNextLevel();
    if (remainingSeconds_ <= 0) return;
    running_ = true;
    timer_->start();
    emit changed();
}

void TournamentClock::pause() {
    if (!running_) return;
    running_ = false;
    timer_->stop();
    emit changed();
}

void TournamentClock::toggle() {
    if (running_) {
        pause();
    } else {
        start();
    }
}

void TournamentClock::goToNextLevel() {
    if (levelIndex_ + 1 >= levelCount()) return;
    ++levelIndex_;
    loadRemaining();
    emit changed();
}

void TournamentClock::goToPreviousLevel() {
    if (levelIndex_ <= 0) return;
    --levelIndex_;
    loadRemaining();
    emit changed();
}

void TournamentClock::goToLevel(int index) {
    if (index < 0 || index >= levelCount() || index == levelIndex_) return;
    levelIndex_ = index;
    loadRemaining();
    emit changed();
}

void TournamentClock::resetLevel() {
    loadRemaining();
    emit changed();
}

void TournamentClock::addSeconds(int delta) {
    remainingSeconds_ = std::max(0, remainingSeconds_ + delta);
    remainingSeconds_ = std::min(remainingSeconds_, 99 * 3600);
    emit changed();
}

int TournamentClock::levelCount() const noexcept {
    return config_ ? static_cast<int>(config_->blinds.size()) : 0;
}

QString TournamentClock::remainingText() const {
    const int minutes = remainingSeconds_ / 60;
    const int seconds = remainingSeconds_ % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

const BlindLevel* TournamentClock::currentLevel() const noexcept {
    return config_ ? config_->levelAt(levelIndex_) : nullptr;
}

const BlindLevel* TournamentClock::upcomingLevel() const noexcept {
    return config_ ? config_->levelAt(levelIndex_ + 1) : nullptr;
}

const BlindLevel* TournamentClock::activeBlinds() const noexcept {
    return config_ ? config_->activeBlinds(levelIndex_) : nullptr;
}

int TournamentClock::secondsUntilNextBreak() const noexcept {
    if (!config_) return -1;
    int seconds = remainingSeconds_;
    const int count = levelCount();
    for (int i = levelIndex_ + 1; i < count; ++i) {
        const BlindLevel* level = config_->levelAt(i);
        if (!level) break;
        if (level->isBreak) return seconds < 0 ? 0 : seconds;
        seconds += level->durationMinutes * 60;
    }
    return -1;
}

void TournamentClock::onTick() {
    if (!running_) return;
    if (remainingSeconds_ > 0) --remainingSeconds_;
    if (remainingSeconds_ <= 0) {
        if (upcomingLevel()) {
            ++levelIndex_;
            loadRemaining();
        } else {
            remainingSeconds_ = 0;
            running_ = false;
            timer_->stop();
        }
    }
    emit changed();
}

void TournamentClock::loadRemaining() {
    remainingSeconds_ = durationSeconds(levelIndex_);
}

void TournamentClock::clampLevel() {
    const int count = levelCount();
    if (count <= 0) {
        levelIndex_ = 0;
        remainingSeconds_ = 0;
        return;
    }
    if (levelIndex_ >= count) levelIndex_ = count - 1;
    if (levelIndex_ < 0) levelIndex_ = 0;
}

int TournamentClock::durationSeconds(int index) const noexcept {
    const BlindLevel* level = config_ ? config_->levelAt(index) : nullptr;
    if (!level || level->durationMinutes < 1) return 0;
    return level->durationMinutes * 60;
}

} // namespace pokersoc::ui::qt
