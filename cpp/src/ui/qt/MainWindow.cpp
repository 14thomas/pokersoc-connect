#include "ui/qt/MainWindow.h"

#include <QCloseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScreen>
#include <QShortcut>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "application/BuyInService.h"
#include "application/PlayerService.h"
#include "application/ReportService.h"
#include "domain/player/Player.h"
#include "domain/session/CashGameMode.h"
#include "domain/session/TournamentMode.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/Transaction.h"
#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Widgets.h"
#include "util/Time.h"

namespace pokersoc::ui::qt {

using namespace domain;

namespace {

constexpr int kExpiryMs = 3 * 60 * 1000;

const char* const kPending = "rgb(255,255,200)";
const char* const kVerified = "rgb(220,255,220)";
const char* const kUnderage = "rgb(255,200,200)";

/// One of the inline bordered boxes inside the "Current Player" panel.
QFrame* makeInlinePanel(const QString& background, const QString& border, int padding = 10) {
    QFrame* panel = makePanel(background, border, 2, 0, padding);
    panel->hide();
    return panel;
}

QPushButton* boldButton(const QString& text, const QString& background, int px = 12,
                        const QString& padding = QStringLiteral("5px 8px")) {
    QPushButton* button = makeButton(text, QString(), px, true);
    button->setStyleSheet(buttonStyle(background, "#707070", "1px", "padding:" + padding + ";"));
    return button;
}

QString activityType(const Transaction& transaction) {
    if (transaction.type() == TransactionType::FoodSale) return "SALE";
    if (transaction.type() == TransactionType::CashOut &&
        !static_cast<const CashOut&>(transaction).food().empty()) {
        return "CASHOUT_SALE";
    }
    return transactionTypeName(transaction.type());
}

} // namespace

MainWindow::MainWindow(std::unique_ptr<SessionContext> context, QWidget* parent)
    : QMainWindow(parent), ctx_(std::move(context)) {
    setAttribute(Qt::WA_DeleteOnClose);
    resize(1080, 720);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    stack_ = new QStackedWidget;
    layout->addWidget(stack_);
    mainPage_ = buildMainPage();
    stack_->addWidget(mainPage_);
    buildCloseOverlay();

    expiry_ = new QTimer(this);
    expiry_->setSingleShot(true);
    expiry_->setInterval(kExpiryMs);
    connect(expiry_, &QTimer::timeout, this, [this] { clearPlayer(); });

    auto* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullscreen->setContext(Qt::WindowShortcut);
    connect(fullscreen, &QShortcut::activated, this, [this] {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });

    applySessionModeUi();
    refreshActivity();
    updatePlayerButtons();
}

MainWindow::~MainWindow() {
    // Screens hold references into the session; destroy them before it.
    if (stack_) {
        while (stack_->count() > 0) {
            QWidget* page = stack_->widget(0);
            stack_->removeWidget(page);
            delete page;
        }
    }
}

void MainWindow::forceClose() {
    allowClose_ = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (allowClose_) {
        event->accept();
        return;
    }
    event->ignore();
    closeOverlay_->open();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    focusIdBoxIfEmpty();
}

// ---------------------------------------------------------------------------
// Layout

QWidget* MainWindow::buildMainPage() {
    auto* page = new QWidget;
    auto* columns = new QHBoxLayout(page);
    columns->setContentsMargins(12, 12, 12, 12);
    columns->setSpacing(12);

    auto* left = new QVBoxLayout;
    left->setSpacing(0);
    left->addWidget(buildPlayerPanel());
    left->addSpacing(15);
    activityHeader_ = makeLabel("Activity Log", 18, true);
    activityHeader_->setContentsMargins(0, 0, 0, 10);
    left->addWidget(activityHeader_);

    activity_ = new QTableWidget(0, 4);
    activity_->setHorizontalHeaderLabels({"Time", "Type", "Player", "Amount"});
    activity_->setColumnWidth(0, 120);
    activity_->setColumnWidth(1, 100);
    activity_->setColumnWidth(2, 120);
    activity_->horizontalHeader()->setStretchLastSection(true);
    activity_->horizontalHeader()->setHighlightSections(false);
    activity_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    activity_->verticalHeader()->setVisible(false);
    activity_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    activity_->verticalHeader()->setDefaultSectionSize(44);
    activity_->setFont(uiFont(16));
    activity_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activity_->setSelectionBehavior(QAbstractItemView::SelectRows);
    activity_->setSelectionMode(QAbstractItemView::SingleSelection);
    left->addWidget(activity_, 1);
    connect(activity_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = activity_->item(row, 0);
        if (!item) return;
        showScreen(new BreakdownScreen(*ctx_, item->data(Qt::UserRole).toLongLong()));
    });

    columns->addLayout(left, 2);
    columns->addWidget(buildTiles(), 3);
    return page;
}

QFrame* MainWindow::buildPlayerPanel() {
    QFrame* panel = makePanel(color::LightBlue, color::Gray, 2, 0, 10);
    QVBoxLayout* layout = panelLayout(panel);
    QLabel* title = makeLabel("Current Player (Scan ID or Press Enter)", 14, true);
    title->setContentsMargins(0, 0, 0, 5);
    layout->addWidget(title);

    auto* row = new QHBoxLayout;
    row->setSpacing(5);
    idBox_ = new QLineEdit;
    idBox_->setFont(uiFont(16));
    setIdBoxState("white", false);
    row->addWidget(idBox_, 1);
    editButton_ = boldButton("Edit", "#FFE0B2", 14, "5px 10px");
    editButton_->setMinimumWidth(56);
    editButton_->setEnabled(false);
    row->addWidget(editButton_);
    QPushButton* clear = boldButton(QStringLiteral("✕"), color::LightCoral, 16, "2px");
    clear->setFixedWidth(40);
    row->addWidget(clear);
    layout->addLayout(row);

    connect(idBox_, &QLineEdit::returnPressed, this, [this] { scanEntered(); });
    connect(idBox_, &QLineEdit::textChanged, this, [this] {
        if (!verified_) currentId_.clear();
        updatePlayerButtons();
    });
    connect(editButton_, &QPushButton::clicked, this, [this] { editMembership(); });
    connect(clear, &QPushButton::clicked, this, [this] { clearPlayer(); });

    // New player: age step, then (tournaments) membership step.
    newPlayerPanel_ = makeInlinePanel(color::LightYellow, color::Orange);
    ageStep_ = new QWidget;
    auto* age = new QVBoxLayout(ageStep_);
    age->setContentsMargins(0, 0, 0, 0);
    age->setSpacing(0);
    QLabel* detected = makeLabel(QStringLiteral("⚠️ NEW PLAYER DETECTED"), 14, true, color::DarkOrange);
    detected->setContentsMargins(0, 0, 0, 5);
    age->addWidget(detected);
    newPlayerText_ = makeLabel(QString(), 12);
    newPlayerText_->setWordWrap(true);
    newPlayerText_->setContentsMargins(0, 0, 0, 3);
    age->addWidget(newPlayerText_);
    cutoffText_ = makeLabel(QString(), 11, true, "#00008B");
    cutoffText_->setContentsMargins(0, 2, 0, 5);
    age->addWidget(cutoffText_);
    auto* ageButtons = new QHBoxLayout;
    ageButtons->setSpacing(6);
    QPushButton* adult = boldButton(QStringLiteral("✓ 18+"), color::LightGreen);
    QPushButton* minor = boldButton(QStringLiteral("👶 Underage"), "#FFFFCC");
    QPushButton* cancelNew = boldButton(QStringLiteral("✕ Cancel"), color::LightCoral);
    for (QPushButton* button : {adult, minor, cancelNew}) ageButtons->addWidget(button, 1);
    age->addLayout(ageButtons);
    panelLayout(newPlayerPanel_)->addWidget(ageStep_);
    connect(adult, &QPushButton::clicked, this, [this] { verifyAdult(); });
    connect(minor, &QPushButton::clicked, this, [this] { verifyUnderage(); });
    connect(cancelNew, &QPushButton::clicked, this, [this] { clearPlayer(); });

    membershipStep_ = new QWidget;
    auto* membership = new QVBoxLayout(membershipStep_);
    membership->setContentsMargins(0, 0, 0, 0);
    membership->setSpacing(0);
    QLabel* membershipTitle = makeLabel("Membership Type", 14, true, color::DarkOrange);
    membershipTitle->setContentsMargins(0, 0, 0, 5);
    membership->addWidget(membershipTitle);
    membershipText_ = makeLabel(QString(), 12);
    membershipText_->setWordWrap(true);
    membershipText_->setContentsMargins(0, 0, 0, 10);
    membership->addWidget(membershipText_);
    membership->addWidget(buildMembershipButtons(false));
    membership->addSpacing(10);
    QPushButton* cancelMembership = boldButton(QStringLiteral("✕ Cancel"), color::LightCoral, 14, "10px");
    membership->addWidget(cancelMembership);
    membershipStep_->hide();
    panelLayout(newPlayerPanel_)->addWidget(membershipStep_);
    connect(cancelMembership, &QPushButton::clicked, this, [this] { clearPlayer(); });
    layout->addWidget(newPlayerPanel_);

    // Edit membership of the current player.
    editPanel_ = makeInlinePanel("#FFF3E0", "#E65100");
    QLabel* editTitle = makeLabel("Edit Membership Type", 14, true, "#E65100");
    editTitle->setContentsMargins(0, 0, 0, 5);
    panelLayout(editPanel_)->addWidget(editTitle);
    editText_ = makeLabel(QString(), 12);
    editText_->setWordWrap(true);
    editText_->setContentsMargins(0, 0, 0, 10);
    panelLayout(editPanel_)->addWidget(editText_);
    panelLayout(editPanel_)->addWidget(buildMembershipButtons(true));
    panelLayout(editPanel_)->addSpacing(10);
    QPushButton* cancelEditButton = boldButton(QStringLiteral("✕ Cancel"), color::LightCoral, 14, "10px");
    panelLayout(editPanel_)->addWidget(cancelEditButton);
    connect(cancelEditButton, &QPushButton::clicked, this, [this] { cancelEdit(); });
    layout->addWidget(editPanel_);

    underagePanel_ = makeInlinePanel("#FFDDDD", color::Red);
    QLabel* underageTitle =
        makeLabel(QStringLiteral("🚫 UNDERAGE PLAYER"), 14, true, color::Red, Qt::AlignCenter);
    underageTitle->setContentsMargins(0, 0, 0, 3);
    panelLayout(underagePanel_)->addWidget(underageTitle);
    panelLayout(underagePanel_)->addWidget(
        makeLabel("Cannot buy-in or cash-out", 11, false, "#8B0000", Qt::AlignCenter));
    layout->addWidget(underagePanel_);

    infoPanel_ = makeInlinePanel("#E8F4FD", "#4A90D9", 8);
    infoLayout_ = panelLayout(infoPanel_);
    layout->addWidget(infoPanel_);

    for (QFrame* inline_ : {newPlayerPanel_, editPanel_, underagePanel_, infoPanel_}) {
        layout->setStretchFactor(inline_, 0);
    }
    return panel;
}

QWidget* MainWindow::buildMembershipButtons(bool editing) {
    auto* holder = new QWidget;
    auto* grid = new QGridLayout(holder);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);
    struct Tier {
        MembershipType type;
        const char* background;
        int row, column, span;
    };
    const Tier tiers[] = {
        {MembershipType::Bronze, "#CD7F32", 0, 0, 3},   {MembershipType::Silver, "#A8A8A8", 0, 3, 3},
        {MembershipType::Gold, "#D4AF37", 1, 0, 2},     {MembershipType::Platinum, "#6E7F8A", 1, 2, 2},
        {MembershipType::Diamond, "#4A90D9", 1, 4, 2},
    };
    for (const Tier& tier : tiers) {
        QPushButton* button = makeButton(membershipTypeLabel(tier.type), QString(), 16, true);
        button->setStyleSheet(buttonStyle(tier.background, "#707070", "1px", "color:white;"));
        button->setMinimumHeight(56);
        grid->addWidget(button, tier.row, tier.column, 1, tier.span);
        const MembershipType type = tier.type;
        connect(button, &QPushButton::clicked, this, [this, type, editing] {
            if (editing) {
                setMembership(type);
            } else {
                completeSignup(type);
            }
        });
    }
    for (int column = 0; column < 6; ++column) grid->setColumnStretch(column, 1);
    return holder;
}

QWidget* MainWindow::buildTiles() {
    auto* holder = new QWidget;
    auto* grid = new QGridLayout(holder);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    buyInTile_ = new TileButton(QStringLiteral("💵"), "Buy-in", "Record player buy-in");
    cashOutTile_ = new TileButton(QStringLiteral("💸"), "Cash-out", "Record player cash-out");
    auto* cashbox = new TileButton(QStringLiteral("💰"), "Cashbox", "Manage float and cash");
    auto* food = new TileButton(QStringLiteral("🍕"), "Food", "Sell food and drinks");
    auto* players = new TileButton(QStringLiteral("👥"), "Players", "Manage player database");
    infoTile_ = new TileButton(QStringLiteral("📊"), "Button 6", "Coming soon");
    lostChipsTile_ = new TileButton(QStringLiteral("💸"), "Lost Chips", "Record lost chips as tips");
    auto* settings = new TileButton(QStringLiteral("⚙"), "Settings", "Configure system");

    TileButton* tiles[] = {lostChipsTile_, settings, cashbox, food, players, infoTile_, buyInTile_, cashOutTile_};
    for (int i = 0; i < 8; ++i) grid->addWidget(tiles[i], i / 2, i % 2);
    makeUniform(grid, 4, 2);

    connect(buyInTile_, &QPushButton::clicked, this, [this] { openBuyIn(); });
    connect(cashOutTile_, &QPushButton::clicked, this, [this] { openCashOut(); });
    connect(cashbox, &QPushButton::clicked, this, [this] { showScreen(new CashboxScreen(*ctx_)); });
    connect(food, &QPushButton::clicked, this, [this] { openFood(); });
    connect(players, &QPushButton::clicked, this, [this] { showScreen(new PlayersScreen(*ctx_)); });
    connect(infoTile_, &QPushButton::clicked, this, [this] {
        if (ctx_->isTournament()) showScreen(new TournamentInfoScreen(*ctx_));
    });
    connect(lostChipsTile_, &QPushButton::clicked, this, [this] {
        if (ctx_->isTournament()) {
            openTournamentSettings();
        } else {
            showScreen(new LostChipsScreen(*ctx_));
        }
    });
    connect(settings, &QPushButton::clicked, this, [this] { openSettings(); });
    return holder;
}

void MainWindow::buildCloseOverlay() {
    closeOverlay_ = new Overlay(this, QColor(0, 0, 0, 0xDD));
    closeOverlay_->card()->setMaximumWidth(500);
    QVBoxLayout* card = closeOverlay_->cardLayout();
    QLabel* title = makeLabel("Close Session?", 20, true);
    title->setContentsMargins(0, 0, 0, 20);
    card->addWidget(title);
    QLabel* text = makeLabel("Are you sure you want to close the current session? Make sure you've "
                             "saved any important data.",
                             14);
    text->setWordWrap(true);
    text->setContentsMargins(0, 0, 0, 20);
    card->addWidget(text);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    QPushButton* cancel = makeButton("Cancel", QString(), 14);
    cancel->setStyleSheet("QPushButton { padding:10px 20px; }");
    QPushButton* confirm = boldButton("Close Session", color::LightCoral, 14, "10px 20px");
    buttons->addWidget(cancel);
    buttons->addSpacing(10);
    buttons->addWidget(confirm);
    card->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, closeOverlay_, &QWidget::hide);
    connect(confirm, &QPushButton::clicked, this, [this] { forceClose(); });
}

void MainWindow::applySessionModeUi() {
    if (ctx_->isTournament()) {
        setWindowTitle(QStringLiteral("pokersoc-connect — Tournament"));
        lostChipsTile_->setIconText(QStringLiteral("🏆"));
        lostChipsTile_->setTitle("Tournament Settings");
        lostChipsTile_->setSubtitle("Buy-in amounts and rebuys");
        cashOutTile_->setIconText(QStringLiteral("🚧"));
        cashOutTile_->setTitle("Coming Soon");
        cashOutTile_->setSubtitle("Tournament cash-out");
        infoTile_->setEnabled(true);
        infoTile_->setColors(color::ButtonFace, color::Gray);
        infoTile_->setFaded(false);
        infoTile_->setTitle("Tournament Info");
        refreshTournamentTile();
    } else {
        setWindowTitle(QStringLiteral("pokersoc-connect — Cash Game"));
        infoTile_->setEnabled(false);
        infoTile_->setColors(color::ButtonFace, color::LightGray);
        infoTile_->setFaded(true);
    }
}

void MainWindow::refreshTournamentTile() {
    if (!ctx_->isTournament()) return;
    const auto stats = ctx_->reports().tournamentStats();
    infoTile_->setSubtitle(
        QStringLiteral("Buy-ins: %1  ·  Rebuys: %2").arg(stats.buyIns).arg(stats.rebuys));
}

// ---------------------------------------------------------------------------
// Current player

void MainWindow::hidePlayerPanels() {
    newPlayerPanel_->hide();
    ageStep_->show();
    membershipStep_->hide();
    editPanel_->hide();
    underagePanel_->hide();
    infoPanel_->hide();
}

void MainWindow::setIdBoxState(const QString& background, bool readOnly) {
    idBox_->setReadOnly(readOnly);
    idBox_->setStyleSheet(QStringLiteral("QLineEdit { background:%1; padding:5px; }").arg(background));
}

void MainWindow::scanEntered() {
    const std::string id = toStd(idBox_->text().trimmed());
    if (id.empty()) return;
    hidePlayerPanels();
    pendingId_.clear();

    try {
        const Player* player = ctx_->players().find(id);
        if (!player) {
            pendingId_ = id;
            currentId_.clear();
            const QString qid = toQ(id);
            newPlayerText_->setText(QStringLiteral("Player ID: %1\n\nPlease verify this player is over 18 "
                                                   "years old before continuing.\nThis player will be "
                                                   "added as 'New Player'.")
                                        .arg(qid));
            membershipText_->setText(QStringLiteral("Player ID: %1\n\nSelect their membership type:").arg(qid));
            cutoffText_->setText(QStringLiteral("🎂 18+ if born on or before: %1")
                                     .arg(toQ(util::adultCutoffDate())));
            newPlayerPanel_->show();
            verified_ = false;
            underage_ = false;
            setIdBoxState(kPending, true);
        } else {
            currentId_ = id;
            verified_ = true;
            underage_ = player->isUnderage();
            setIdBoxState(underage_ ? kUnderage : kVerified, true);
            if (underage_) underagePanel_->show();
            showPlayerInfo(id);
            ctx_->players().logAttendance(id);
        }
        startExpiry();
        refreshActivity();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error", QStringLiteral("Error checking player: %1").arg(error.what()));
    }
    updatePlayerButtons();
}

void MainWindow::showPlayerInfo(const std::string& id) {
    clearLayout(infoLayout_);
    infoPanel_->hide();
    const Player* player = ctx_->players().find(id);
    if (!player) return;

    const QString arc = player->arcStatus() == ArcStatus::Arc      ? "Yes"
                        : player->arcStatus() == ArcStatus::NonArc ? "No"
                                                                   : QString();
    const std::pair<QString, QString> fields[] = {
        {"Name", toQ(player->name())},
        {"Degree", toQ(player->degree())},
        {"Year", player->studyYear() > 0 ? QString::number(player->studyYear()) : QString()},
        {"Student #", toQ(player->studentNumber())},
        {"Email", toQ(player->email())},
        {"ARC Member", arc},
        {"Membership", membershipTypeLabel(player->membership())},
    };
    bool any = false;
    for (const auto& [label, value] : fields) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty() || trimmed == "None" || trimmed == "New Player" || trimmed == "N/A") continue;
        any = true;
        QLabel* line = makeLabel(QStringLiteral("<b>%1:</b> %2").arg(label, trimmed.toHtmlEscaped()), 12);
        line->setTextFormat(Qt::RichText);
        line->setWordWrap(true);
        line->setContentsMargins(0, 1, 0, 1);
        infoLayout_->addWidget(line);
    }
    infoPanel_->setVisible(any);
}

void MainWindow::verifyAdult() {
    if (ctx_->isTournament()) {
        ageStep_->hide();
        membershipStep_->show();
        startExpiry();
        return;
    }
    completeSignup(MembershipType::None);
}

void MainWindow::completeSignup(MembershipType membership) {
    if (pendingId_.empty()) return;
    try {
        Player player(pendingId_, "New Player");
        player.setMembership(membership);
        ctx_->players().save(std::move(player));
        currentId_ = pendingId_;
        pendingId_.clear();
        verified_ = true;
        underage_ = false;
        idBox_->setText(toQ(currentId_));
        hidePlayerPanels();
        setIdBoxState(kVerified, true);
        showPlayerInfo(currentId_);
        updatePlayerButtons();
        ctx_->players().logAttendance(currentId_);
        startExpiry();
        refreshActivity();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error", QStringLiteral("Error adding player: %1").arg(error.what()));
    }
}

void MainWindow::verifyUnderage() {
    if (pendingId_.empty()) return;
    try {
        Player player(pendingId_, "New Player");
        player.setUnderage(true);
        ctx_->players().save(std::move(player));
        currentId_ = pendingId_;
        pendingId_.clear();
        verified_ = true;
        underage_ = true;
        idBox_->setText(toQ(currentId_));
        hidePlayerPanels();
        setIdBoxState(kUnderage, true);
        underagePanel_->show();
        updatePlayerButtons();
        ctx_->players().logAttendance(currentId_);
        startExpiry();
        refreshActivity();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error", QStringLiteral("Error adding player: %1").arg(error.what()));
    }
}

void MainWindow::editMembership() {
    if (currentId_.empty() || !verified_) return;
    const Player* player = ctx_->players().find(currentId_);
    const MembershipType current = player ? player->membership() : MembershipType::None;
    const QString qid = toQ(currentId_);
    editText_->setText(current == MembershipType::None
                           ? QStringLiteral("Player ID: %1\n\nSelect their membership type:").arg(qid)
                           : QStringLiteral("Player ID: %1\nCurrent: %2\n\nSelect a new membership type:")
                                 .arg(qid, membershipTypeLabel(current)));
    infoPanel_->hide();
    underagePanel_->hide();
    newPlayerPanel_->hide();
    editPanel_->show();
    startExpiry();
}

void MainWindow::setMembership(MembershipType membership) {
    if (currentId_.empty() || !verified_) return;
    ctx_->players().setMembership(currentId_, membership);
    editPanel_->hide();
    showPlayerInfo(currentId_);
    updatePlayerButtons();
    startExpiry();
}

void MainWindow::cancelEdit() {
    editPanel_->hide();
    if (!currentId_.empty() && verified_) {
        if (underage_) {
            underagePanel_->show();
        } else {
            showPlayerInfo(currentId_);
        }
    }
    startExpiry();
}

void MainWindow::clearPlayer() {
    expiry_->stop();
    currentId_.clear();
    pendingId_.clear();
    verified_ = false;
    underage_ = false;
    setIdBoxState("white", false);
    idBox_->clear();
    hidePlayerPanels();
    idBox_->setFocus();
    updatePlayerButtons();
    refreshActivity();
}

void MainWindow::updatePlayerButtons() {
    const bool hasVerified = !currentId_.empty() && verified_ && !underage_;
    bool canBuyIn = hasVerified;
    if (canBuyIn && ctx_->isTournament() && ctx_->tournament().config().usesMembershipPricing()) {
        const Player* player = ctx_->players().find(currentId_);
        canBuyIn = player && player->membership() != MembershipType::None;
    }
    if (canBuyIn && !ctx_->isTournament()) {
        const BuyInHistory history = ctx_->buyIns().history(currentId_);
        canBuyIn = ctx_->cashGame().config().hasRoom(history);
    }
    buyInTile_->setEnabled(canBuyIn);
    cashOutTile_->setEnabled(hasVerified);
    editButton_->setEnabled(!currentId_.empty() && verified_);
}

void MainWindow::startExpiry() { expiry_->start(); }

void MainWindow::focusIdBoxIfEmpty() {
    if (stack_->currentWidget() == mainPage_ && !verified_) idBox_->setFocus();
}

// ---------------------------------------------------------------------------
// Activity log and screens

void MainWindow::refreshActivity() {
    const bool filter = verified_ && !currentId_.empty();
    const auto entries = ctx_->reports().activity(filter ? currentId_ : std::string());
    activityHeader_->setText(filter ? QStringLiteral("Activity Log — %1").arg(toQ(currentId_))
                                    : QStringLiteral("Activity Log"));
    activity_->setRowCount(static_cast<int>(entries.size()));
    int row = 0;
    for (const Transaction* transaction : entries) {
        auto* time = new QTableWidgetItem(toQ(util::utcToLocal(transaction->time(), "%H:%M:%S")));
        time->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(transaction->id()));
        activity_->setItem(row, 0, time);
        activity_->setItem(row, 1, new QTableWidgetItem(activityType(*transaction)));
        activity_->setItem(row, 2, new QTableWidgetItem(toQ(transaction->playerId())));
        activity_->setItem(row, 3, new QTableWidgetItem(money(transaction->amount())));
        ++row;
    }
    refreshTournamentTile();
}

void MainWindow::showScreen(Screen* screen) {
    closeOverlay_->hide();
    stack_->addWidget(screen);
    stack_->setCurrentWidget(screen);
    connect(screen, &Screen::closed, this, [this, screen](bool changed) {
        if (stack_->indexOf(screen) < 0) return;
        stack_->setCurrentWidget(mainPage_);
        stack_->removeWidget(screen);
        screen->deleteLater();
        showMain(changed);
    });
}

void MainWindow::showMain(bool) {
    if (verified_ && !currentId_.empty()) {
        if (const Player* player = ctx_->players().find(currentId_)) {
            underage_ = player->isUnderage();
            setIdBoxState(underage_ ? kUnderage : kVerified, true);
            underagePanel_->setVisible(underage_ && !editPanel_->isVisible());
            showPlayerInfo(currentId_);
        }
    }
    refreshActivity();
    updatePlayerButtons();
    focusIdBoxIfEmpty();
}

void MainWindow::openBuyIn() {
    if (ctx_->isTournament()) {
        if (currentId_.empty()) {
            QMessageBox::information(this, "pokersoc-connect",
                                     "Please scan a player before recording a tournament buy-in.");
            return;
        }
        const Player* player = ctx_->players().find(currentId_);
        if (ctx_->tournament().config().usesMembershipPricing() &&
            (!player || player->membership() == MembershipType::None)) {
            QMessageBox::warning(this, "Membership Required",
                                 "This player has no membership type set.\n\nUse Edit on the current "
                                 "player to set their membership before buying in.");
            return;
        }
    }
    showScreen(new BuyInScreen(*ctx_, currentId_));
}

void MainWindow::openCashOut() {
    if (ctx_->isTournament()) {
        showScreen(new ComingSoonScreen(*ctx_, "Coming Soon", "Tournament cash-out is not available yet."));
        return;
    }
    showScreen(new CashOutScreen(*ctx_, currentId_));
}

void MainWindow::openFood() {
    auto* screen = new FoodScreen(*ctx_, currentId_);
    connect(screen, &FoodScreen::openSettings, this, [this, screen] {
        emit screen->closed(true);
        openSettings();
    });
    showScreen(screen);
}

void MainWindow::openSettings() {
    auto* screen = new SettingsScreen(*ctx_);
    connect(screen, &SettingsScreen::switchSession, this,
            [this](const QString& path, std::optional<SessionModeType> mode) {
                // Queued so the settings screen's click handler has unwound first.
                QTimer::singleShot(0, this, [this, path, mode] { emit switchSessionRequested(path, mode); });
            });
    connect(screen, &SettingsScreen::closeSession, this,
            [this] { QTimer::singleShot(0, this, [this] { forceClose(); }); });
    showScreen(screen);
}

void MainWindow::openTournamentSettings() {
    auto* screen = new TournamentSettingsScreen(*ctx_);
    connect(screen, &TournamentSettingsScreen::configurePricing, this, [this, screen] {
        emit screen->closed(false);
        openTournamentPricing();
    });
    showScreen(screen);
}

void MainWindow::openTournamentPricing() {
    auto* screen = new TournamentPricingScreen(*ctx_);
    showScreen(screen);
    connect(screen, &Screen::closed, this, [this] { openTournamentSettings(); });
}

} // namespace pokersoc::ui::qt
