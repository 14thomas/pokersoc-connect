#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QWidget>

#include "domain/food/SaleLine.h"
#include "domain/session/SessionMode.h"
#include "ui/qt/Style.h"

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace pokersoc::domain {
class Transaction;
}

namespace pokersoc::ui::qt {

class CountTable;
class CurrencyPad;
class ChipPad;
class MultiplierBar;
class Numpad;
class Overlay;
class PasswordOverlay;
class SessionContext;
class TileButton;
class TournamentClock;

/// Common shape of a full-window screen hosted by the main window.
class Screen : public QWidget {
    Q_OBJECT
public:
    explicit Screen(SessionContext& context, QWidget* parent = nullptr)
        : QWidget(parent), ctx_(context) {}

signals:
    /// Go back to the main window; `changed` = the session data was modified.
    void closed(bool changed = false);

protected:
    SessionContext& ctx_;
};

// ---------------------------------------------------------------------------

class BuyInScreen : public Screen {
    Q_OBJECT
public:
    BuyInScreen(SessionContext& context, std::string playerId, QWidget* parent = nullptr);

private:
    QWidget* buildTypePage();
    QWidget* buildCashPage();
    QWidget* buildAmountPage();

    void startTournament();
    void selectArc(bool isArc);
    bool proceedToCash();
    void refreshCash();
    void toAmountPage();
    void refreshAmount();
    void amountKey(const QString& key);
    void confirm();
    void process();
    void back();

    /// Cash-game default (and maximum) buy-in: remaining session cap, clamped
    /// by cash received and the per-buy-in maximum so Confirm can succeed.
    std::int64_t defaultAmountCents() const;

    std::string playerId_;
    bool tournament_;
    bool isRebuy_ = false;
    std::int64_t fixedCents_ = 0; // tournament price
    QString buyInLabel_;

    Counts cash_;
    std::vector<std::pair<std::int64_t, int>> history_;
    QString amountText_ = QStringLiteral("0.00");
    std::int64_t amountCents_ = 0;

    QStackedWidget* pages_ = nullptr;
    QWidget* typePage_ = nullptr;
    QWidget* cashPage_ = nullptr;
    QWidget* amountPage_ = nullptr;

    QLabel* typeSubtitle_ = nullptr;
    QWidget* arcPanel_ = nullptr;
    TileButton* arcButton_ = nullptr;
    TileButton* nonArcButton_ = nullptr;
    QLabel* arcPrice_ = nullptr;
    QLabel* arcType_ = nullptr;
    QLabel* nonArcPrice_ = nullptr;
    QLabel* nonArcType_ = nullptr;
    QLabel* capWarning_ = nullptr;

    CountTable* cashTable_ = nullptr;
    QWidget* tournamentInfo_ = nullptr;
    QLabel* tournamentAmount_ = nullptr;
    QLabel* tournamentType_ = nullptr;
    QLabel* tournamentSplit_ = nullptr;
    QLabel* cashTotal_ = nullptr;
    TileButton* nextButton_ = nullptr;
    CurrencyPad* pad_ = nullptr;

    CountTable* changeTable_ = nullptr;
    QWidget* amountEditButtons_ = nullptr;
    QLabel* changeDue_ = nullptr;
    QLabel* cashReceived_ = nullptr;
    QLabel* buyInAmount_ = nullptr;
    Numpad* numpad_ = nullptr;
    TileButton* confirmButton_ = nullptr;
};

// ---------------------------------------------------------------------------

class CashOutScreen : public Screen {
    Q_OBJECT
public:
    CashOutScreen(SessionContext& context, std::string playerId, QWidget* parent = nullptr);

private:
    QWidget* buildChipPage();
    QWidget* buildChangePage();
    void buildFoodOverlay();
    void buildTipOverlay();
    void buildExtraCashOverlay();
    void buildChangeOverlay();

    void refreshChips();
    void toChangePage();
    void refreshPlan();
    void refreshFoodOverlay();
    void refreshExtraCashOverlay();
    void refreshChangeOverlay();
    void confirm();

    std::string playerId_;
    Counts chips_;
    std::vector<std::pair<std::int64_t, int>> history_;
    std::vector<domain::SaleLine> food_;
    std::vector<domain::SaleLine> foodDraft_;
    std::int64_t tipCents_ = 0;
    std::int64_t tipDraft_ = 0;
    Counts extraCash_;
    Counts extraDraft_;
    std::vector<std::int64_t> extraDraftHistory_;
    std::optional<Counts> customChange_;
    Counts shownChange_;

    QStackedWidget* pages_ = nullptr;
    CountTable* chipTable_ = nullptr;
    QLabel* chipTotal_ = nullptr;
    TileButton* nextButton_ = nullptr;
    ChipPad* pad_ = nullptr;

    CountTable* changeTable_ = nullptr;
    QLabel* changeTotal_ = nullptr;
    QLabel* chipValueText_ = nullptr;
    QLabel* foodValueText_ = nullptr;
    QLabel* tipValueText_ = nullptr;
    QLabel* extraValueText_ = nullptr;
    QLabel* finalChangeText_ = nullptr;
    TileButton* confirmButton_ = nullptr;

    Overlay* foodOverlay_ = nullptr;
    QVBoxLayout* foodSelected_ = nullptr;
    QLabel* foodTotal_ = nullptr;
    Overlay* tipOverlay_ = nullptr;
    QLabel* tipDisplay_ = nullptr;
    Overlay* extraOverlay_ = nullptr;
    QLabel* extraDisplay_ = nullptr;
    Overlay* changeOverlay_ = nullptr;
    QVBoxLayout* changeList_ = nullptr;
};

// ---------------------------------------------------------------------------

class CashboxScreen : public Screen {
    Q_OBJECT
public:
    explicit CashboxScreen(SessionContext& context, QWidget* parent = nullptr);

private:
    QWidget* buildMainPage();
    QWidget* buildCountPage(bool tip);
    QWidget* buildExchangePage();
    void refreshMain();
    void refreshCount(bool tip);
    void refreshExchange();
    void saveFloat();
    void saveTip();
    void saveExchange();

    QStackedWidget* pages_ = nullptr;
    QWidget* mainPage_ = nullptr;
    QWidget* floatPage_ = nullptr;
    QWidget* tipPage_ = nullptr;
    QWidget* exchangePage_ = nullptr;
    bool changed_ = false;

    QVBoxLayout* stockRows_ = nullptr;
    QLabel* totalCash_ = nullptr;
    QLabel* floatAdded_ = nullptr;
    QLabel* tips_ = nullptr;
    QLabel* totalSales_ = nullptr;

    struct CountPage {
        Counts counts;
        std::vector<std::pair<std::int64_t, int>> history;
        CountTable* table = nullptr;
        QLabel* total = nullptr;
        CurrencyPad* pad = nullptr;
    };
    CountPage float_;
    CountPage tip_;

    Counts give_;
    Counts receive_;
    bool giveMode_ = true;
    CountTable* giveTable_ = nullptr;
    CountTable* receiveTable_ = nullptr;
    QLabel* giveTotal_ = nullptr;
    QLabel* receiveTotal_ = nullptr;
    QWidget* balanceBox_ = nullptr;
    QLabel* balance_ = nullptr;
    QPushButton* giveToggle_ = nullptr;
    QPushButton* receiveToggle_ = nullptr;
    TileButton* exchangeButton_ = nullptr;
};

// ---------------------------------------------------------------------------

class LostChipsScreen : public Screen {
    Q_OBJECT
public:
    explicit LostChipsScreen(SessionContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void showConfirmation();

    Counts chips_;
    std::vector<std::pair<std::int64_t, int>> history_; // denomination, count before
    QVBoxLayout* rows_ = nullptr;
    QLabel* total_ = nullptr;
    std::vector<std::pair<std::int64_t, QLabel*>> countLabels_;
    MultiplierBar* multipliers_ = nullptr;
    Overlay* confirm_ = nullptr;
    QLabel* confirmText_ = nullptr;
    bool changed_ = false;
};

// ---------------------------------------------------------------------------

class FoodScreen : public Screen {
    Q_OBJECT
public:
    FoodScreen(SessionContext& context, std::string playerId, QWidget* parent = nullptr);

signals:
    void openSettings();

private:
    QWidget* buildMainPage();
    QWidget* buildSelectPage();
    QWidget* buildPaymentPage();
    void refreshMain();
    void slotClicked(int slot);
    void startPayment(long long productId);
    void refreshPayment();
    void setCashMode(bool cash);
    void confirmPayment();

    std::string playerId_;
    bool changed_ = false;
    QStackedWidget* pages_ = nullptr;
    QWidget* mainPage_ = nullptr;
    QWidget* selectPage_ = nullptr;
    QWidget* paymentPage_ = nullptr;

    std::vector<TileButton*> slots_;
    QLabel* productsStat_ = nullptr;
    QLabel* slotsStat_ = nullptr;
    QLabel* totalMade_ = nullptr;
    QVBoxLayout* productList_ = nullptr;
    int selectingSlot_ = 0;

    long long productId_ = 0;
    bool cashMode_ = false;
    Counts cash_;
    Counts chips_;
    QLabel* productName_ = nullptr;
    QLabel* productPrice_ = nullptr;
    QLineEdit* playerBox_ = nullptr;
    QLabel* summary_ = nullptr;
    QLabel* changeText_ = nullptr;
    QWidget* changeBreakdown_ = nullptr;
    QVBoxLayout* changeRows_ = nullptr;
    TileButton* methodToggle_ = nullptr;
    TileButton* confirmButton_ = nullptr;
    QStackedWidget* padStack_ = nullptr;
    std::vector<std::pair<std::int64_t, QLabel*>> cashCounts_;
    std::vector<std::pair<std::int64_t, QLabel*>> chipCounts_;
    MultiplierBar* chipMultiplier_ = nullptr;
};

// ---------------------------------------------------------------------------

class PlayersScreen : public Screen {
    Q_OBJECT
public:
    explicit PlayersScreen(SessionContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void lookup();
    void setAge(bool underage);
    void importPlayers();
    void exportPlayers();
    void buildNotification();
    void showNotification(const QString& icon, const QString& title, const QString& message,
                          bool success);

    QLabel* subtitle_ = nullptr;
    QLineEdit* idBox_ = nullptr;
    QPushButton* adultButton_ = nullptr;
    QPushButton* underageButton_ = nullptr;
    QTableWidget* grid_ = nullptr;
    PasswordOverlay* password_ = nullptr;
    Overlay* notification_ = nullptr;
    QLabel* notificationIcon_ = nullptr;
    QLabel* notificationTitle_ = nullptr;
    QLabel* notificationText_ = nullptr;
    bool changed_ = false;
};

// ---------------------------------------------------------------------------

class SettingsScreen : public Screen {
    Q_OBJECT
public:
    explicit SettingsScreen(SessionContext& context, QWidget* parent = nullptr);

signals:
    /// Open another session file; `newMode` is set for a brand-new session.
    void switchSession(const QString& path, std::optional<domain::SessionModeType> newMode);
    void closeSession();

private:
    void refreshProducts();

    QTableWidget* products_ = nullptr;
    QPushButton* deleteProduct_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* price_ = nullptr;
    QLineEdit* image_ = nullptr;
    bool changed_ = false;
};

// ---------------------------------------------------------------------------

class TournamentSettingsScreen : public Screen {
    Q_OBJECT
public:
    explicit TournamentSettingsScreen(SessionContext& context, QWidget* parent = nullptr);

signals:
    void configurePricing();
    /// Rebuys or the blind structure were written.
    void structureChanged();
};

class TournamentPricingScreen : public Screen {
    Q_OBJECT
public:
    explicit TournamentPricingScreen(SessionContext& context, QWidget* parent = nullptr);
};

class PrizingScreen : public Screen {
    Q_OBJECT
public:
    explicit PrizingScreen(SessionContext& context, QWidget* parent = nullptr);

private:
    void scanPlayer();
    void clearPlayer();
    void refresh();
    void prizingChanged();
    void persistPrizing();
    void openBountyOverlay();
    void openPlaceOverlay();
    void payBounties();
    void payPlace(int place, domain::Money amount);
    void refreshPayoutPreview();
    void bountyKey(const QString& key);
    void refreshBountyPad();
    domain::Money bountyUnitForPlayer() const;
    QString idlePlayerText() const;

    std::string playerId_;
    int bountyCount_ = 0;
    int bountyMaxCount_ = 0;
    QString bountyDigits_;

    QLineEdit* idBox_ = nullptr;
    QLabel* playerText_ = nullptr;
    QWidget* redeemPanel_ = nullptr;
    QPushButton* bountyButton_ = nullptr;
    QPushButton* placeButton_ = nullptr;
    QLabel* prizePool_ = nullptr;
    QLabel* bountyPool_ = nullptr;
    QWidget* bountyPoolCard_ = nullptr;
    QLabel* bountyHint_ = nullptr;
    QComboBox* roundingBox_ = nullptr;
    QCheckBox* leftoverBox_ = nullptr;
    QLabel* payoutPreview_ = nullptr;
    QTableWidget* prizesTable_ = nullptr;
    Overlay* bountyOverlay_ = nullptr;
    QLabel* bountyCountLabel_ = nullptr;
    QLabel* bountyPayLabel_ = nullptr;
    Overlay* placeOverlay_ = nullptr;
    QVBoxLayout* placeList_ = nullptr;
};

class ComingSoonScreen : public Screen {
    Q_OBJECT
public:
    ComingSoonScreen(SessionContext& context, const QString& title, const QString& message,
                     QWidget* parent = nullptr);
};

class TournamentBoardControlScreen : public Screen {
    Q_OBJECT
public:
    TournamentBoardControlScreen(SessionContext& context, TournamentClock& clock,
                                 QWidget* parent = nullptr);

signals:
    void openBoardRequested();
    void closeBoardRequested();
    void sendBoardToTvRequested();
    void toggleBoardFullscreenRequested();

private:
    void refresh();
    void rebuildLevels();

    TournamentClock& clock_;
    QLabel* time_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* level_ = nullptr;
    QLabel* blinds_ = nullptr;
    QLabel* nextBlinds_ = nullptr;
    QLabel* pot_ = nullptr;
    QPushButton* start_ = nullptr;
    QPushButton* pause_ = nullptr;
    QPushButton* prevLevel_ = nullptr;
    QPushButton* skipLevel_ = nullptr;
    QWidget* levelsHost_ = nullptr;
    QGridLayout* levelsGrid_ = nullptr;
    std::vector<QPushButton*> levelButtons_;
};

// ---------------------------------------------------------------------------

/// Details of one activity-log entry, with an admin-protected "Delete" (undo).
class BreakdownScreen : public Screen {
    Q_OBJECT
public:
    BreakdownScreen(SessionContext& context, long long transactionId, QWidget* parent = nullptr);

private:
    void buildCashOut(const domain::Transaction& transaction, QVBoxLayout* layout);
    void buildGeneric(const domain::Transaction& transaction, QVBoxLayout* layout);

    long long id_;
    PasswordOverlay* password_ = nullptr;
};

/// Shared builder: the title + "✕" header used by the settings-style screens.
QWidget* makeScreenHeader(const QString& title, QPushButton** close, QLabel** subtitle = nullptr);

} // namespace pokersoc::ui::qt
