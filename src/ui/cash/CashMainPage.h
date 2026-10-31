#pragma once

#include <string>

#include <QFrame>
#include <QHash>
#include <QWidget>

class QLabel;
class QLineEdit;
class QMouseEvent;
class QPushButton;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace pokersoc::ui {

class PlayerSwipe;

/// Clickable frame. QPushButton ignores child layouts, so anything with more
/// than an icon and one line of text uses this instead.
class TapFrame : public QFrame {
    Q_OBJECT
public:
    explicit TapFrame(QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool pressed_ = false;
};

} // namespace pokersoc::ui

namespace pokersoc::ui {

class SessionContext;

/// Cash-game home page: scanner, activity, and the six actions.
/// Tournament sessions keep the older main page.
class CashMainPage : public QWidget {
    Q_OBJECT
public:
    explicit CashMainPage(SessionContext& context, QWidget* parent = nullptr);

    void focusScanner();
    void refresh();
    /// Call after a buy-in, rebuy, or cash-out has been saved.
    void afterMoneyTransaction();
    const std::string& loadedId() const { return playerId_; }

signals:
    void openPlayers();
    void openLostChips();
    void openShop();
    void openCashbox();
    void openSettings();
    void openBuyIn(const QString& playerId);
    void openCashOut(const QString& playerId);
    void openEntry(qlonglong transactionId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    struct Action {
        TapFrame* button = nullptr;
        QLabel* icon = nullptr;
        QLabel* title = nullptr;
        QLabel* sub = nullptr;
    };

    void buildTopBar(QVBoxLayout* root);
    void buildActivity(QWidget* host);
    void buildPlayerCard(QWidget* host);
    void buildTiles(QVBoxLayout* column);
    void buildActions(QVBoxLayout* column);
    void buildScanner();
    Action makeAction(const QString& iconName);

    void lookup(const QString& rawId);
    void clearPlayer();
    void ensureRecord();
    void chooseMembership();
    void showKeypad();
    void showIdCheck(const std::string& id);
    void resolveIdCheck(bool adult);
    void closeOverlay();
    void placeHandle();
    bool loadedMinor() const;
    void rebuildPlayer();
    void rebuildList();
    void updateHeader();
    void updateSummary();
    void updateActions();
    void updateAttendance();
    void updateClock();
    void showUndo(const class QString& html);
    void tickUndo();
    void askUndoPassword();
    void undoLast();

    bool knownPlayer() const;
    const class QString displayName() const;

    SessionContext& ctx_;
    std::string playerId_;
    std::string pendingId_;
    int filter_ = 0;
    long long undoId_ = 0;
    int undoLeft_ = 0;
    bool holdUndo_ = false;
    bool overlayOpen_ = false;
    bool idCheck_ = false;

    QLineEdit* scanner_ = nullptr;
    QLabel* attendanceValue_ = nullptr;
    QLabel* clockTime_ = nullptr;
    QLabel* clockPeriod_ = nullptr;
    QTimer* clockTimer_ = nullptr;
    QTimer* undoTimer_ = nullptr;

    QLabel* activityCount_ = nullptr;
    QWidget* filterRow_ = nullptr;
    QPushButton* filterButtons_[5] = {};
    TapFrame* playerChip_ = nullptr;
    QLabel* playerChipText_ = nullptr;

    QWidget* summary_ = nullptr;
    QLabel* boughtValue_ = nullptr;
    QLabel* boughtSub_ = nullptr;
    QLabel* cashedValue_ = nullptr;
    QLabel* cashedSub_ = nullptr;
    QLabel* sittingValue_ = nullptr;
    QLabel* sittingSub_ = nullptr;

    QVBoxLayout* listLayout_ = nullptr;
    QHash<qlonglong, TapFrame*> logRows_;
    QWidget* toastWrap_ = nullptr;
    QLabel* toastText_ = nullptr;
    QPushButton* undoButton_ = nullptr;

    QFrame* playerCard_ = nullptr;
    QFrame* swipeHandle_ = nullptr;
    PlayerSwipe* swipe_ = nullptr;
    QStackedWidget* playerStack_ = nullptr;
    QVBoxLayout* knownLayout_ = nullptr;

    Action buy_;
    Action cash_;
    QWidget* dim_ = nullptr;
    QFrame* idDialog_ = nullptr;
};

} // namespace pokersoc::ui
