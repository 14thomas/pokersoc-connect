#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "application/CashOutService.h"
#include "application/PlayerService.h"
#include "domain/food/ProductCatalog.h"
#include "domain/money/DenominationTable.h"
#include "domain/player/Player.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

using namespace domain;
using application::CashOutService;

namespace {

const std::int64_t kCurrency[] = {10000, 5000, 2000, 1000, 500, 200, 100, 50, 20, 10, 5};

TileButton* makeActionTile(const QString& icon, const QString& title, const QString& subtitle,
                           const QString& background = color::ButtonFace) {
    auto* tile = new TileButton(icon, title, subtitle);
    tile->setColors(background);
    return tile;
}

} // namespace

CashOutScreen::CashOutScreen(SessionContext& context, std::string playerId, QWidget* parent)
    : Screen(context, parent), playerId_(std::move(playerId)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget;
    layout->addWidget(pages_);
    pages_->addWidget(buildChipPage());
    pages_->addWidget(buildChangePage());

    buildFoodOverlay();
    buildTipOverlay();
    buildExtraCashOverlay();
    buildChangeOverlay();
    refreshChips();
}

// ---------------------------------------------------------------------------
// Step 1: chips

QWidget* CashOutScreen::buildChipPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Chips to Cash Out"));
    chipTable_ = new CountTable({"Chip", "Count", "Value"});
    left->addWidget(chipTable_, 1);

    const Player* player = ctx_.players().find(playerId_);
    const QString name = !player ? QStringLiteral("Unknown")
                         : player->name().empty() ? QStringLiteral("New Player")
                                                  : toQ(player->name());
    left->addWidget(makePlayerBox(toQ(playerId_), name));

    auto* clear = new TileButton(QStringLiteral("🗑️"), "Clear", "All chips");
    clear->setColors(color::Orange, color::Gray, "0px 1px 1px 1px");
    auto* undo = new TileButton(QStringLiteral("↶"), "Undo", "Last action");
    undo->setColors(color::Yellow, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(clear, undo));
    connect(clear, &QPushButton::clicked, this, [this] {
        chips_.clear();
        history_.clear();
        refreshChips();
    });
    connect(undo, &QPushButton::clicked, this, [this] {
        if (history_.empty()) return;
        const auto [cents, qty] = history_.back();
        history_.pop_back();
        chips_[cents] = std::max(0, chips_[cents] - qty);
        refreshChips();
    });

    left->addWidget(makeInfoTile(QStringLiteral("💰"), "Total Value", &chipTotal_, color::LightGreen));

    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral, color::Gray, "0px 1px 1px 1px");
    nextButton_ = new TileButton(QStringLiteral("→"), "Next", "Continue to change");
    nextButton_->setColors(color::LightBlue, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(back, nextButton_));
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });
    connect(nextButton_, &QPushButton::clicked, this, [this] { toChangePage(); });
    for (QWidget* tile : {static_cast<QWidget*>(clear), static_cast<QWidget*>(undo),
                          static_cast<QWidget*>(back), static_cast<QWidget*>(nextButton_)}) {
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    pad_ = new ChipPad;
    connect(pad_, &ChipPad::denomClicked, this, [this](std::int64_t cents) {
        const int qty = std::max(1, pad_->addAmount(cents));
        chips_[cents] += qty;
        history_.emplace_back(cents, qty);
        refreshChips();
    });
    columns.right->addWidget(pad_);
    return page;
}

void CashOutScreen::refreshChips() {
    chipTable_->setCounts(chips_, "Click chip buttons to add chips");
    pad_->setCounts(chips_);
    const std::int64_t total = totalCents(chips_);
    chipTotal_->setText(money(total));
    nextButton_->setEnabled(total > 0);
}

void CashOutScreen::toChangePage() {
    if (totalCents(chips_) <= 0) {
        QMessageBox::information(this, QString(), "Please select chips to cash out first.");
        return;
    }
    food_.clear();
    tipCents_ = 0;
    extraCash_.clear();
    customChange_.reset();
    pages_->setCurrentIndex(1);
    refreshPlan();
}

// ---------------------------------------------------------------------------
// Step 2: change, food, tip, extra cash

QWidget* CashOutScreen::buildChangePage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Change Breakdown"));
    changeTable_ = new CountTable({"Denomination", "Count", "Value"});
    left->addWidget(changeTable_, 1);
    left->addWidget(
        makeInfoTile(QStringLiteral("💰"), "Total Value", &changeTotal_, color::LightGreen));

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    auto* food = makeActionTile(QStringLiteral("🍕"), "Add Food", "Purchase items");
    auto* tip = makeActionTile(QStringLiteral("💸"), "Add Tip", "Custom tip amount");
    auto* change = makeActionTile(QStringLiteral("💰"), "Change", "Customize denominations");
    auto* extra = makeActionTile(QStringLiteral("💵"), "Add Cash", "Add extra money");
    grid->addWidget(food, 0, 0);
    grid->addWidget(tip, 0, 1);
    grid->addWidget(change, 1, 0);
    grid->addWidget(extra, 1, 1);

    QFrame* summary = makePanel(color::LightBlue, color::Gray, 2, 0, 0);
    QVBoxLayout* summaryLayout = panelLayout(summary);
    summaryLayout->setContentsMargins(15, 12, 15, 12);
    summaryLayout->addStretch(1);
    summaryLayout->addWidget(makeLabel(QStringLiteral("📊"), 36, false, QString(), Qt::AlignCenter));
    summaryLayout->addSpacing(8);
    summaryLayout->addWidget(makeLabel("Cashout Summary", 18, true, QString(), Qt::AlignCenter));
    const auto addLine = [&](QLabel** label, int px, bool bold) {
        *label = makeLabel(QString(), px, bold, QString(), Qt::AlignCenter);
        (*label)->setContentsMargins(0, 2, 0, 2);
        summaryLayout->addWidget(*label);
    };
    addLine(&chipValueText_, 12, false);
    addLine(&foodValueText_, 12, false);
    addLine(&tipValueText_, 12, false);
    addLine(&extraValueText_, 12, false);
    addLine(&finalChangeText_, 14, true);
    summaryLayout->addStretch(1);
    summary->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(summary, 2, 0, 1, 2);

    auto* back = makeActionTile(QStringLiteral("←"), "Back", "Edit chips", color::LightCoral);
    confirmButton_ = makeActionTile(QStringLiteral("✓"), "Confirm", "Process cashout", color::LightBlue);
    grid->addWidget(back, 3, 0);
    grid->addWidget(confirmButton_, 3, 1);
    makeUniform(grid, 4, 2);
    columns.right->addLayout(grid);

    connect(food, &QPushButton::clicked, this, [this] {
        foodDraft_ = food_;
        refreshFoodOverlay();
        foodOverlay_->open();
    });
    connect(tip, &QPushButton::clicked, this, [this] {
        if (tipCents_ == 0) {
            // Suggest the coins in the change as the tip.
            tipDraft_ = 0;
            for (const auto& [cents, qty] : shownChange_) {
                if (cents <= 200) tipDraft_ += cents * qty;
            }
        } else {
            tipDraft_ = tipCents_;
        }
        tipDisplay_->setText(money(tipDraft_));
        tipOverlay_->open();
    });
    connect(change, &QPushButton::clicked, this, [this] {
        refreshChangeOverlay();
        changeOverlay_->open();
    });
    connect(extra, &QPushButton::clicked, this, [this] {
        extraDraft_ = extraCash_;
        extraDraftHistory_.clear();
        refreshExtraCashOverlay();
        extraOverlay_->open();
    });
    connect(back, &QPushButton::clicked, this, [this] { pages_->setCurrentIndex(0); });
    connect(confirmButton_, &QPushButton::clicked, this, [this] { confirm(); });
    return page;
}

void CashOutScreen::refreshPlan() {
    CashOutService::Request request;
    request.playerId = playerId_;
    request.chips = toChipCount(chips_, ctx_.table());
    request.tip = Money(tipCents_);
    request.extraCash = toCurrencyCount(extraCash_, ctx_.table());
    request.food = food_;
    if (customChange_) request.customChange = toCurrencyCount(*customChange_, ctx_.table());

    const std::int64_t chipCents = totalCents(chips_);
    const std::int64_t foodCents = saleLinesTotal(food_).cents();
    const std::int64_t extraCents = totalCents(extraCash_);
    const std::int64_t raw = chipCents - foodCents - tipCents_ + extraCents;
    chipValueText_->setText("Chip Value: " + money(chipCents));
    foodValueText_->setText("Food: " + money(foodCents));
    tipValueText_->setText("Tip: " + money(tipCents_));
    extraValueText_->setText("Extra Cash: " + money(extraCents));
    finalChangeText_->setText("Final Change: " +
                              money(application::roundToFiveCents(Money(raw))));

    const CashOutService::Plan plan =
        ctx_.cashOuts().plan(ctx_.players().find(playerId_), request);
    if (!plan.ok) {
        shownChange_.clear();
        changeTotal_->setText(money(0));
        confirmButton_->setEnabled(false);
        const std::string& why = plan.message;
        if (why.rfind("Cannot make change", 0) == 0 || why.rfind("The cashbox", 0) == 0) {
            changeTable_->setBody(makeChangeErrorPanel());
        } else {
            changeTable_->setRows({}, toQ(why));
        }
        return;
    }

    shownChange_ = toCounts(plan.change);
    // Orange rows when stock gaps forced a different mix from the ideal one.
    Counts ideal;
    std::int64_t remaining = plan.payout.cents();
    for (std::int64_t face : kCurrency) {
        if (remaining >= face) {
            ideal[face] = static_cast<int>(remaining / face);
            remaining %= face;
        }
    }
    changeTable_->look().rowBg = !customChange_ && ideal != shownChange_ ? "#FFE6C8" : "#F5FFF5";
    changeTable_->setCounts(shownChange_, "No change to give");
    changeTotal_->setText(money(plan.change.total()));
    confirmButton_->setEnabled(chipCents > 0);
}

void CashOutScreen::confirm() {
    CashOutService::Request request;
    request.playerId = playerId_;
    request.chips = toChipCount(chips_, ctx_.table());
    request.tip = Money(tipCents_);
    request.extraCash = toCurrencyCount(extraCash_, ctx_.table());
    request.food = food_;
    if (customChange_) request.customChange = toCurrencyCount(*customChange_, ctx_.table());

    const auto result = ctx_.cashOuts().cashOut(ctx_.players().find(playerId_), request);
    if (!result.success) {
        QMessageBox::warning(this, "Cash-out", toQ(result.message));
        refreshPlan();
        return;
    }
    emit closed(true);
}

// ---------------------------------------------------------------------------
// Overlays

void CashOutScreen::buildFoodOverlay() {
    foodOverlay_ = new Overlay(this, QColor(0, 0, 0, 0x80));
    foodOverlay_->card()->setMinimumWidth(500);
    QVBoxLayout* layout = foodOverlay_->cardLayout();
    layout->addWidget(makeLabel("Select Food Items", 20, true, QString(), Qt::AlignCenter));
    layout->addSpacing(20);

    QFrame* selected = makePanel("#F0F0F0", "#F0F0F0", 0, 5, 10);
    panelLayout(selected)->addWidget(makeLabel("Selected Items:", 12, true));
    panelLayout(selected)->addSpacing(5);
    foodSelected_ = new QVBoxLayout;
    foodSelected_->setSpacing(2);
    panelLayout(selected)->addLayout(foodSelected_);
    foodTotal_ = makeLabel("Total: $0.00", 12, true, QString(), Qt::AlignRight);
    foodTotal_->setContentsMargins(0, 5, 0, 5);
    panelLayout(selected)->addWidget(foodTotal_);
    layout->addWidget(selected);
    layout->addSpacing(10);

    layout->addWidget(makeLabel("Available Items:", 12, true));
    layout->addSpacing(5);
    auto* scroll = new QScrollArea;
    scroll->setFixedSize(450, 300);
    scroll->setWidgetResizable(true);
    auto* list = new QWidget;
    auto* listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);
    const auto products = ctx_.catalog().all();
    if (products.empty()) {
        listLayout->addWidget(makeLabel("No food items available", 12, false, QString(), Qt::AlignCenter));
    }
    for (const Product* product : products) {
        QPushButton* button = makeButton(toQ(product->name) + " - " + money(product->price),
                                         color::LightBlue, 14);
        button->setStyleSheet(buttonStyle(color::LightBlue, "#707070", "1px", "padding:8px 10px;"));
        const SaleLine line{product->name, product->price, 1};
        connect(button, &QPushButton::clicked, this, [this, line] {
            for (SaleLine& existing : foodDraft_) {
                if (existing.productName == line.productName && existing.unitPrice == line.unitPrice) {
                    ++existing.qty;
                    refreshFoodOverlay();
                    return;
                }
            }
            foodDraft_.push_back(line);
            refreshFoodOverlay();
        });
        listLayout->addWidget(button);
    }
    listLayout->addStretch(1);
    scroll->setWidget(list);
    layout->addWidget(scroll, 0, Qt::AlignHCenter);
    layout->addSpacing(20);

    QPushButton* cancel = makeButton("Cancel");
    QPushButton* done = makeButton("Done");
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addSpacing(10);
    buttons->addWidget(done);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, [this] {
        foodOverlay_->hide();
        food_.clear();
        refreshPlan();
    });
    connect(done, &QPushButton::clicked, this, [this] {
        foodOverlay_->hide();
        food_ = foodDraft_;
        refreshPlan();
    });
}

void CashOutScreen::refreshFoodOverlay() {
    clearLayout(foodSelected_);
    for (std::size_t i = 0; i < foodDraft_.size(); ++i) {
        const SaleLine& line = foodDraft_[i];
        auto* row = new QHBoxLayout;
        QLabel* name = makeLabel(toQ(line.productName) + " x" + QString::number(line.qty), 12);
        name->setFixedWidth(200);
        QLabel* price = makeLabel(money(line.total()), 12, false, QString(), Qt::AlignRight);
        price->setFixedWidth(80);
        auto* remove = new QPushButton(QStringLiteral("✕"));
        remove->setFixedSize(30, 25);
        remove->setFont(uiFont(12, true));
        remove->setStyleSheet(buttonStyle(color::LightCoral, "#707070", "1px"));
        connect(remove, &QPushButton::clicked, this, [this, i] {
            if (i >= foodDraft_.size()) return;
            if (--foodDraft_[i].qty <= 0) foodDraft_.erase(foodDraft_.begin() + static_cast<long>(i));
            refreshFoodOverlay();
        });
        row->addWidget(name);
        row->addWidget(price);
        row->addSpacing(5);
        row->addWidget(remove);
        row->addStretch(1);
        foodSelected_->addLayout(row);
    }
    foodTotal_->setText("Total: " + money(saleLinesTotal(foodDraft_)));
}

void CashOutScreen::buildTipOverlay() {
    tipOverlay_ = new Overlay(this, QColor(0, 0, 0, 0x80));
    tipOverlay_->card()->setMinimumWidth(400);
    QVBoxLayout* layout = tipOverlay_->cardLayout();
    layout->addWidget(makeLabel("Enter Tip Amount", 20, true, QString(), Qt::AlignCenter));
    layout->addSpacing(20);
    QFrame* box = makePanel(color::LightGray, color::LightGray, 0, 5, 20);
    tipDisplay_ = makeLabel("$0.00", 24, true, QString(), Qt::AlignCenter);
    panelLayout(box)->addWidget(tipDisplay_);
    layout->addWidget(box);
    layout->addSpacing(20);

    auto* pad = new Numpad({"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", QStringLiteral("⌫")},
                           20);
    pad->setFixedSize(300, 300);
    connect(pad, &Numpad::pressed, this, [this](const QString& key) {
        if (key == QStringLiteral("⌫")) {
            tipDraft_ /= 10;
        } else if (key != ".") {
            tipDraft_ = tipDraft_ * 10 + key.toInt();
        }
        tipDisplay_->setText(money(tipDraft_));
    });
    layout->addWidget(pad, 0, Qt::AlignHCenter);
    layout->addSpacing(20);

    QPushButton* close = makeButton("Close");
    QPushButton* set = makeButton("Set Tip");
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);
    buttons->addSpacing(10);
    buttons->addWidget(set);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, tipOverlay_, &QWidget::hide);
    connect(set, &QPushButton::clicked, this, [this] {
        tipCents_ = tipDraft_;
        refreshPlan();
    });
}

void CashOutScreen::buildExtraCashOverlay() {
    extraOverlay_ = new Overlay(this, QColor(0, 0, 0, 0x80));
    extraOverlay_->card()->setMinimumWidth(500);
    QVBoxLayout* layout = extraOverlay_->cardLayout();
    layout->addWidget(makeLabel("Add Extra Cash", 20, true, QString(), Qt::AlignCenter));
    layout->addSpacing(20);
    QFrame* box = makePanel(color::LightGray, color::LightGray, 0, 5, 20);
    extraDisplay_ = makeLabel("$0.00", 24, true, QString(), Qt::AlignCenter);
    panelLayout(box)->addWidget(extraDisplay_);
    layout->addWidget(box);
    layout->addSpacing(20);

    auto* padWidget = new QWidget;
    padWidget->setFixedSize(400, 360);
    auto* grid = new QGridLayout(padWidget);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    const std::int64_t faces[] = {5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (int i = 0; i < 11; ++i) {
        const std::int64_t cents = faces[i];
        QPushButton* button = makeButton(denomLabel(cents), QString(), 16);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(button, &QPushButton::clicked, this, [this, cents] {
            ++extraDraft_[cents];
            extraDraftHistory_.push_back(cents);
            refreshExtraCashOverlay();
        });
        grid->addWidget(button, i / 4, i % 4);
    }
    QPushButton* clear = makeButton("Clear", QString(), 16);
    clear->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(clear, 2, 3);
    QPushButton* backspace = makeButton(QStringLiteral("⌫"), QString(), 16);
    backspace->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(backspace, 3, 3);
    makeUniform(grid, 4, 4);
    connect(clear, &QPushButton::clicked, this, [this] {
        extraDraft_.clear();
        extraDraftHistory_.clear();
        refreshExtraCashOverlay();
    });
    connect(backspace, &QPushButton::clicked, this, [this] {
        if (extraDraftHistory_.empty()) return;
        const std::int64_t cents = extraDraftHistory_.back();
        extraDraftHistory_.pop_back();
        if (--extraDraft_[cents] <= 0) extraDraft_.erase(cents);
        refreshExtraCashOverlay();
    });
    layout->addWidget(padWidget, 0, Qt::AlignHCenter);
    layout->addSpacing(20);

    QPushButton* close = makeButton("Close");
    QPushButton* add = makeButton("Add Cash");
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);
    buttons->addSpacing(10);
    buttons->addWidget(add);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, extraOverlay_, &QWidget::hide);
    connect(add, &QPushButton::clicked, this, [this] {
        extraCash_ = extraDraft_;
        extraOverlay_->hide();
        refreshPlan();
    });
}

void CashOutScreen::refreshExtraCashOverlay() {
    extraDisplay_->setText(money(totalCents(extraDraft_)));
}

void CashOutScreen::buildChangeOverlay() {
    changeOverlay_ = new Overlay(this, QColor(0, 0, 0, 0x80));
    changeOverlay_->card()->setMinimumWidth(520);
    QVBoxLayout* layout = changeOverlay_->cardLayout();
    layout->addWidget(makeLabel("Customize Change Denominations", 20, true, QString(), Qt::AlignCenter));
    layout->addSpacing(10);
    layout->addWidget(makeLabel("(Manual customization disables automatic optimal)", 12, false,
                                color::Gray, Qt::AlignCenter));
    layout->addSpacing(20);

    auto* list = new QWidget;
    list->setFixedWidth(460);
    changeList_ = new QVBoxLayout(list);
    changeList_->setContentsMargins(0, 0, 0, 0);
    changeList_->setSpacing(4);
    layout->addWidget(list, 0, Qt::AlignHCenter);
    layout->addSpacing(20);

    QPushButton* close = makeButton("Close");
    QPushButton* reset = makeButton("Reset to Optimal");
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(close);
    buttons->addSpacing(10);
    buttons->addWidget(reset);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, changeOverlay_, &QWidget::hide);
    connect(reset, &QPushButton::clicked, this, [this] {
        customChange_.reset();
        refreshPlan();
        refreshChangeOverlay();
    });
}

void CashOutScreen::refreshChangeOverlay() {
    clearLayout(changeList_);
    const Counts& current = customChange_ ? *customChange_ : shownChange_;
    changeList_->addWidget(makeLabel("Current change total: " + money(totalCents(current)), 16,
                                     true, QString(), Qt::AlignCenter));
    changeList_->addSpacing(6);

    for (std::int64_t cents : kCurrency) {
        const auto it = current.find(cents);
        const int qty = it == current.end() ? 0 : it->second;
        QFrame* row = makePanel(color::White, color::Gray, 1, 0, 0);
        row->setFixedHeight(40);
        delete row->layout();
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(0, 2, 0, 2);
        line->setSpacing(0);
        QLabel* label = makeLabel(denomLabel(cents), 14, true, QString(), Qt::AlignCenter);
        label->setFixedWidth(80);
        auto* minus = new QPushButton("-");
        minus->setFixedWidth(40);
        minus->setFont(uiFont(16, true));
        minus->setStyleSheet(buttonStyle(color::LightCoral, "#707070", "1px"));
        QLabel* count = makeLabel(QString::number(qty), 14, true, QString(), Qt::AlignCenter);
        count->setFixedWidth(60);
        auto* plus = new QPushButton("+");
        plus->setFixedWidth(40);
        plus->setFont(uiFont(16, true));
        plus->setStyleSheet(buttonStyle(color::LightGreen, "#707070", "1px"));
        QLabel* value = makeLabel(money(cents * qty), 12, false, QString(), Qt::AlignCenter);
        value->setFixedWidth(80);
        line->addStretch(1);
        line->addWidget(label);
        line->addWidget(minus);
        line->addWidget(count);
        line->addWidget(plus);
        line->addWidget(value);
        line->addStretch(1);
        changeList_->addWidget(row);

        const auto adjust = [this, cents](int delta) {
            Counts edited = customChange_ ? *customChange_ : shownChange_;
            const int next = edited[cents] + delta;
            if (next < 0) return;
            if (next == 0) edited.erase(cents);
            else edited[cents] = next;
            customChange_ = edited;
            refreshPlan();
            refreshChangeOverlay();
        };
        connect(minus, &QPushButton::clicked, this, [adjust] { adjust(-1); });
        connect(plus, &QPushButton::clicked, this, [adjust] { adjust(+1); });
    }
}

} // namespace pokersoc::ui
