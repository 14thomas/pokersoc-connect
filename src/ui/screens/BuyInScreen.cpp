#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "application/BuyInService.h"
#include "application/PlayerService.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/player/Player.h"
#include "domain/session/CashGameMode.h"
#include "domain/session/TournamentMode.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

using namespace domain;

namespace {

QString playerName(SessionContext& ctx, const std::string& id) {
    const Player* player = ctx.players().find(id);
    if (!player) return "Unknown";
    return player->name().empty() ? QStringLiteral("New Player") : toQ(player->name());
}

CountTable::Look changeLook() {
    CountTable::Look look;
    look.rowBg = "#DCFFDC";
    look.rowBorder = color::Gray;
    look.countColor = color::Green;
    look.placeholderBg = color::White;
    look.placeholderBorderWidth = "0px";
    look.placeholderPx = 14;
    return look;
}

} // namespace

BuyInScreen::BuyInScreen(SessionContext& context, std::string playerId, QWidget* parent)
    : Screen(context, parent), playerId_(std::move(playerId)), tournament_(context.isTournament()) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget;
    layout->addWidget(pages_);

    typePage_ = buildTypePage();
    cashPage_ = buildCashPage();
    amountPage_ = buildAmountPage();
    pages_->addWidget(typePage_);
    pages_->addWidget(cashPage_);
    pages_->addWidget(amountPage_);
    refreshCash();

    if (tournament_) {
        nextButton_->setSubtitle("Review and confirm");
        pages_->setCurrentWidget(typePage_);
        // Deferred so a "can't buy in" result can close the screen once it is shown.
        QTimer::singleShot(0, this, [this] { startTournament(); });
    } else {
        pages_->setCurrentWidget(cashPage_);
    }
}

// ---------------------------------------------------------------------------
// Pages

QWidget* BuyInScreen::buildTypePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(0);

    layout->addWidget(makeLabel("Tournament Buy-in", 24, true));
    layout->addSpacing(8);
    typeSubtitle_ = makeLabel("Select buy-in type", 14, false, color::Gray);
    layout->addWidget(typeSubtitle_);
    layout->addSpacing(20);
    layout->addStretch(1);

    arcPanel_ = new QWidget;
    auto* row = new QHBoxLayout(arcPanel_);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    const auto makeTypeButton = [](const QString& title, const QString& background,
                                   const QString& priceColor, QLabel** price, QLabel** type) {
        auto* button = new TileButton(QString(), title);
        button->setSizes(36, 22, 10);
        button->setColors(background);
        button->layout()->setContentsMargins(20, 30, 20, 30);
        *price = button->addLine(QString(), 28, true, priceColor, 8);
        *type = button->addLine(QString(), 12, false, "rgba(0,0,0,178)", 6);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        return button;
    };
    arcButton_ = makeTypeButton("ARC MEMBER", "#E8F4FD", "#2E6DA4", &arcPrice_, &arcType_);
    nonArcButton_ =
        makeTypeButton("NON-ARC MEMBER", "#FFF8E8", "#B8860B", &nonArcPrice_, &nonArcType_);
    row->addWidget(arcButton_, 1);
    row->addWidget(nonArcButton_, 1);
    connect(arcButton_, &QPushButton::clicked, this, [this] { selectArc(true); });
    connect(nonArcButton_, &QPushButton::clicked, this, [this] { selectArc(false); });
    layout->addWidget(arcPanel_);
    arcPanel_->hide();

    layout->addStretch(1);
    capWarning_ = makeLabel(QString(), 14, true, color::Red, Qt::AlignCenter);
    capWarning_->setWordWrap(true);
    capWarning_->hide();
    layout->addWidget(capWarning_);
    layout->addSpacing(20);

    QPushButton* back = makeButton("Back to Main");
    back->setStyleSheet("padding: 10px 16px;");
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });
    layout->addWidget(back, 0, Qt::AlignLeft);
    return page;
}

QWidget* BuyInScreen::buildCashPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Cash Received"));
    cashTable_ = new CountTable({"Denomination", "Count", "Value"});
    left->addWidget(cashTable_, 1);

    QFrame* playerBox = makePlayerBox(toQ(playerId_), playerName(ctx_, playerId_));
    QFrame* info = makePanel("#E8F4FD", "#4A90D9", 2, 0, 8);
    QVBoxLayout* infoLayout = panelLayout(info);
    infoLayout->addWidget(makeLabel("Tournament Buy-in", 12, true));
    infoLayout->addSpacing(5);
    auto* amountRow = new QHBoxLayout;
    QLabel* amountCaption = makeLabel("Amount: ", 12, true);
    amountCaption->setFixedWidth(100);
    tournamentAmount_ = makeLabel(QString(), 14, true);
    amountRow->addWidget(amountCaption);
    amountRow->addWidget(tournamentAmount_, 1);
    infoLayout->addLayout(amountRow);
    tournamentType_ = makeLabel(QString(), 12, false, "rgba(0,0,0,204)");
    tournamentType_->setContentsMargins(0, 4, 0, 0);
    infoLayout->addWidget(tournamentType_);
    tournamentSplit_ = makeLabel(QString(), 12, false, "#2E6DA4");
    tournamentSplit_->setWordWrap(true);
    tournamentSplit_->setContentsMargins(0, 6, 0, 0);
    infoLayout->addWidget(tournamentSplit_);
    tournamentSplit_->hide();
    panelLayout(playerBox)->addSpacing(8);
    panelLayout(playerBox)->addWidget(info);
    tournamentInfo_ = info;
    info->hide();
    left->addWidget(playerBox);

    auto* clear = new TileButton(QStringLiteral("🗑️"), "Clear", "All cash");
    clear->setColors(color::Orange, color::Gray, "0px 1px 1px 1px");
    auto* undo = new TileButton(QStringLiteral("↶"), "Undo", "Last action");
    undo->setColors(color::Yellow, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(clear, undo));
    connect(clear, &QPushButton::clicked, this, [this] {
        if (tournament_ && fixedCents_ == 0) return;
        cash_.clear();
        history_.clear();
        refreshCash();
    });
    connect(undo, &QPushButton::clicked, this, [this] {
        if (history_.empty() || (tournament_ && fixedCents_ == 0)) return;
        const auto [cents, qty] = history_.back();
        history_.pop_back();
        cash_[cents] = std::max(0, cash_[cents] - qty);
        refreshCash();
    });

    left->addWidget(makeInfoTile(QStringLiteral("💰"), "Total Cash", &cashTotal_, color::LightGreen));

    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral, color::Gray, "0px 1px 1px 1px");
    nextButton_ = new TileButton(QStringLiteral("➡️"), "Next", "Set buy-in amount");
    nextButton_->setColors(color::LightBlue, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(back, nextButton_));
    connect(back, &QPushButton::clicked, this, [this] { this->back(); });
    connect(nextButton_, &QPushButton::clicked, this, [this] { toAmountPage(); });
    for (QWidget* tile : {static_cast<QWidget*>(clear), static_cast<QWidget*>(undo),
                          static_cast<QWidget*>(back), static_cast<QWidget*>(nextButton_)}) {
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    pad_ = new CurrencyPad;
    connect(pad_, &CurrencyPad::denomClicked, this, [this](std::int64_t cents) {
        if (tournament_ && fixedCents_ == 0) return;
        const int qty = pad_->multiplier();
        cash_[cents] += qty;
        history_.emplace_back(cents, qty);
        refreshCash();
    });
    columns.right->addWidget(pad_);
    return page;
}

QWidget* BuyInScreen::buildAmountPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Change to Give"));
    changeTable_ = new CountTable({"Denomination", "Count", "Value"}, changeLook());
    left->addWidget(changeTable_, 1);
    left->addWidget(makePlayerBox(toQ(playerId_), playerName(ctx_, playerId_)));

    auto* clear = new TileButton(QStringLiteral("🗑️"), "Clear", "Set to $0");
    clear->setColors(color::Orange, color::Gray, "1px");
    auto* reset = new TileButton(QStringLiteral("↺"), "Reset", "Full amount");
    reset->setColors(color::Yellow, color::Gray, "1px");
    amountEditButtons_ = new QWidget;
    auto* editRow = makePair(clear, reset);
    editRow->setContentsMargins(0, 0, 0, 0);
    amountEditButtons_->setLayout(editRow);
    left->addWidget(amountEditButtons_);
    connect(clear, &QPushButton::clicked, this, [this] {
        amountCents_ = 0;
        amountText_ = "0.00";
        refreshAmount();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        amountCents_ = defaultAmountCents();
        amountText_ = QString::number(amountCents_ / 100.0, 'f', 2);
        refreshAmount();
    });

    left->addWidget(makeInfoTile(QStringLiteral("🔄"), "Change Due", &changeDue_, color::LightGreen,
                                 color::Gray, "1px"));

    auto* back = new TileButton(QStringLiteral("⬅️"), "Back", "Edit cash");
    back->setColors(color::LightCoral, color::Gray, "1px");
    confirmButton_ = new TileButton(QStringLiteral("✅"), "Confirm", "Process buy-in");
    confirmButton_->setColors(color::LightGreen, color::Gray, "1px");
    left->addLayout(makePair(back, confirmButton_));
    connect(back, &QPushButton::clicked, this, [this] { pages_->setCurrentWidget(cashPage_); });
    connect(confirmButton_, &QPushButton::clicked, this, [this] { confirm(); });
    for (QWidget* tile : {static_cast<QWidget*>(clear), static_cast<QWidget*>(reset),
                          static_cast<QWidget*>(back), static_cast<QWidget*>(confirmButton_)}) {
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    QVBoxLayout* right = columns.right;
    right->setSpacing(0);
    right->addWidget(makeHeading("Buy-in Amount"));
    right->addSpacing(10);
    QFrame* display = makePanel(color::LightYellow, color::Gray, 2, 5, 20);
    QVBoxLayout* displayLayout = panelLayout(display);
    displayLayout->addWidget(makeLabel("Cash Received:", 14, false, "rgba(0,0,0,178)", Qt::AlignCenter));
    cashReceived_ = makeLabel("$0.00", 24, true, QString(), Qt::AlignCenter);
    displayLayout->addWidget(cashReceived_);
    displayLayout->addSpacing(10);
    displayLayout->addWidget(makeLabel("Buy-in Amount:", 14, false, "rgba(0,0,0,178)", Qt::AlignCenter));
    buyInAmount_ = makeLabel("$0.00", 32, true, color::Blue, Qt::AlignCenter);
    displayLayout->addWidget(buyInAmount_);
    right->addWidget(display);
    right->addSpacing(20);

    numpad_ = new Numpad;
    connect(numpad_, &Numpad::pressed, this, [this](const QString& key) { amountKey(key); });
    right->addWidget(numpad_, 1);
    right->addStretch(0);
    return page;
}

// ---------------------------------------------------------------------------
// Tournament type selection

void BuyInScreen::startTournament() {
    const TournamentConfig& config = ctx_.tournament().config();
    const BuyInHistory history = ctx_.buyIns().history(playerId_);
    isRebuy_ = history.count > 0;
    const bool canBuyIn = config.canBuyIn(history.count);
    const QString cap = config.rebuyCap <= 0 ? QStringLiteral("unlimited")
                                             : QString::number(config.rebuyCap);
    const QString capMessage =
        QStringLiteral("Rebuy limit reached (%1 rebuy cap). No further buy-ins allowed.").arg(cap);

    if (config.usesMembershipPricing()) {
        // Membership pricing uses the player's stored tier — no selection screen.
        arcPanel_->hide();
        if (!canBuyIn) {
            capWarning_->setText(capMessage);
            capWarning_->show();
            return;
        }
        const Player* player = ctx_.players().find(playerId_);
        if (!player || player->membership() == MembershipType::None) {
            QMessageBox::warning(this, "Membership Required",
                                 "This player has no membership type set.\n\nUse Edit on the "
                                 "current player to set Bronze, Silver, Gold, Platinum, or Diamond "
                                 "before buying in.");
            emit closed(false);
            return;
        }
        if (!proceedToCash()) emit closed(false);
        return;
    }

    typeSubtitle_->setText(isRebuy_ ? QStringLiteral("Rebuy #%1 — select member type").arg(history.count)
                                    : QStringLiteral("First buy-in — select member type"));
    arcPanel_->show();

    const QString typeLabel = isRebuy_ ? "Rebuy" : "Initial buy-in";
    arcPrice_->setText(money(isRebuy_ ? config.arcRebuyCents : config.arcBuyInCents));
    arcType_->setText(typeLabel);
    nonArcPrice_->setText(money(isRebuy_ ? config.nonArcRebuyCents : config.nonArcBuyInCents));
    nonArcType_->setText(typeLabel);

    nonArcButton_->setVisible(!config.arcOnly);
    arcButton_->setEnabled(canBuyIn);
    nonArcButton_->setEnabled(canBuyIn && !config.arcOnly);

    if (!canBuyIn) {
        capWarning_->setText(capMessage);
        capWarning_->show();
        return;
    }
    capWarning_->hide();
    if (config.arcOnly) selectArc(true);
}

void BuyInScreen::selectArc(bool isArc) {
    const Player* player = ctx_.players().find(playerId_);
    if (!player) return;
    const ArcStatus status = isArc ? ArcStatus::Arc : ArcStatus::NonArc;
    if (player->arcStatus() != status) {
        Player updated = *player;
        updated.setArcStatus(status);
        ctx_.players().save(std::move(updated));
    }
    proceedToCash();
}

bool BuyInScreen::proceedToCash() {
    const Player* player = ctx_.players().find(playerId_);
    if (!player) return false;
    const BuyInQuote quote = ctx_.buyIns().preview(*player, Money{});
    if (!quote.allowed) {
        QMessageBox::warning(this, "Buy-in", toQ(quote.message));
        return false;
    }
    fixedCents_ = quote.amount.cents();
    buyInLabel_ = toQ(quote.label);
    tournamentAmount_->setText(money(quote.amount));
    tournamentType_->setText(buyInLabel_);
    tournamentSplit_->setText(QStringLiteral("Prize %1    ·    Bounty %2    ·    Rake %3")
                                  .arg(money(quote.prize.isZero() && quote.bounty.isZero() &&
                                                     quote.rake.isZero()
                                                 ? quote.amount
                                                 : quote.prize),
                                       money(quote.bounty), money(quote.rake)));
    tournamentSplit_->show();
    tournamentInfo_->show();

    cash_.clear();
    history_.clear();
    pad_->resetMultiplier();
    refreshCash();
    pages_->setCurrentWidget(cashPage_);
    return true;
}

// ---------------------------------------------------------------------------
// Cash and amount

void BuyInScreen::refreshCash() {
    const bool freeroll = tournament_ && fixedCents_ == 0;
    cashTable_->setCounts(cash_, freeroll ? "Free buy-in — no cash required"
                                          : "Click currency buttons to add cash");
    pad_->setCounts(cash_);
    const std::int64_t total = totalCents(cash_);
    cashTotal_->setText(money(total));

    if (!tournament_) {
        nextButton_->setEnabled(true);
        pad_->setInteractive(true);
    } else if (freeroll) {
        pad_->setInteractive(false);
        nextButton_->setEnabled(true);
    } else {
        pad_->setInteractive(true);
        nextButton_->setEnabled(total >= fixedCents_);
    }
}

void BuyInScreen::toAmountPage() {
    const std::int64_t total = totalCents(cash_);
    if (!tournament_ && total <= 0) {
        QMessageBox::information(this, QString(), "Please add at least one coin/note.");
        return;
    }
    if (tournament_ && fixedCents_ > 0 && total < fixedCents_) {
        QMessageBox::information(this, QString(),
                                 QStringLiteral("Cash received is less than the buy-in amount (%1).")
                                     .arg(money(fixedCents_)));
        return;
    }

    amountCents_ = defaultAmountCents();
    amountText_ = QString::number(amountCents_ / 100.0, 'f', 2);
    amountEditButtons_->setVisible(!tournament_);
    numpad_->setVisible(!tournament_);
    pages_->setCurrentWidget(amountPage_);
    refreshAmount();
}

void BuyInScreen::refreshAmount() {
    const std::int64_t total = totalCents(cash_);
    cashReceived_->setText(money(total));
    buyInAmount_->setText(money(amountCents_));
    const std::int64_t change = total - amountCents_;
    changeDue_->setText(money(change));

    if (change <= 0) {
        changeTable_->setRows({}, "No change to give");
        return;
    }
    // Change can come from the float plus the cash just handed over.
    Cashbox probe = ctx_.cashbox();
    probe.applyCount(toCurrencyCount(cash_, ctx_.table()), +1, MovementReason::BuyIn);
    const auto made = probe.makeChange(Money(change));
    if (made) {
        changeTable_->setCounts(toCounts(*made), "No change to give");
    } else {
        changeTable_->setBody(makeChangeErrorPanel());
    }
}

void BuyInScreen::amountKey(const QString& key) {
    if (key == QStringLiteral("⌫")) {
        if (amountText_.size() > 1) amountText_.chop(1);
        else amountText_ = "0.00";
    } else if (key == ".") {
        if (!amountText_.contains('.')) amountText_ += '.';
    } else if (amountText_ == "0.00") {
        amountText_ = key;
    } else {
        amountText_ += key;
    }
    bool ok = false;
    const double value = amountText_.toDouble(&ok);
    if (ok) {
        amountCents_ = static_cast<std::int64_t>(std::llround(value * 100.0));
        refreshAmount();
    }
}

void BuyInScreen::confirm() {
    const std::int64_t total = totalCents(cash_);
    if (tournament_ ? amountCents_ < 0 : amountCents_ <= 0) {
        QMessageBox::information(this, QString(), "Please enter a valid buy-in amount.");
        return;
    }
    if (amountCents_ > total) {
        QMessageBox::information(this, QString(), "Buy-in amount cannot exceed cash received.");
        return;
    }
    const Player* player = ctx_.players().find(playerId_);
    if (!player) {
        QMessageBox::warning(this, "Buy-in", "Unknown player.");
        return;
    }
    const BuyInQuote quote = ctx_.buyIns().preview(*player, Money(amountCents_));
    if (!quote.allowed) {
        QMessageBox::warning(this, "Buy-in", toQ(quote.message));
        return;
    }
    process();
}

void BuyInScreen::process() {
    const Player* player = ctx_.players().find(playerId_);
    if (!player) return;
    const auto result = ctx_.buyIns().buyIn(*player, toCurrencyCount(cash_, ctx_.table()),
                                            Money(amountCents_));
    if (!result.success) {
        QMessageBox::warning(this, "Buy-in", toQ(result.message));
        return;
    }
    emit closed(true);
}

std::int64_t BuyInScreen::defaultAmountCents() const {
    if (tournament_) return fixedCents_;

    const CashGameConfig& config = ctx_.cashGame().config();
    std::int64_t amount = totalCents(cash_);
    if (const auto max = config.ceiling(ctx_.buyIns().history(playerId_))) {
        if (max->cents() < amount) amount = max->cents();
    }
    return amount < 0 ? 0 : amount;
}

void BuyInScreen::back() {
    if (tournament_) {
        const TournamentConfig& config = ctx_.tournament().config();
        if (config.usesMembershipPricing() || config.arcOnly) {
            emit closed(false);
            return;
        }
        pages_->setCurrentWidget(typePage_);
        startTournament();
        return;
    }
    emit closed(false);
}

} // namespace pokersoc::ui
