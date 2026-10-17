#pragma once

#include <memory>
#include <optional>
#include <string>

#include <QMainWindow>
#include <QPointer>

#include "domain/player/MembershipType.h"
#include "domain/session/SessionMode.h"

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;
class QVBoxLayout;

namespace pokersoc::ui::qt {

class Overlay;
class Screen;
class SessionContext;
class TileButton;
class TournamentBoardWindow;
class TournamentClock;

/// The session window: current player + activity log on the left, the eight
/// big action tiles on the right. Every other screen replaces this page until
/// it closes (the WPF "ScreenHost").
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(std::unique_ptr<SessionContext> context, QWidget* parent = nullptr);
    ~MainWindow() override;

    SessionContext& context() noexcept { return *ctx_; }
    /// Close without the "Close Session?" confirmation.
    void forceClose();

signals:
    /// Settings asked to open another session file (`newMode` set = brand new).
    void switchSessionRequested(const QString& path, std::optional<domain::SessionModeType> newMode);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildMainPage();
    QFrame* buildClockBar();
    QFrame* buildPlayerPanel();
    QWidget* buildTiles();
    void buildCloseOverlay();
    QWidget* buildMembershipButtons(bool editing);

    void applySessionModeUi();
    void refreshTournamentTile();
    void refreshClockBar();
    void openTournamentBoard();
    void openBoardControls();

    // ---- current player ----
    void scanEntered();
    void showPlayerInfo(const std::string& id);
    void verifyAdult();
    void verifyUnderage();
    void completeSignup(domain::MembershipType membership);
    void editMembership();
    void setMembership(domain::MembershipType membership);
    void cancelEdit();
    void clearPlayer();
    void hidePlayerPanels();
    void setIdBoxState(const QString& background, bool readOnly);
    void updatePlayerButtons();
    void startExpiry();
    void focusIdBoxIfEmpty();

    // ---- activity / screens ----
    void refreshActivity();
    void showScreen(Screen* screen);
    void showMain(bool changed);
    void openBuyIn();
    void openCashOut();
    void openFood();
    void openSettings();
    void openTournamentSettings();
    void openTournamentPricing();

    std::unique_ptr<SessionContext> ctx_;
    bool allowClose_ = false;

    std::string currentId_;
    std::string pendingId_;
    bool verified_ = false;
    bool underage_ = false;
    QTimer* expiry_ = nullptr;

    QStackedWidget* stack_ = nullptr;
    QWidget* mainPage_ = nullptr;

    QLineEdit* idBox_ = nullptr;
    QPushButton* editButton_ = nullptr;
    QFrame* newPlayerPanel_ = nullptr;
    QWidget* ageStep_ = nullptr;
    QWidget* membershipStep_ = nullptr;
    QLabel* newPlayerText_ = nullptr;
    QLabel* cutoffText_ = nullptr;
    QLabel* membershipText_ = nullptr;
    QFrame* editPanel_ = nullptr;
    QLabel* editText_ = nullptr;
    QFrame* underagePanel_ = nullptr;
    QFrame* infoPanel_ = nullptr;
    QVBoxLayout* infoLayout_ = nullptr;

    QLabel* activityHeader_ = nullptr;
    QTableWidget* activity_ = nullptr;

    TileButton* buyInTile_ = nullptr;
    TileButton* cashOutTile_ = nullptr;
    TileButton* infoTile_ = nullptr;
    TileButton* lostChipsTile_ = nullptr;

    Overlay* closeOverlay_ = nullptr;

    TournamentClock* clock_ = nullptr;
    QPointer<TournamentBoardWindow> board_;
    QFrame* clockBar_ = nullptr;
    QLabel* clockLevel_ = nullptr;
    QLabel* clockTime_ = nullptr;
    QLabel* clockBlinds_ = nullptr;
    QPushButton* clockStart_ = nullptr;
    QPushButton* clockPause_ = nullptr;
    QPushButton* boardButton_ = nullptr;
};

} // namespace pokersoc::ui::qt
