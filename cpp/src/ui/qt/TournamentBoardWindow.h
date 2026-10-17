#pragma once

#include <QMainWindow>

class QFrame;
class QHBoxLayout;
class QLabel;

namespace pokersoc::ui::qt {

class SessionContext;
class TournamentClock;

/// Full-screen room display: clock, blinds, next level, prize pool and payouts.
/// The host keeps the main window for buy-ins; this is a second window, ideally
/// on the TV / projector.
class TournamentBoardWindow : public QMainWindow {
    Q_OBJECT
public:
    TournamentBoardWindow(SessionContext& context, TournamentClock& clock,
                          QWidget* parent = nullptr);

    /// Open (or move) this window onto a second screen when one is available.
    void showOnRoomDisplay(QWidget* host);
    void setBoardFullScreen(bool on);
    void refreshStats();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QFrame* makeBlindCard(const QString& caption, QLabel** value);
    void applyFonts();
    void refreshClock();
    void refreshAll();
    QFont boardFont(int pixelSize, bool bold = false) const;

    SessionContext& ctx_;
    TournamentClock& clock_;

    QLabel* title_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* level_ = nullptr;
    QLabel* time_ = nullptr;
    QLabel* smallBlind_ = nullptr;
    QLabel* bigBlind_ = nullptr;
    QLabel* ante_ = nullptr;
    QLabel* nextBlinds_ = nullptr;
    QLabel* pot_ = nullptr;
    QLabel* entries_ = nullptr;
    QLabel* rebuys_ = nullptr;
    QWidget* prizeRow_ = nullptr;
    QHBoxLayout* prizeLayout_ = nullptr;
    QLabel* hint_ = nullptr;
};

} // namespace pokersoc::ui::qt
