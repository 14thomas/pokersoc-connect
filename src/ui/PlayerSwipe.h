#pragma once

#include <functional>

#include <QObject>
#include <QPoint>

class QEvent;
class QLabel;
class QWidget;

namespace pokersoc::ui {

/// Upward drag that clears the loaded player. Shared by the tournament card
/// and the cash-game card so both use the same threshold and ghost animation.
class PlayerSwipe : public QObject {
    Q_OBJECT
public:
    PlayerSwipe(QWidget* card, std::function<bool()> occupied, std::function<void()> clear,
                QObject* parent = nullptr);

    /// Listen on this widget and its current children. Buttons are left alone
    /// so a tap on Clear still clicks.
    void watch(QWidget* root);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void begin();
    void finish(bool dismiss);

    QWidget* card_ = nullptr;
    std::function<bool()> occupied_;
    std::function<void()> clear_;
    QLabel* ghost_ = nullptr;
    bool tracking_ = false;
    bool dragging_ = false;
    QPoint start_;
    QPoint origin_;
};

} // namespace pokersoc::ui
