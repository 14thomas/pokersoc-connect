#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "application/CashboxService.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

LostChipsScreen::LostChipsScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    const Columns columns = makeColumns(this);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Lost Chips"));
    chipTable_ = new CountTable({"Chip", "Count", "Value"});
    left->addWidget(chipTable_, 1);

    QFrame* note = makePanel("#FFF8E1", color::DarkGray, 2, 0, 10);
    panelLayout(note)->addWidget(makeLabel(QStringLiteral("House tip"), 12, true));
    panelLayout(note)->addSpacing(4);
    QLabel* noteText = makeLabel("Chips found on the floor or left behind. Submit records their "
                                 "value as a tip. No cash is paid out.",
                                 11, false, "rgba(0,0,0,204)");
    noteText->setWordWrap(true);
    panelLayout(note)->addWidget(noteText);
    left->addWidget(note);

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
    submitButton_ = new TileButton(QStringLiteral("✓"), "Submit", "Record as tip");
    submitButton_->setColors(color::LightBlue, color::Gray, "0px 1px 1px 1px");
    left->addLayout(makePair(back, submitButton_));
    connect(back, &QPushButton::clicked, this, [this] { emit closed(changed_); });
    connect(submitButton_, &QPushButton::clicked, this, [this] { submit(); });
    for (QWidget* tile : {static_cast<QWidget*>(clear), static_cast<QWidget*>(undo),
                          static_cast<QWidget*>(back), static_cast<QWidget*>(submitButton_)}) {
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
    refreshChips();
}

void LostChipsScreen::refreshChips() {
    chipTable_->setCounts(chips_, "Click chip buttons to add chips");
    pad_->setCounts(chips_);
    const std::int64_t total = totalCents(chips_);
    chipTotal_->setText(money(total));
    submitButton_->setEnabled(total > 0);
}

void LostChipsScreen::submit() {
    if (totalCents(chips_) <= 0) return;
    const auto result = ctx_.cashboxOps().recordLostChips(toChipCount(chips_, ctx_.table()),
                                                          "Lost chip recorded as tip");
    if (!result.success) {
        QMessageBox::warning(this, "Lost Chips", "Error recording lost chips: " + toQ(result.message));
        return;
    }
    changed_ = true;
    emit closed(true);
}

} // namespace pokersoc::ui
