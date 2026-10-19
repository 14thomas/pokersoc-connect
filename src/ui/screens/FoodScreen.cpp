#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "application/CashOutService.h"
#include "application/FoodService.h"
#include "application/ReportService.h"
#include "domain/cashbox/Cashbox.h"
#include "domain/food/ProductCatalog.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

using namespace domain;

namespace {

constexpr int kSlots = 5;
const char* const kSlotIcons[kSlots] = {"🍕", "🍔", "🥤", "🍿", "🍰"};

QGroupBox* makeGroup(const QString& title, int padding) {
    auto* group = new QGroupBox(title);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(padding, padding, padding, padding);
    layout->setSpacing(0);
    return group;
}

QLabel* instruction(const QString& text) {
    QLabel* label = makeLabel(text, 14);
    label->setWordWrap(true);
    label->setContentsMargins(0, 0, 0, 10);
    return label;
}

/// Text-only denomination button: big label, small label, "count" underneath.
TileButton* makeTextDenom(const QString& label) {
    auto* button = new TileButton(label, label, "0");
    button->setSizes(36, 18, 10);
    button->layout()->setContentsMargins(15, 2, 15, 2);
    return button;
}

} // namespace

FoodScreen::FoodScreen(SessionContext& context, std::string playerId, QWidget* parent)
    : Screen(context, parent), playerId_(std::move(playerId)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    pages_ = new QStackedWidget;
    layout->addWidget(pages_);
    mainPage_ = buildMainPage();
    selectPage_ = buildSelectPage();
    paymentPage_ = buildPaymentPage();
    pages_->addWidget(mainPage_);
    pages_->addWidget(selectPage_);
    pages_->addWidget(paymentPage_);
    refreshMain();
}

// ---------------------------------------------------------------------------
// Main page: quick slots

QWidget* FoodScreen::buildMainPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;
    left->addWidget(makeHeading("Food Sales"));

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    QGroupBox* instructions = makeGroup("Instructions", 10);
    instructions->layout()->addWidget(instruction("1. Click on an empty slot to assign a product"));
    instructions->layout()->addWidget(instruction("2. Click on a filled slot to sell that item"));
    instructions->layout()->addWidget(instruction("3. Use Settings to manage your product catalog"));
    contentLayout->addWidget(instructions);
    contentLayout->addSpacing(15);
    QGroupBox* stats = makeGroup("Quick Stats", 10);
    productsStat_ = makeLabel(QString(), 16);
    productsStat_->setContentsMargins(0, 0, 0, 5);
    slotsStat_ = makeLabel(QString(), 16);
    stats->layout()->addWidget(productsStat_);
    stats->layout()->addWidget(slotsStat_);
    contentLayout->addWidget(stats);
    contentLayout->addStretch(1);
    scroll->setWidget(content);
    left->addWidget(scroll, 1);

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    for (int i = 0; i < kSlots; ++i) {
        auto* slot = new TileButton(QString::fromUtf8(kSlotIcons[i]), "Empty Slot", "Click to assign");
        const int number = i + 1;
        connect(slot, &QPushButton::clicked, this, [this, number] { slotClicked(number); });
        grid->addWidget(slot, i / 2, i % 2);
        slots_.push_back(slot);
    }
    grid->addWidget(makeInfoTile(QStringLiteral("💰"), "Total Made", &totalMade_, color::LightYellow,
                                 color::Gold, "2px"),
                    2, 1);
    auto* settings = new TileButton(QStringLiteral("⚙️"), "Settings", "Manage items");
    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral);
    grid->addWidget(settings, 3, 0);
    grid->addWidget(back, 3, 1);
    makeUniform(grid, 4, 2);
    for (int i = 0; i < grid->count(); ++i) {
        grid->itemAt(i)->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    columns.right->addLayout(grid);

    connect(settings, &QPushButton::clicked, this, [this] { emit openSettings(); });
    connect(back, &QPushButton::clicked, this, [this] { emit closed(changed_); });
    return page;
}

void FoodScreen::refreshMain() {
    const ProductCatalog& catalog = ctx_.catalog();
    int filled = 0;
    for (int i = 0; i < kSlots; ++i) {
        TileButton* slot = slots_[i];
        if (const Product* product = catalog.slot(i + 1)) {
            ++filled;
            slot->setIconText(QStringLiteral("🍕"));
            slot->setTitle(toQ(product->name));
            slot->setSubtitle(money(product->price));
            slot->setColors(color::LightGreen, color::Green);
        } else {
            slot->setIconText(QString::fromUtf8(kSlotIcons[i]));
            slot->setTitle("Empty Slot");
            slot->setSubtitle("Click to assign");
            slot->setColors(color::White, color::LightGray);
        }
    }
    productsStat_->setText(QStringLiteral("Products: %1").arg(catalog.size()));
    slotsStat_->setText(QStringLiteral("Filled Slots: %1/%2").arg(filled).arg(kSlots));
    totalMade_->setText(money(ctx_.reports().summary().foodSalesTotal));
}

void FoodScreen::slotClicked(int slot) {
    if (const Product* product = ctx_.catalog().slot(slot)) {
        startPayment(product->id);
        return;
    }
    if (ctx_.catalog().empty()) {
        QMessageBox::information(this, "Food Sales",
                                 "There are no products yet. Use Settings to add food items.");
        return;
    }
    selectingSlot_ = slot;
    clearLayout(productList_);
    for (const Product* product : ctx_.catalog().all()) {
        QPushButton* button =
            makeButton(QStringLiteral("🍕 ") + toQ(product->name) + " - " + money(product->price),
                       QString(), 16, true);
        button->setFixedHeight(60);
        const long long id = product->id;
        connect(button, &QPushButton::clicked, this, [this, id] {
            ctx_.food().setSlot(selectingSlot_, id);
            selectingSlot_ = 0;
            refreshMain();
            pages_->setCurrentWidget(mainPage_);
        });
        productList_->addWidget(button);
    }
    productList_->addStretch(1);
    pages_->setCurrentWidget(selectPage_);
}

// ---------------------------------------------------------------------------
// Product selection page

QWidget* FoodScreen::buildSelectPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;
    left->addWidget(makeHeading("Select Product"));
    QGroupBox* instructions = makeGroup("Instructions", 10);
    instructions->layout()->addWidget(
        instruction("Choose a product from the list on the right to assign to this slot."));
    instructions->layout()->addWidget(
        instruction("The product will appear on the main screen for quick sales."));
    left->addWidget(instructions);
    left->addStretch(1);
    auto* back = new TileButton(QStringLiteral("⬅️"), "Back", "Cancel selection");
    back->setColors(color::LightCoral);
    back->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    left->addWidget(back);
    connect(back, &QPushButton::clicked, this, [this] {
        selectingSlot_ = 0;
        refreshMain();
        pages_->setCurrentWidget(mainPage_);
    });

    columns.right->addWidget(makeHeading("Available Products", 16));
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* list = new QWidget;
    productList_ = new QVBoxLayout(list);
    productList_->setContentsMargins(0, 0, 0, 0);
    productList_->setSpacing(10);
    scroll->setWidget(list);
    columns.right->addWidget(scroll, 1);
    return page;
}

// ---------------------------------------------------------------------------
// Payment page

QWidget* FoodScreen::buildPaymentPage() {
    auto* page = new QWidget;
    const Columns columns = makeColumns(page);
    QVBoxLayout* left = columns.left;
    left->addWidget(makeHeading("Payment"));

    QGroupBox* details = makeGroup("Sale Details", 10);
    productName_ = makeLabel(QString(), 16, true);
    productPrice_ = makeLabel(QString(), 24, true, color::Blue);
    details->layout()->addWidget(productName_);
    details->layout()->addWidget(productPrice_);
    QLabel* playerCaption = makeLabel("Player ID (Optional):", 12);
    playerCaption->setContentsMargins(0, 10, 0, 5);
    details->layout()->addWidget(playerCaption);
    playerBox_ = new QLineEdit;
    playerBox_->setFont(uiFont(14));
    details->layout()->addWidget(playerBox_);
    left->addWidget(details);

    QGroupBox* summary = makeGroup("Payment Summary", 15);
    summary_ = makeLabel(QString(), 20, true, QString(), Qt::AlignCenter);
    summary_->setWordWrap(true);
    changeText_ = makeLabel(QString(), 18, true, QString(), Qt::AlignCenter);
    changeText_->setContentsMargins(0, 5, 0, 10);
    summary->layout()->addWidget(summary_);
    summary->layout()->addWidget(changeText_);
    changeBreakdown_ = new QWidget;
    auto* breakdownLayout = new QVBoxLayout(changeBreakdown_);
    breakdownLayout->setContentsMargins(0, 0, 0, 0);
    breakdownLayout->addWidget(makeLabel("Change Breakdown:", 16, true));
    auto* breakdownScroll = new QScrollArea;
    breakdownScroll->setWidgetResizable(true);
    breakdownScroll->setMaximumHeight(150);
    auto* rows = new QWidget;
    changeRows_ = new QVBoxLayout(rows);
    changeRows_->setContentsMargins(0, 0, 0, 0);
    changeRows_->setSpacing(2);
    breakdownScroll->setWidget(rows);
    breakdownLayout->addWidget(breakdownScroll);
    summary->layout()->addWidget(changeBreakdown_);
    left->addWidget(summary);

    QGroupBox* method = makeGroup("Payment Method", 10);
    methodToggle_ = new TileButton(QStringLiteral("🪙"), "Chips", "Click to toggle");
    methodToggle_->setColors(color::LightBlue);
    methodToggle_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    method->layout()->addWidget(methodToggle_);
    left->addWidget(method);
    connect(methodToggle_, &QPushButton::clicked, this, [this] { setCashMode(!cashMode_); });

    left->addStretch(1);
    auto* back = new TileButton(QStringLiteral("⬅️"), "Back", "Edit item");
    back->setColors(color::LightCoral);
    confirmButton_ = new TileButton(QStringLiteral("✅"), "Confirm", "Process payment");
    confirmButton_->setColors(color::LightGreen);
    back->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    confirmButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    left->addLayout(makePair(back, confirmButton_));
    connect(back, &QPushButton::clicked, this, [this] {
        productId_ = 0;
        refreshMain();
        pages_->setCurrentWidget(mainPage_);
    });
    connect(confirmButton_, &QPushButton::clicked, this, [this] { confirmPayment(); });

    // Right: cash pad / chip pad
    padStack_ = new QStackedWidget;
    columns.right->addWidget(padStack_);

    auto* cashPad = new QWidget;
    auto* cashGrid = new QGridLayout(cashPad);
    cashGrid->setContentsMargins(0, 0, 0, 0);
    cashGrid->setSpacing(10);
    const std::int64_t cashFaces[] = {5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    for (int i = 0; i < 10; ++i) {
        const std::int64_t cents = cashFaces[i];
        TileButton* button = makeTextDenom(denomLabel(cents));
        cashCounts_.emplace_back(cents, button->subtitleLabel());
        connect(button, &QPushButton::clicked, this, [this, cents] {
            ++cash_[cents];
            refreshPayment();
        });
        cashGrid->addWidget(button, i / 2, i % 2);
    }
    TileButton* hundred = makeTextDenom("$100");
    cashCounts_.emplace_back(10000, hundred->subtitleLabel());
    connect(hundred, &QPushButton::clicked, this, [this] {
        ++cash_[10000];
        refreshPayment();
    });
    cashGrid->addWidget(hundred, 5, 1);
    auto* cashClear = new TileButton(QStringLiteral("🗑️"), "Clear", "Reset all");
    cashClear->setColors(color::Orange);
    connect(cashClear, &QPushButton::clicked, this, [this] {
        cash_.clear();
        refreshPayment();
    });
    cashGrid->addWidget(cashClear, 5, 0);
    makeUniform(cashGrid, 6, 2);
    padStack_->addWidget(cashPad);

    auto* chipPad = new QWidget;
    auto* chipGrid = new QGridLayout(chipPad);
    chipGrid->setContentsMargins(0, 0, 0, 0);
    chipGrid->setSpacing(10);
    const std::int64_t chipFaces[] = {5, 25, 100, 500, 2500};
    chipMultiplier_ = new MultiplierBar({{QStringLiteral("×1"), 1}, {QStringLiteral("×4/×5"), 5},
                                         {QStringLiteral("×20"), 20}});
    for (int i = 0; i < 5; ++i) {
        const std::int64_t cents = chipFaces[i];
        TileButton* button = makeTextDenom(denomLabel(cents));
        chipCounts_.emplace_back(cents, button->subtitleLabel());
        connect(button, &QPushButton::clicked, this, [this, cents] {
            int add = chipMultiplier_->value();
            if (add == 5) add = cents == 25 || cents == 2500 ? 4 : 5;
            chips_[cents] += add;
            refreshPayment();
        });
        chipGrid->addWidget(button, i / 2, i % 2);
    }
    for (int cell = 5; cell < 10; ++cell) {
        auto* filler = new QPushButton;
        filler->setEnabled(false);
        filler->setStyleSheet(buttonStyle(color::ButtonFace));
        filler->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        chipGrid->addWidget(filler, cell / 2, cell % 2);
    }
    auto* chipClear = new TileButton(QStringLiteral("🗑️"), "Clear", "Reset all");
    chipClear->setColors(color::Orange);
    connect(chipClear, &QPushButton::clicked, this, [this] {
        chips_.clear();
        refreshPayment();
    });
    chipGrid->addWidget(chipClear, 5, 0);
    chipGrid->addWidget(chipMultiplier_, 5, 1);
    makeUniform(chipGrid, 6, 2);
    padStack_->addWidget(chipPad);
    return page;
}

void FoodScreen::startPayment(long long productId) {
    const Product* product = ctx_.catalog().find(productId);
    if (!product) return;
    productId_ = productId;
    cash_.clear();
    chips_.clear();
    chipMultiplier_->setValue(1);
    productName_->setText(toQ(product->name));
    productPrice_->setText(money(product->price));
    playerBox_->setText(toQ(playerId_));

    const bool chipsAllowed = ctx_.food().chipPaymentAllowed();
    methodToggle_->setEnabled(chipsAllowed);
    setCashMode(!chipsAllowed);
    pages_->setCurrentWidget(paymentPage_);
}

void FoodScreen::setCashMode(bool cash) {
    cashMode_ = cash;
    cash_.clear();
    chips_.clear();
    methodToggle_->setIconText(cash ? QStringLiteral("💰") : QStringLiteral("🪙"));
    methodToggle_->setTitle(cash ? "Cash" : "Chips");
    padStack_->setCurrentIndex(cash ? 0 : 1);
    refreshPayment();
}

void FoodScreen::refreshPayment() {
    for (const auto& [cents, label] : cashCounts_) {
        const auto it = cash_.find(cents);
        label->setText(QString::number(it == cash_.end() ? 0 : it->second));
    }
    for (const auto& [cents, label] : chipCounts_) {
        const auto it = chips_.find(cents);
        label->setText(QString::number(it == chips_.end() ? 0 : it->second));
    }

    const Product* product = ctx_.catalog().find(productId_);
    if (!product) return;
    const std::int64_t price = product->price.cents();
    const Counts& paidWith = cashMode_ ? cash_ : chips_;
    const std::int64_t paid = totalCents(paidWith);
    summary_->setText(QStringLiteral("Product: %1 | Payment: %2").arg(money(price), money(paid)));

    clearLayout(changeRows_);
    changeBreakdown_->hide();
    if (paid < price) {
        changeText_->setText("Owing: " + money(price - paid));
        changeText_->setStyleSheet(QStringLiteral("color:%1;").arg(color::Red));
        confirmButton_->setEnabled(false);
        return;
    }
    const Money change = application::roundToFiveCents(Money(paid - price));
    changeText_->setText("Change: " + money(change));
    changeText_->setStyleSheet(QStringLiteral("color:%1;").arg(color::Green));
    confirmButton_->setEnabled(true);

    if (!cashMode_ || change.isZero()) return;
    Cashbox probe = ctx_.cashbox();
    probe.applyCount(toCurrencyCount(cash_, ctx_.table()), +1, MovementReason::Sale);
    const auto made = probe.makeChange(change);
    if (!made) {
        changeRows_->addWidget(makeLabel("Not enough cash in the cashbox for this change.", 12, true,
                                         color::Red));
    } else {
        for (const auto& entry : made->entries()) {
            const std::int64_t cents = entry.denomination->cents();
            auto* row = new QWidget;
            auto* line = new QHBoxLayout(row);
            line->setContentsMargins(0, 2, 0, 2);
            QLabel* face = makeLabel(denomLabel(cents), 12, true);
            face->setFixedWidth(60);
            QLabel* qty = makeLabel(QStringLiteral("x%1").arg(entry.qty), 12, false, QString(),
                                    Qt::AlignCenter);
            qty->setFixedWidth(40);
            QLabel* amount = makeLabel(money(cents * entry.qty), 12, true, QString(), Qt::AlignRight);
            amount->setFixedWidth(60);
            line->addWidget(face);
            line->addWidget(qty);
            line->addWidget(amount);
            line->addStretch(1);
            changeRows_->addWidget(row);
        }
    }
    changeRows_->addStretch(1);
    changeBreakdown_->show();
}

void FoodScreen::confirmPayment() {
    const Product* product = ctx_.catalog().find(productId_);
    if (!product) {
        QMessageBox::critical(this, "Error", "No product selected for payment.");
        return;
    }
    const Counts& paidWith = cashMode_ ? cash_ : chips_;
    if (totalCents(paidWith) < product->price.cents()) {
        QMessageBox::warning(this, "Insufficient Payment", "Payment amount is less than product price.");
        return;
    }
    const std::vector<SaleLine> lines{SaleLine{product->name, product->price, 1}};
    const std::string playerId = toStd(playerBox_->text().trimmed());
    const auto result =
        cashMode_ ? ctx_.food().sellForCash(playerId, lines, toCurrencyCount(cash_, ctx_.table()))
                  : ctx_.food().sellForChips(playerId, lines, toChipCount(chips_, ctx_.table()));
    if (!result.success) {
        QMessageBox::critical(this, "Error", "Error processing payment: " + toQ(result.message));
        return;
    }
    changed_ = true;
    productId_ = 0;
    refreshMain();
    pages_->setCurrentWidget(mainPage_);
}

} // namespace pokersoc::ui
