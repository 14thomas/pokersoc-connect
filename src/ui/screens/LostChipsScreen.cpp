#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "application/CashboxService.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

namespace {

struct ChipLook {
    std::int64_t cents;
    const char* colourName;
    const char* background;
    const char* border;
    const char* text;      // denomination + count colour
    const char* subtitle;  // colour-name colour
    int subtitlePx;
};

const ChipLook kChips[] = {
    {5, "White", "#FFFFFF", "#000000", "#000000", "#808080", 14},
    {25, "Red", "#E53935", "#8B0000", "#FFFFFF", "#FFCDD2", 14},
    {100, "Blue", "#1E88E5", "#00008B", "#FFFFFF", "#BBDEFB", 14},
    {500, "Black", "#212121", "#333333", "#FFFFFF", "#9E9E9E", 14},
    {2500, "White Plaque", "#FFFFFF", "#FFD700", "#000000", "#808080", 12},
};

} // namespace

LostChipsScreen::LostChipsScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    const Columns columns = makeColumns(this);
    QVBoxLayout* left = columns.left;

    left->addWidget(makeHeading("Lost Chips Summary"));
    auto* table = new QWidget;
    table->setObjectName("lost");
    table->setAttribute(Qt::WA_StyledBackground);
    table->setStyleSheet("QWidget#lost { background: white; }");
    rows_ = new QVBoxLayout(table);
    rows_->setContentsMargins(0, 0, 0, 0);
    rows_->setSpacing(0);
    left->addWidget(table, 1);

    QFrame* info = makePanel("#FFF8E1", color::DarkGray, 2, 0, 10);
    panelLayout(info)->addWidget(makeLabel(QStringLiteral("💡 What are Lost Chips?"), 12, true));
    panelLayout(info)->addSpacing(5);
    QLabel* infoText = makeLabel("Chips found on the floor or left behind by players are recorded "
                                 "as tips for the house.",
                                 11, false, "rgba(0,0,0,204)");
    infoText->setWordWrap(true);
    panelLayout(info)->addWidget(infoText);
    left->addWidget(info);

    auto* clear = new TileButton(QStringLiteral("🗑️"), "Clear", "All chips");
    clear->setColors(color::Orange, color::Gray, "0px 1px 1px 1px");
    auto* undo = new TileButton(QStringLiteral("↶"), "Undo", "Last action");
    undo->setColors(color::Yellow, color::Gray, "0px 1px 1px 1px");
    clear->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    undo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    left->addLayout(makePair(clear, undo));
    left->addWidget(makeInfoTile(QStringLiteral("💰"), "Total Tips", &total_, color::LightGreen));
    connect(clear, &QPushButton::clicked, this, [this] {
        for (const auto& [cents, qty] : chips_) {
            if (qty > 0) history_.emplace_back(cents, qty);
        }
        chips_.clear();
        refresh();
    });
    connect(undo, &QPushButton::clicked, this, [this] {
        if (history_.empty()) return;
        const auto [cents, before] = history_.back();
        history_.pop_back();
        chips_[cents] = before;
        refresh();
    });

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    for (int i = 0; i < 5; ++i) {
        const ChipLook& look = kChips[i];
        auto* button = new TileButton(denomLabel(look.cents), look.colourName, QStringLiteral("×0"));
        button->setSizes(36, look.subtitlePx, 16);
        button->iconLabel()->setFont(uiFont(36, true));
        button->iconLabel()->setStyleSheet(QStringLiteral("color:%1;").arg(look.text));
        button->titleLabel()->setStyleSheet(QStringLiteral("color:%1;").arg(look.subtitle));
        button->subtitleLabel()->setStyleSheet(
            QStringLiteral("color:%1;").arg(QString(look.text) == "#000000" ? "rgba(0,0,0,178)"
                                                                             : "rgba(255,255,255,204)"));
        button->setColors(look.background, look.border, "3px");
        countLabels_.emplace_back(look.cents, button->subtitleLabel());
        const std::int64_t cents = look.cents;
        connect(button, &QPushButton::clicked, this, [this, cents] {
            int add = multipliers_->value();
            if (add == 5) add = cents == 25 || cents == 2500 ? 4 : 5;
            history_.emplace_back(cents, chips_[cents]);
            chips_[cents] += add;
            refresh();
        });
        grid->addWidget(button, i / 2, i % 2);
    }
    auto* spacer = new QPushButton;
    spacer->setEnabled(false);
    spacer->setStyleSheet(buttonStyle(color::ButtonFace, color::Gray, "3px"));
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(spacer, 2, 1);

    auto* back = new TileButton(QStringLiteral("🏠"), "Back", "Return to main");
    back->setColors(color::LightCoral);
    auto* record = new TileButton(QStringLiteral("✅"), "Record", "Save as tips");
    record->setColors(color::LightBlue);
    grid->addWidget(back, 3, 0);
    grid->addWidget(record, 3, 1);
    makeUniform(grid, 4, 2);
    columns.right->addLayout(grid, 1);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(changed_); });
    connect(record, &QPushButton::clicked, this, [this] { showConfirmation(); });

    multipliers_ = new MultiplierBar({{QStringLiteral("×1"), 1}, {QStringLiteral("×4/×5"), 5},
                                      {QStringLiteral("×20"), 20}},
                                     28);
    for (QPushButton* button : multipliers_->findChildren<QPushButton*>()) {
        button->setFont(uiFont(28, true));
        button->setMinimumHeight(60);
    }
    columns.right->addWidget(multipliers_);

    // Confirmation overlay
    confirm_ = new Overlay(this);
    confirm_->card()->setMaximumWidth(500);
    QVBoxLayout* card = confirm_->cardLayout();
    card->addWidget(makeLabel(QStringLiteral("💰"), 48, false, QString(), Qt::AlignCenter));
    card->addWidget(makeLabel("Confirm Lost Chips", 24, true, QString(), Qt::AlignCenter));
    card->addSpacing(20);
    confirmText_ = makeLabel(QString(), 16, false, QString(), Qt::AlignCenter);
    confirmText_->setWordWrap(true);
    card->addWidget(confirmText_);
    card->addSpacing(25);
    QPushButton* cancel = makeButton("Cancel", color::LightGray, 16);
    cancel->setStyleSheet(buttonStyle(color::LightGray, "#707070", "1px", "padding:12px 20px;"));
    QPushButton* ok = makeButton("Confirm", color::LightGreen, 16, true);
    ok->setStyleSheet(buttonStyle(color::LightGreen, "#707070", "1px", "padding:12px 20px;"));
    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->addWidget(cancel);
    buttons->addSpacing(10);
    buttons->addWidget(ok);
    buttons->addStretch(1);
    card->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, confirm_, &QWidget::hide);
    connect(ok, &QPushButton::clicked, this, [this] {
        confirm_->hide();
        if (totalCents(chips_) <= 0) return;
        const auto result = ctx_.cashboxOps().recordLostChips(toChipCount(chips_, ctx_.table()),
                                                              "Lost chip recorded as tip");
        if (!result.success) {
            QMessageBox::warning(this, "Lost Chips",
                                 "Error recording lost chips: " + toQ(result.message));
            return;
        }
        changed_ = true;
        chips_.clear();
        history_.clear();
        refresh();
    });

    refresh();
}

void LostChipsScreen::refresh() {
    clearLayout(rows_);
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
        const int widths[] = {100, 80, 100};
        const QString texts[] = {a, b, c};
        for (int i = 0; i < 3; ++i) {
            QLabel* label = makeLabel(texts[i], header ? 14 : 13, header || i == 1, QString(),
                                      Qt::AlignCenter);
            label->setFixedWidth(widths[i]);
            line->addWidget(label);
        }
        line->addStretch(1);
        return row;
    };
    rows_->addWidget(makeRow("Chip", "Count", "Value", true));

    bool any = false;
    for (const ChipLook& look : kChips) {
        const auto it = chips_.find(look.cents);
        if (it == chips_.end() || it->second <= 0) continue;
        any = true;
        rows_->addWidget(makeRow(denomLabel(look.cents) + " " + look.colourName,
                                 QString::number(it->second), money(look.cents * it->second), false));
    }
    if (!any) {
        auto* empty = new QFrame;
        empty->setObjectName("e");
        empty->setStyleSheet("QFrame#e { background:#FAFAFA; }");
        empty->setFixedHeight(50);
        auto* line = new QHBoxLayout(empty);
        QLabel* text = makeLabel("Click chips on the right to add them", 13, false, color::Gray,
                                 Qt::AlignCenter);
        QFont italic = text->font();
        italic.setItalic(true);
        text->setFont(italic);
        line->addWidget(text);
        rows_->addWidget(empty);
    }
    rows_->addStretch(1);

    for (const auto& [cents, label] : countLabels_) {
        const auto it = chips_.find(cents);
        label->setText(QStringLiteral("×%1").arg(it == chips_.end() ? 0 : it->second));
    }
    total_->setText(money(totalCents(chips_)));
}

void LostChipsScreen::showConfirmation() {
    int pieces = 0;
    QString lines;
    for (const ChipLook& look : kChips) {
        const auto it = chips_.find(look.cents);
        if (it == chips_.end() || it->second <= 0) continue;
        pieces += it->second;
        lines += QStringLiteral("  • %1 (%2): %3\n")
                     .arg(denomLabel(look.cents), look.colourName)
                     .arg(it->second);
    }
    if (pieces == 0) {
        confirmText_->setText("Please click on at least one chip to record as tips.");
    } else {
        confirmText_->setText(QStringLiteral("Record %1 lost chip%2 as tips?\n\n%3\nTotal value: %4")
                                  .arg(pieces)
                                  .arg(pieces > 1 ? QStringLiteral("s") : QString(), lines, money(totalCents(chips_))));
    }
    confirm_->open();
}

} // namespace pokersoc::ui
