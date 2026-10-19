#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "application/CashboxService.h"
#include "application/ReportService.h"
#include "domain/cashbox/Cashbox.h"
#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Widgets.h"

namespace pokersoc::ui::qt {

using namespace domain;

namespace {

const std::int64_t kAscending[] = {5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};

QString exchangeColor(std::int64_t cents) {
    switch (cents) {
        case 500: return "#CDA9E5";
        case 1000: return "#AED5F2";
        case 2000: return "#F6B0A7";
        case 5000: return "#F7E89A";
        case 10000: return "#BFE7C0";
        default: return cents >= 100 ? color::GoldCoin : color::SilverCoin;
    }
}

CountTable::Look exchangeLook(const QString& accent, const QString& tint) {
    CountTable::Look look;
    look.headerBg = tint;
    look.headerBorder = accent;
    look.headerBorderWidth = "2px 2px 1px 2px";
    look.headerPx = 12;
    look.headerHeight = 30;
    look.rowBg = color::White;
    look.rowBorder = accent;
    look.rowBorderWidth = "0px 2px 1px 2px";
    look.rowRadius = 0;
    look.countColor = accent;
    look.placeholderBorder = accent;
    look.placeholderBorderWidth = "0px 2px 2px 2px";
    look.placeholderPx = 12;
    look.minRows = 3;
    look.compact = true;
    return look;
}

QLabel* inlineTotal(QFrame* box, const QString& caption, int px, const QString& valueColor) {
    auto* row = new QHBoxLayout;
    row->addWidget(makeLabel(caption, px, true));
    QLabel* value = makeLabel("$0.00", px, true, valueColor);
    row->addWidget(value);
    row->addStretch(1);
    panelLayout(box)->addLayout(row);
    return value;
}

} // namespace

CashboxScreen::CashboxScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget;
    layout->addWidget(pages_);

    mainPage_ = buildMainPage();
    floatPage_ = buildCountPage(false);
    tipPage_ = buildCountPage(true);
    exchangePage_ = buildExchangePage();
    for (QWidget* page : {mainPage_, floatPage_, tipPage_, exchangePage_}) {
        pages_->addWidget(page);
    }
    refreshMain();
    refreshCount(false);
    refreshCount(true);
    refreshExchange();
}

// ---------------------------------------------------------------------------
// Main page

QWidget* CashboxScreen::buildMainPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Denominations"));
    auto* table = new QWidget;
    table->setObjectName("stock");
    table->setAttribute(Qt::WA_StyledBackground);
    table->setStyleSheet("QWidget#stock { background: white; }");
    stockRows_ = new QVBoxLayout(table);
    stockRows_->setContentsMargins(0, 0, 0, 0);
    stockRows_->setSpacing(0);
    left->addWidget(table, 1);
    left->addWidget(makeInfoTile(QStringLiteral("💵"), "Total Cash", &totalCash_, color::LightYellow));

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    auto* addFloat = new TileButton(QStringLiteral("💰"), "Add Float", "Add starting cash");
    auto* addTip = new TileButton(QStringLiteral("💸"), "Add Tip", "Record cash tip received");
    auto* exchange = new TileButton(QStringLiteral("🔄"), "Exchange", "Swap denominations");
    auto* soon = new TileButton(QStringLiteral("📈"), "Button 7", "Coming soon");
    soon->setColors(color::ButtonFace, color::LightGray);
    soon->setEnabled(false);
    soon->setFaded(true);
    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral);

    grid->addWidget(addFloat, 0, 0);
    grid->addWidget(makeInfoTile(QStringLiteral("📊"), "Float Added", &floatAdded_, color::LightGreen,
                                 color::Gray, "2px"),
                    0, 1);
    grid->addWidget(addTip, 1, 0);
    grid->addWidget(makeInfoTile(QStringLiteral("💸"), "Tips", &tips_, color::Plum, color::Gray, "2px"),
                    1, 1);
    grid->addWidget(exchange, 2, 0);
    grid->addWidget(makeInfoTile(QStringLiteral("💰"), "Total Sales", &totalSales_, color::LightYellow,
                                 color::Gold, "2px"),
                    2, 1);
    grid->addWidget(soon, 3, 0);
    grid->addWidget(back, 3, 1);
    makeUniform(grid, 4, 2);
    for (int i = 0; i < grid->count(); ++i) {
        grid->itemAt(i)->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    columns.right->addLayout(grid);

    connect(addFloat, &QPushButton::clicked, this, [this] { pages_->setCurrentWidget(floatPage_); });
    connect(addTip, &QPushButton::clicked, this, [this] { pages_->setCurrentWidget(tipPage_); });
    connect(exchange, &QPushButton::clicked, this, [this] { pages_->setCurrentWidget(exchangePage_); });
    connect(back, &QPushButton::clicked, this, [this] { emit closed(changed_); });
    return page;
}

void CashboxScreen::refreshMain() {
    clearLayout(stockRows_);
    const auto makeRow = [](const QString& a, const QString& b, const QString& c, bool header) {
        auto* row = new QFrame;
        row->setObjectName("r");
        row->setStyleSheet(header ? QStringLiteral("QFrame#r { background:%1; border:1px solid %2; }")
                                        .arg(color::LightGray, color::Gray)
                                  : QStringLiteral("QFrame#r { border-style:solid; border-color:%1;"
                                                   " border-width:0px 1px 1px 1px; }")
                                        .arg(color::Gray));
        row->setFixedHeight(header ? 40 : 36);
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 0, 0, 0);
        line->setSpacing(0);
        const int widths[] = {110, 80, 100};
        const QString texts[] = {a, b, c};
        for (int i = 0; i < 3; ++i) {
            QLabel* label = makeLabel(texts[i], header ? 14 : 13, header, QString(), Qt::AlignCenter);
            label->setFixedWidth(widths[i]);
            line->addWidget(label);
        }
        line->addStretch(1);
        return row;
    };
    stockRows_->addWidget(makeRow("Denomination", "Count", "Value", true));
    for (std::int64_t cents : kAscending) {
        const int qty = ctx_.cashbox().quantityOf(cents);
        const QString label = cents < 100 ? QString::number(cents) + QStringLiteral("¢")
                                          : "$" + QString::number(cents / 100);
        stockRows_->addWidget(makeRow(label, QString::number(qty), money(cents * qty), false));
    }
    stockRows_->addStretch(1);

    const auto summary = ctx_.reports().summary();
    totalCash_->setText(money(summary.cashInBox));
    floatAdded_->setText(money(summary.floatTotal));
    tips_->setText(money(summary.tipsTotal));
    totalSales_->setText(money(summary.foodSalesTotal));
}

// ---------------------------------------------------------------------------
// Add float / add tip

QWidget* CashboxScreen::buildCountPage(bool tip) {
    CountPage& state = tip ? tip_ : float_;
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading(tip ? "Tip Received" : "Add Float"));
    state.table = new CountTable({"Denomination", "Count", "Value"});
    left->addWidget(state.table, 1);

    auto* clear = new TileButton(QStringLiteral("🗑️"), "Clear", tip ? "All cash" : "All float");
    clear->setColors(color::Orange, color::Gray, "0px 1px 1px 1px");
    auto* undo = new TileButton(QStringLiteral("↶"), "Undo", "Last action");
    undo->setColors(color::Yellow, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(clear, undo));
    left->addWidget(makeInfoTile(QStringLiteral("💰"), tip ? "Total Tip" : "Total Float", &state.total,
                                 color::LightGreen));

    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral, color::Gray, "0px 1px 1px 1px");
    auto* save = tip ? new TileButton(QStringLiteral("✅"), "Confirm Tip", "Record tip to cashbox")
                     : new TileButton(QStringLiteral("💰"), "Add Float", "Save to cashbox");
    save->setColors(color::LightBlue, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(back, save));
    for (QWidget* tile : {static_cast<QWidget*>(clear), static_cast<QWidget*>(undo),
                          static_cast<QWidget*>(back), static_cast<QWidget*>(save)}) {
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    state.pad = new CurrencyPad;
    columns.right->addWidget(state.pad);

    connect(state.pad, &CurrencyPad::denomClicked, this, [this, tip](std::int64_t cents) {
        CountPage& s = tip ? tip_ : float_;
        const int qty = s.pad->multiplier();
        s.counts[cents] += qty;
        s.history.emplace_back(cents, qty);
        refreshCount(tip);
    });
    connect(clear, &QPushButton::clicked, this, [this, tip] {
        CountPage& s = tip ? tip_ : float_;
        s.counts.clear();
        s.history.clear();
        refreshCount(tip);
    });
    connect(undo, &QPushButton::clicked, this, [this, tip] {
        CountPage& s = tip ? tip_ : float_;
        if (s.history.empty()) return;
        const auto [cents, qty] = s.history.back();
        s.history.pop_back();
        s.counts[cents] = std::max(0, s.counts[cents] - qty);
        refreshCount(tip);
    });
    connect(back, &QPushButton::clicked, this, [this] { pages_->setCurrentWidget(mainPage_); });
    connect(save, &QPushButton::clicked, this, [this, tip] { tip ? saveTip() : saveFloat(); });
    return page;
}

void CashboxScreen::refreshCount(bool tip) {
    CountPage& state = tip ? tip_ : float_;
    state.table->setCounts(state.counts, tip ? "Click currency buttons to add tip"
                                             : "Click currency buttons to add float");
    state.pad->setCounts(state.counts);
    const QString total = money(totalCents(state.counts));
    state.total->setText(tip ? total : "Total add float: " + total);
}

void CashboxScreen::saveFloat() {
    if (totalCents(float_.counts) > 0) {
        const auto result =
            ctx_.cashboxOps().addFloat(toCurrencyCount(float_.counts, ctx_.table()));
        if (!result.success) {
            QMessageBox::warning(this, "Add Float", toQ(result.message));
            return;
        }
        changed_ = true;
    }
    float_.counts.clear();
    float_.history.clear();
    refreshCount(false);
    refreshMain();
    pages_->setCurrentWidget(mainPage_);
}

void CashboxScreen::saveTip() {
    if (totalCents(tip_.counts) <= 0) {
        QMessageBox::information(this, "No Tip Amount",
                                 "Please add at least one denomination for the tip.");
        return;
    }
    const auto result = ctx_.cashboxOps().addTip(toCurrencyCount(tip_.counts, ctx_.table()), "",
                                                 "Cash tip received");
    if (!result.success) {
        QMessageBox::warning(this, "Add Tip", toQ(result.message));
        return;
    }
    changed_ = true;
    tip_.counts.clear();
    tip_.history.clear();
    refreshCount(true);
    refreshMain();
    pages_->setCurrentWidget(mainPage_);
}

// ---------------------------------------------------------------------------
// Exchange

QWidget* CashboxScreen::buildExchangePage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;
    left->setSpacing(0);

    left->addWidget(makeLabel("Give Out (Remove from Cashbox)", 16, true, color::Crimson));
    left->addSpacing(5);
    giveTable_ = new CountTable({"Denom", "Count", "Value"}, exchangeLook(color::Crimson, "#FFE0E0"));
    left->addWidget(giveTable_, 1);
    QFrame* giveBox = makePanel("#FFE0E0", color::Crimson, 2, 0, 8);
    giveTotal_ = inlineTotal(giveBox, "Give Out Total: ", 14, color::Crimson);
    left->addWidget(giveBox);
    left->addSpacing(10);

    left->addWidget(makeLabel("Receive (Add to Cashbox)", 16, true, color::Green));
    left->addSpacing(5);
    receiveTable_ = new CountTable({"Denom", "Count", "Value"}, exchangeLook(color::Green, "#E0FFE0"));
    left->addWidget(receiveTable_, 1);
    QFrame* receiveBox = makePanel("#E0FFE0", color::Green, 2, 0, 8);
    receiveTotal_ = inlineTotal(receiveBox, "Receive Total: ", 14, color::Green);
    left->addWidget(receiveBox);
    left->addSpacing(5);

    auto* balanceBox = makePanel(color::LightYellow, color::Orange, 2, 0, 8);
    balance_ = inlineTotal(balanceBox, "Balance: ", 16, color::Orange);
    balanceBox_ = balanceBox;
    left->addWidget(balanceBox);
    left->addSpacing(10);

    auto* cancel = new TileButton(QStringLiteral("❌"), "Cancel");
    cancel->setSizes(24, 14, 10);
    cancel->setColors(color::LightCoral);
    cancel->layout()->setContentsMargins(15, 15, 15, 15);
    exchangeButton_ = new TileButton(QStringLiteral("✅"), "Exchange");
    exchangeButton_->setSizes(24, 14, 10);
    exchangeButton_->setColors(color::LightGreen);
    exchangeButton_->layout()->setContentsMargins(15, 15, 15, 15);
    cancel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    exchangeButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    left->addLayout(makePair(cancel, exchangeButton_));
    connect(cancel, &QPushButton::clicked, this, [this] {
        give_.clear();
        receive_.clear();
        refreshExchange();
        pages_->setCurrentWidget(mainPage_);
    });
    connect(exchangeButton_, &QPushButton::clicked, this, [this] { saveExchange(); });

    QVBoxLayout* right = columns.right;
    auto* giveToggle = new TileButton(QStringLiteral("➖"), "Give Out");
    giveToggle->setSizes(24, 14, 10);
    giveToggle->titleLabel()->setStyleSheet(QStringLiteral("color:%1;").arg(color::Crimson));
    giveToggle->layout()->setContentsMargins(10, 10, 10, 10);
    auto* receiveToggle = new TileButton(QStringLiteral("➕"), "Receive");
    receiveToggle->setSizes(24, 14, 10);
    receiveToggle->titleLabel()->setStyleSheet(QStringLiteral("color:%1;").arg(color::Green));
    receiveToggle->layout()->setContentsMargins(10, 10, 10, 10);
    giveToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    receiveToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    giveToggle_ = giveToggle;
    receiveToggle_ = receiveToggle;
    right->addLayout(makePair(giveToggle, receiveToggle));
    connect(giveToggle, &QPushButton::clicked, this, [this] {
        giveMode_ = true;
        refreshExchange();
    });
    connect(receiveToggle, &QPushButton::clicked, this, [this] {
        giveMode_ = false;
        refreshExchange();
    });

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    for (int i = 0; i < 11; ++i) {
        const std::int64_t cents = kAscending[i];
        QPushButton* button = makeButton(denomLabel(cents), QString(), 28, true);
        button->setStyleSheet(buttonStyle(exchangeColor(cents)));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(button, &QPushButton::clicked, this, [this, cents] {
            ++(giveMode_ ? give_ : receive_)[cents];
            refreshExchange();
        });
        grid->addWidget(button, i / 2, i % 2);
    }
    auto* clearAll = new TileButton(QStringLiteral("🗑️"), "Clear All");
    clearAll->setSizes(28, 12, 10);
    clearAll->setColors(color::Orange);
    clearAll->layout()->setContentsMargins(4, 4, 4, 4);
    grid->addWidget(clearAll, 5, 1);
    makeUniform(grid, 6, 2);
    right->addLayout(grid, 1);
    connect(clearAll, &QPushButton::clicked, this, [this] {
        give_.clear();
        receive_.clear();
        refreshExchange();
    });
    return page;
}

void CashboxScreen::refreshExchange() {
    giveTable_->setCounts(give_, "Click denominations");
    receiveTable_->setCounts(receive_, "Click denominations");
    const std::int64_t give = totalCents(give_);
    const std::int64_t receive = totalCents(receive_);
    giveTotal_->setText(money(give));
    receiveTotal_->setText(money(receive));

    const std::int64_t balance = receive - give;
    const bool balanced = balance == 0 && give > 0;
    balance_->setText(money(balance));
    const QString valueColor = balanced ? color::Green : balance < 0 ? color::Crimson : color::Orange;
    balance_->setStyleSheet(QStringLiteral("color:%1;").arg(valueColor));
    balanceBox_->setStyleSheet(panelStyle("panel", balanced ? "#E0FFE0" : color::LightYellow,
                                          balanced ? color::Green : color::Orange, 2, 0));
    exchangeButton_->setEnabled(balanced);

    static_cast<TileButton*>(giveToggle_)
        ->setColors(giveMode_ ? "#FFE0E0" : color::LightGray, giveMode_ ? color::Crimson : color::Gray,
                    giveMode_ ? "3px" : "2px");
    static_cast<TileButton*>(receiveToggle_)
        ->setColors(!giveMode_ ? "#E0FFE0" : color::LightGray, !giveMode_ ? color::Green : color::Gray,
                    !giveMode_ ? "3px" : "2px");
}

void CashboxScreen::saveExchange() {
    const std::int64_t give = totalCents(give_);
    if (give <= 0 || give != totalCents(receive_)) {
        QMessageBox::information(this, "Invalid Exchange",
                                 "Exchange amounts must be equal and greater than zero.");
        return;
    }
    const auto result = ctx_.cashboxOps().exchange(toCurrencyCount(give_, ctx_.table()),
                                                   toCurrencyCount(receive_, ctx_.table()));
    if (!result.success) {
        QMessageBox::warning(this, "Exchange", toQ(result.message));
        return;
    }
    changed_ = true;
    give_.clear();
    receive_.clear();
    refreshExchange();
    refreshMain();
    pages_->setCurrentWidget(mainPage_);
}

} // namespace pokersoc::ui::qt
