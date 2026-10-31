#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "application/PlayerService.h"
#include "application/PrizeService.h"
#include "application/ReportService.h"
#include "domain/player/Player.h"
#include "domain/session/TournamentMode.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/SettingsChrome.h"
#include "ui/Style.h"
#include "ui/board/TournamentClock.h"
#include "ui/screens/TournamentEditors.h"
#include "ui/Widgets.h"

#include <QAbstractButton>
#include <QFrame>
#include <QMessageBox>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <exception>

namespace pokersoc::ui {

using namespace domain;

namespace {

QGroupBox* makeGroup(const QString& title) {
    auto* group = new QGroupBox(title);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);
    return group;
}

QVBoxLayout* groupLayout(QGroupBox* group) { return static_cast<QVBoxLayout*>(group->layout()); }

QLabel* hint(const QString& text) {
    QLabel* label = makeLabel(text, 11, false, color::Gray);
    label->setWordWrap(true);
    label->setContentsMargins(0, 8, 0, 0);
    return label;
}

QScrollArea* makeScroll(QWidget* content) {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

QPushButton* footerButton(const QString& text, int width, bool bold) {
    auto* button = new QPushButton(text);
    button->setFont(uiFont(12, bold));
    button->setFixedWidth(width);
    button->setStyleSheet("QPushButton { padding:8px 12px; }");
    return button;
}

QPushButton* actionButton(const QString& text, const QString& background) {
    QPushButton* button = makeButton(text, background, 15, true);
    button->setMinimumHeight(52);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

QString dollars(std::int64_t cents) { return QString::number(cents / 100.0, 'f', 2); }

QString placeOrdinal(int place) {
    const int mod100 = place % 100;
    const int mod10 = place % 10;
    const char* suffix = "th";
    if (mod100 < 11 || mod100 > 13) {
        if (mod10 == 1) suffix = "st";
        else if (mod10 == 2) suffix = "nd";
        else if (mod10 == 3) suffix = "rd";
    }
    return QString::number(place) + suffix;
}

bool parseDollars(const QString& text, std::int64_t& cents) {
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || value < 0 || !std::isfinite(value)) return false;
    cents = static_cast<std::int64_t>(std::llround(value * 100.0));
    return true;
}

struct FeeRow {
    QString label;
    std::int64_t TournamentConfig::*price;
    BuyInFees TournamentConfig::*fees;
    bool hideWhenArcOnly = false;
    QLabel* nameLabel = nullptr;
    QLineEdit* priceBox = nullptr;
    QLineEdit* bountyBox = nullptr;
    QLineEdit* rakeBox = nullptr;
    QLabel* prizeLabel = nullptr;
};

void setFeeRowVisible(FeeRow& row, bool visible) {
    for (QWidget* widget : {static_cast<QWidget*>(row.nameLabel), static_cast<QWidget*>(row.priceBox),
                            static_cast<QWidget*>(row.bountyBox), static_cast<QWidget*>(row.rakeBox),
                            static_cast<QWidget*>(row.prizeLabel)}) {
        if (widget) widget->setVisible(visible);
    }
}

void refreshFeeRow(FeeRow& row) {
    std::int64_t buyIn = 0;
    std::int64_t bounty = 0;
    std::int64_t rake = 0;
    parseDollars(row.priceBox->text(), buyIn);
    parseDollars(row.bountyBox->text(), bounty);
    parseDollars(row.rakeBox->text(), rake);
    if (buyIn < 0) buyIn = 0;
    if (bounty < 0) bounty = 0;
    if (rake < 0) rake = 0;
    std::int64_t remaining = buyIn;
    remaining -= std::min(rake, remaining);
    remaining -= std::min(bounty, remaining);
    row.prizeLabel->setText(money(remaining));
}

bool readFeeRows(const std::vector<FeeRow>& rows, TournamentConfig& config, QString& error) {
    for (const FeeRow& row : rows) {
        std::int64_t buyIn = 0;
        std::int64_t bounty = 0;
        std::int64_t rake = 0;
        if (!parseDollars(row.priceBox->text(), buyIn) || !parseDollars(row.bountyBox->text(), bounty) ||
            !parseDollars(row.rakeBox->text(), rake)) {
            error = "Enter valid buy-in, bounty, and rake amounts for each category.";
            return false;
        }
        config.*row.price = buyIn;
        (config.*row.fees).bountyCents = bounty;
        (config.*row.fees).rakeCents = rake;
    }
    return true;
}

QGroupBox* makeFeeGroup(const QString& title, std::vector<FeeRow>& rows, const TournamentConfig& config) {
    QGroupBox* group = makeGroup(title);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->addWidget(makeLabel("Category", 12, true), 0, 0);
    grid->addWidget(makeLabel("Buy-in ($)", 12, true), 0, 1);
    grid->addWidget(makeLabel("Bounty ($)", 12, true), 0, 2);
    grid->addWidget(makeLabel("Rake ($)", 12, true), 0, 3);
    grid->addWidget(makeLabel("To prizes", 12, true), 0, 4);
    int gridRow = 1;
    for (FeeRow& row : rows) {
        const BuyInFees& fees = config.*row.fees;
        row.nameLabel = makeLabel(row.label);
        grid->addWidget(row.nameLabel, gridRow, 0);
        row.priceBox = new QLineEdit(dollars(config.*row.price));
        row.priceBox->setFixedWidth(90);
        row.bountyBox = new QLineEdit(dollars(fees.bountyCents));
        row.bountyBox->setFixedWidth(90);
        row.rakeBox = new QLineEdit(dollars(fees.rakeCents));
        row.rakeBox->setFixedWidth(90);
        row.prizeLabel = makeLabel(QString(), 13, true);
        grid->addWidget(row.priceBox, gridRow, 1);
        grid->addWidget(row.bountyBox, gridRow, 2);
        grid->addWidget(row.rakeBox, gridRow, 3);
        grid->addWidget(row.prizeLabel, gridRow, 4);
        const auto refresh = [&row] { refreshFeeRow(row); };
        QObject::connect(row.priceBox, &QLineEdit::textChanged, row.priceBox, refresh);
        QObject::connect(row.bountyBox, &QLineEdit::textChanged, row.bountyBox, refresh);
        QObject::connect(row.rakeBox, &QLineEdit::textChanged, row.rakeBox, refresh);
        refresh();
        ++gridRow;
    }
    grid->setColumnStretch(0, 1);
    groupLayout(group)->addLayout(grid);
    return group;
}

} // namespace

// ---------------------------------------------------------------------------

TournamentPricingScreen::TournamentPricingScreen(SessionContext& context, QWidget* parent)
    : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Pricing Structure", &close));
    layout->addSpacing(16);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(false); });

    const TournamentConfig& config = ctx_.tournament().config();
    auto* content = new QWidget;
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(0, 0, 8, 0);
    sections->setSpacing(16);

    QGroupBox* type = makeGroup("Type of Pricing Structure");
    auto* typeRow = new QHBoxLayout;
    typeRow->setSpacing(16);
    auto* arcType = new TileButton(QString(), "ARC vs Non-ARC",
                                    "Buy-in, bounty, and rake for ARC and non-ARC");
    auto* tierType = new TileButton(QString(), "Membership Tiers",
                                    "Buy-in, bounty, and rake for Bronze through Diamond");
    for (TileButton* button : {arcType, tierType}) {
        button->iconLabel()->hide();
        button->setSizes(18, 18, 12);
        button->subtitleLabel()->setWordWrap(true);
        button->subtitleLabel()->setStyleSheet("color:rgba(0,0,0,191);");
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        typeRow->addWidget(button);
    }
    groupLayout(type)->addLayout(typeRow);
    sections->addWidget(type);

    // ARC vs Non-ARC fields
    auto* arcPanel = new QWidget;
    auto* arcLayout = new QVBoxLayout(arcPanel);
    arcLayout->setContentsMargins(0, 0, 0, 0);
    arcLayout->setSpacing(16);
    QGroupBox* options = makeGroup("Event Options");
    auto* arcOnly = new QCheckBox("ARC Only (non-ARC buy-ins disabled)");
    arcOnly->setFont(uiFont(14));
    arcOnly->setChecked(config.arcOnly);
    groupLayout(options)->addWidget(arcOnly);
    QLabel* arcHint = hint("When enabled, the buy-in screen only offers ARC member pricing.");
    arcHint->setContentsMargins(0, 8, 0, 0);
    groupLayout(options)->addWidget(arcHint);
    arcLayout->addWidget(options);

    auto arcBuyIns = std::make_shared<std::vector<FeeRow>>(std::vector<FeeRow>{
        {"ARC Member", &TournamentConfig::arcBuyInCents, &TournamentConfig::arcFees},
        {"Non-ARC Member", &TournamentConfig::nonArcBuyInCents, &TournamentConfig::nonArcFees, true},
    });
    auto arcRebuys = std::make_shared<std::vector<FeeRow>>(std::vector<FeeRow>{
        {"ARC Member", &TournamentConfig::arcRebuyCents, &TournamentConfig::arcRebuyFees},
        {"Non-ARC Member", &TournamentConfig::nonArcRebuyCents, &TournamentConfig::nonArcRebuyFees, true},
    });
    arcLayout->addWidget(makeFeeGroup("Initial Buy-ins", *arcBuyIns, config));
    arcLayout->addWidget(makeFeeGroup("Rebuys", *arcRebuys, config));
    sections->addWidget(arcPanel);

    // Membership tier fields
    auto* tierPanel = new QWidget;
    auto* tierLayout = new QVBoxLayout(tierPanel);
    tierLayout->setContentsMargins(0, 0, 0, 0);
    tierLayout->setSpacing(16);
    auto tierBuyIns = std::make_shared<std::vector<FeeRow>>(std::vector<FeeRow>{
        {"Bronze", &TournamentConfig::bronzeBuyInCents, &TournamentConfig::bronzeFees},
        {"Silver", &TournamentConfig::silverBuyInCents, &TournamentConfig::silverFees},
        {"Gold", &TournamentConfig::goldBuyInCents, &TournamentConfig::goldFees},
        {"Platinum", &TournamentConfig::platinumBuyInCents, &TournamentConfig::platinumFees},
        {"Diamond", &TournamentConfig::diamondBuyInCents, &TournamentConfig::diamondFees},
    });
    auto tierRebuys = std::make_shared<std::vector<FeeRow>>(std::vector<FeeRow>{
        {"Bronze", &TournamentConfig::bronzeRebuyCents, &TournamentConfig::bronzeRebuyFees},
        {"Silver", &TournamentConfig::silverRebuyCents, &TournamentConfig::silverRebuyFees},
        {"Gold", &TournamentConfig::goldRebuyCents, &TournamentConfig::goldRebuyFees},
        {"Platinum", &TournamentConfig::platinumRebuyCents, &TournamentConfig::platinumRebuyFees},
        {"Diamond", &TournamentConfig::diamondRebuyCents, &TournamentConfig::diamondRebuyFees},
    });
    tierLayout->addWidget(makeFeeGroup("Initial Buy-ins", *tierBuyIns, config));
    tierLayout->addWidget(makeFeeGroup("Rebuys", *tierRebuys, config));
    sections->addWidget(tierPanel);
    sections->addStretch(1);
    layout->addWidget(makeScroll(content), 1);

    auto selected = std::make_shared<TournamentPricing>(config.pricing);
    const auto applyArcOnly = [arcBuyIns, arcRebuys](bool on) {
        for (auto* rows : {arcBuyIns.get(), arcRebuys.get()}) {
            for (FeeRow& row : *rows) {
                if (row.hideWhenArcOnly) setFeeRowVisible(row, !on);
            }
        }
    };
    const auto applyType = [=] {
        const bool arc = *selected == TournamentPricing::ArcVsNonArc;
        arcPanel->setVisible(arc);
        tierPanel->setVisible(!arc);
        arcType->setColors(arc ? "#E8F4FD" : "#F5F5F5", arc ? "#2E6DA4" : color::Gray, "3px");
        tierType->setColors(arc ? "#F5F5F5" : "#E8F4FD", arc ? color::Gray : "#2E6DA4", "3px");
        if (arc) applyArcOnly(arcOnly->isChecked());
    };
    applyType();
    connect(arcOnly, &QCheckBox::toggled, this, applyArcOnly);
    connect(arcType, &QPushButton::clicked, this, [=] {
        *selected = TournamentPricing::ArcVsNonArc;
        applyType();
    });
    connect(tierType, &QPushButton::clicked, this, [=] {
        *selected = TournamentPricing::MembershipTiers;
        applyType();
    });

    layout->addSpacing(16);
    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    QPushButton* back = footerButton("Back", 100, false);
    QPushButton* save = footerButton("Save Pricing", 140, true);
    footer->addWidget(back);
    footer->addSpacing(8);
    footer->addWidget(save);
    layout->addLayout(footer);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });
    connect(save, &QPushButton::clicked, this,
            [this, selected, arcOnly, arcBuyIns, arcRebuys, tierBuyIns, tierRebuys] {
                TournamentConfig updated = ctx_.tournament().config();
                const bool arc = *selected == TournamentPricing::ArcVsNonArc;
                QString problem;
                const bool valid = arc ? readFeeRows(*arcBuyIns, updated, problem) &&
                                             readFeeRows(*arcRebuys, updated, problem)
                                       : readFeeRows(*tierBuyIns, updated, problem) &&
                                             readFeeRows(*tierRebuys, updated, problem);
                if (!valid) {
                    QMessageBox::warning(this, "Validation Error", problem);
                    return;
                }
                updated.pricing = *selected;
                updated.arcOnly = arc && arcOnly->isChecked();
                ctx_.tournament().config() = updated;
                try {
                    ctx_.saveTournamentConfig();
                } catch (const std::exception& error) {
                    QMessageBox::critical(this, "Error",
                                          QStringLiteral("Could not save pricing:\n%1").arg(error.what()));
                    return;
                }
                emit closed(true);
            });
}

// ---------------------------------------------------------------------------

PrizingScreen::PrizingScreen(SessionContext& context, std::string playerId, QWidget* parent)
    : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Prizing", &close));
    layout->addSpacing(16);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(false); });

    auto* content = new QWidget;
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(0, 0, 8, 0);
    sections->setSpacing(16);

    QGroupBox* redeem = makeGroup("Redeem");
    playerText_ = makeLabel(idlePlayerText(), 16, true);
    playerText_->setWordWrap(true);
    playerText_->setContentsMargins(0, 4, 0, 8);
    groupLayout(redeem)->addWidget(playerText_);
    redeemPanel_ = new QWidget;
    auto* redeemRow = new QHBoxLayout(redeemPanel_);
    redeemRow->setContentsMargins(0, 0, 0, 0);
    redeemRow->setSpacing(8);
    bountyButton_ = actionButton("Redeem bounty", "#FFF8E8");
    placeButton_ = actionButton("Redeem placement", "#E8F4FD");
    redeemRow->addWidget(bountyButton_, 1);
    redeemRow->addWidget(placeButton_, 1);
    groupLayout(redeem)->addWidget(redeemPanel_);
    sections->addWidget(redeem);
    connect(bountyButton_, &QPushButton::clicked, this, &PrizingScreen::openBountyOverlay);
    connect(placeButton_, &QPushButton::clicked, this, &PrizingScreen::openPlaceOverlay);

    auto* pools = new QHBoxLayout;
    pools->setSpacing(10);
    auto addPool = [&](const QString& title, const QString& background, const QString& border,
                       QLabel** value) -> QFrame* {
        QFrame* card = makePanel(background, border, 2, 6, 12);
        panelLayout(card)->addWidget(makeLabel(title, 12, true, QString(), Qt::AlignCenter));
        *value = makeLabel("$0.00", 22, true, QString(), Qt::AlignCenter);
        (*value)->setWordWrap(true);
        panelLayout(card)->addWidget(*value);
        pools->addWidget(card, 1);
        return card;
    };
    addPool("Prize pool", "#E8F4FD", "#2E6DA4", &prizePool_);
    bountyPoolCard_ = addPool("Bounty pool", "#FFF8E8", "#B8860B", &bountyPool_);
    sections->addLayout(pools);
    bountyHint_ = hint("These pools are prize and bounty money only. Rake and per-membership bounty "
                       "amounts are set in Pricing Structure.");
    sections->addWidget(bountyHint_);

    sections->addStretch(1);

    layout->addWidget(makeScroll(content), 1);
    layout->addSpacing(16);
    QPushButton* back = makeButton("Back to Main", QString(), 14);
    back->setStyleSheet("QPushButton { padding:10px 16px; }");
    layout->addWidget(back, 0, Qt::AlignLeft);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });

    bountyOverlay_ = new Overlay(this, QColor(0, 0, 0, 0x80));
    bountyOverlay_->card()->setMinimumWidth(400);
    QVBoxLayout* bountyCard = bountyOverlay_->cardLayout();
    bountyCard->addWidget(makeLabel("How many bounties?", 20, true, QString(), Qt::AlignCenter));
    bountyCard->addSpacing(16);
    QFrame* bountyBox = makePanel(color::LightGray, color::LightGray, 0, 5, 20);
    bountyCountLabel_ = makeLabel("0", 32, true, QString(), Qt::AlignCenter);
    panelLayout(bountyBox)->addWidget(bountyCountLabel_);
    bountyCard->addWidget(bountyBox);
    bountyPayLabel_ = makeLabel(QString(), 14, false, color::Gray, Qt::AlignCenter);
    bountyPayLabel_->setWordWrap(true);
    bountyPayLabel_->setContentsMargins(0, 12, 0, 16);
    bountyCard->addWidget(bountyPayLabel_);
    auto* bountyPad = new Numpad({"1", "2", "3", "4", "5", "6", "7", "8", "9", QStringLiteral("⌫"),
                                  "0", "C"},
                                 24);
    bountyPad->setFixedSize(240, 240);
    bountyPad->button(QStringLiteral("⌫"))->setStyleSheet(buttonStyle(color::LightCoral, "#707070", "1px"));
    bountyPad->button("C")->setStyleSheet(buttonStyle(color::Orange, "#707070", "1px"));
    connect(bountyPad, &Numpad::pressed, this, &PrizingScreen::bountyKey);
    bountyCard->addWidget(bountyPad, 0, Qt::AlignHCenter);
    bountyCard->addSpacing(16);
    auto* bountyButtons = new QHBoxLayout;
    QPushButton* cancelBounty = makeButton("Cancel", QString(), 14);
    QPushButton* payBounty = makeButton("Pay", color::LightGreen, 14, true);
    bountyButtons->addWidget(cancelBounty, 1);
    bountyButtons->addSpacing(8);
    bountyButtons->addWidget(payBounty, 1);
    bountyCard->addLayout(bountyButtons);
    connect(cancelBounty, &QPushButton::clicked, bountyOverlay_, &QWidget::hide);
    connect(payBounty, &QPushButton::clicked, this, &PrizingScreen::payBounties);

    placeOverlay_ = new Overlay(this);
    placeOverlay_->card()->setMinimumWidth(420);
    QVBoxLayout* placeCard = placeOverlay_->cardLayout();
    placeCard->addWidget(makeLabel("Redeem placement", 20, true));
    placeList_ = new QVBoxLayout;
    placeList_->setSpacing(8);
    placeCard->addSpacing(12);
    placeCard->addLayout(placeList_);
    QPushButton* cancelPlace = makeButton("Cancel", QString(), 14);
    cancelPlace->setContentsMargins(0, 12, 0, 0);
    placeCard->addWidget(cancelPlace);
    connect(cancelPlace, &QPushButton::clicked, placeOverlay_, &QWidget::hide);

    const Player* player = playerId.empty() ? nullptr : ctx_.players().find(playerId);
    if (!player || player->isUnderage()) {
        playerId_.clear();
        playerText_->setText(player && player->isUnderage()
                                 ? QStringLiteral("This player is underage and cannot be paid out.")
                                 : idlePlayerText());
        playerText_->setStyleSheet(QStringLiteral("color:%1;").arg(color::Gray));
    } else {
        playerId_ = std::move(playerId);
        const QString name = toQ(player->name());
        const QString shown = name.isEmpty() || name == QStringLiteral("New Player") ? toQ(playerId_) : name;
        const QString category = ctx_.tournament().config().usesMembershipPricing()
                                     ? QString(membershipTypeLabel(player->membership()))
                                     : player->arcStatus() == ArcStatus::Arc      ? QStringLiteral("ARC")
                                     : player->arcStatus() == ArcStatus::NonArc ? QStringLiteral("Non-ARC")
                                                                                : QStringLiteral("ARC not set");
        playerText_->setText(QStringLiteral("Paying  %1    ·    %2").arg(shown, category));
        playerText_->setStyleSheet("color:black;");
    }
    refresh();
}

void TournamentSettingsScreen::buildPrizeEditors(QVBoxLayout* roundingBody, QVBoxLayout* placesBody) {
    roundingBody->setSpacing(8);
    placesBody->setSpacing(10);
    auto* roundingRow = new QHBoxLayout;
    roundingRow->addWidget(makeLabel("Round place amounts:"));
    roundingBox_ = new QComboBox;
    roundingBox_->setObjectName(QStringLiteral("combo"));
    roundingBox_->setFont(uiFont(13));
    roundingBox_->setMinimumWidth(180);
    roundingBox_->addItem("Exact", static_cast<int>(PrizeRounding::Exact));
    roundingBox_->addItem("Nearest 5c", static_cast<int>(PrizeRounding::NearestFiveCents));
    roundingBox_->addItem("Nearest $1", static_cast<int>(PrizeRounding::NearestDollar));
    roundingBox_->addItem("Nearest $5", static_cast<int>(PrizeRounding::NearestFive));
    roundingBox_->addItem("Nearest $10", static_cast<int>(PrizeRounding::NearestTen));
    roundingBox_->addItem("Round down $1", static_cast<int>(PrizeRounding::DownDollar));
    roundingBox_->addItem("Round down $5", static_cast<int>(PrizeRounding::DownFive));
    const int roundingIndex =
        roundingBox_->findData(static_cast<int>(ctx_.tournament().config().prizeRounding));
    roundingBox_->setCurrentIndex(roundingIndex >= 0 ? roundingIndex : 0);
    roundingRow->addWidget(roundingBox_);
    roundingRow->addStretch(1);
    roundingBody->addLayout(roundingRow);
    leftoverBox_ = new QCheckBox("Give leftover from rounding to 1st place");
    leftoverBox_->setFont(uiFont(13));
    leftoverBox_->setChecked(ctx_.tournament().config().leftoverToFirst);
    roundingBody->addSpacing(8);
    roundingBody->addWidget(leftoverBox_);
    roundingBody->addWidget(
        hint("Percents apply to the prize pool only. Rounding is used on the room display and when "
             "paying a place. Unallocated percent stays in the prize pool."));
        auto* presetRow = new QHBoxLayout;
    presetRow->addWidget(makeLabel("Preset:"));
    presetBox_ = new QComboBox;
    presetBox_->setObjectName(QStringLiteral("combo"));
    presetBox_->setFont(uiFont(13));
    presetBox_->setMinimumWidth(220);
    presetBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    presetRow->addWidget(presetBox_, 1);
    placesBody->addLayout(presetRow);

    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(makeLabel("Name:"));
    presetName_ = new QLineEdit;
    presetName_->setObjectName(QStringLiteral("textField"));
    presetName_->setFont(uiFont(13));
    nameRow->addWidget(presetName_, 1);
    placesBody->addLayout(nameRow);

    auto* fileRow = new QHBoxLayout;
    fileRow->setSpacing(8);
    QPushButton* loadJson = footerButton("Load JSON", 120, false);
    QPushButton* saveJson = footerButton("Save JSON", 120, false);
    QPushButton* newField = footerButton("New by field size", 170, false);
    QPushButton* newFixed = footerButton("New fixed", 120, false);
    deletePreset_ = footerButton("Delete", 100, false);
    fileRow->addWidget(loadJson);
    fileRow->addWidget(saveJson);
    fileRow->addWidget(newField);
    fileRow->addWidget(newFixed);
    fileRow->addWidget(deletePreset_);
    fileRow->addStretch(1);
    placesBody->addSpacing(8);
    placesBody->addLayout(fileRow);

    variablePanel_ = new QWidget;
    auto* variableLayout = new QVBoxLayout(variablePanel_);
    variableLayout->setContentsMargins(0, 8, 0, 0);
    variableLayout->setSpacing(8);
    auto* bandRow = new QHBoxLayout;
    bandRow->addWidget(makeLabel("Field size:"));
    bandBox_ = new QComboBox;
    bandBox_->setObjectName(QStringLiteral("combo"));
    bandBox_->setFont(uiFont(13));
    bandBox_->setMinimumWidth(180);
    bandRow->addWidget(bandBox_, 1);
    QPushButton* addBandButton = footerButton("Add band", 110, false);
    QPushButton* removeBandButton = footerButton("Remove band", 130, false);
    bandRow->addWidget(addBandButton);
    bandRow->addWidget(removeBandButton);
    variableLayout->addLayout(bandRow);
    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(makeLabel("Players:"));
    minPlayers_ = new QSpinBox;
    maxPlayers_ = new QSpinBox;
    for (QSpinBox* spin : {minPlayers_, maxPlayers_}) {
        spin->setRange(0, 10000);
        spin->setFont(uiFont(13));
        spin->setMinimumWidth(80);
    }
    rangeRow->addWidget(minPlayers_);
    rangeRow->addWidget(makeLabel("to"));
    rangeRow->addWidget(maxPlayers_);
    rangeRow->addSpacing(12);
    countBuyins_ = new QCheckBox("Count buy-ins");
    countRebuys_ = new QCheckBox("Count rebuys");
    countBuyins_->setFont(uiFont(13));
    countRebuys_->setFont(uiFont(13));
    rangeRow->addWidget(countBuyins_);
    rangeRow->addWidget(countRebuys_);
    rangeRow->addStretch(1);
    variableLayout->addLayout(rangeRow);
    fieldHint_ = hint(QString());
    variableLayout->addWidget(fieldHint_);
    placesBody->addWidget(variablePanel_);

    prizesTable_ = makeEditorTable({"Place", "Percent of prize pool"});
    prizesTable_->setMaximumHeight(360);
    placesBody->addWidget(prizesTable_);
    auto* prizeButtons = new QHBoxLayout;
    QPushButton* addPrize = footerButton("Add Place", 120, false);
    QPushButton* removePrize = footerButton("Remove Last", 120, false);
    prizeButtons->addWidget(addPrize);
    prizeButtons->addWidget(removePrize);
    prizeButtons->addStretch(1);
    placesBody->addSpacing(8);
    placesBody->addLayout(prizeButtons);
    placesBody->addWidget(
        hint("Percents apply to the prize pool only (not bounty). A field-size preset picks the band "
             "for the current entries. Each place can be paid once."));
    payoutPreview_ = makeLabel(QString(), 13, false, color::Gray);
    payoutPreview_->setWordWrap(true);
    payoutPreview_->setContentsMargins(0, 8, 0, 0);
    placesBody->addWidget(payoutPreview_);
    reloadPresetList();
    showPreset(ctx_.tournament().config().activePrizePreset);
    refreshPayoutPreview();
    connect(addPrize, &QPushButton::clicked, this, [this] {
        addPrizeRow(prizesTable_, PrizeShare{prizesTable_->rowCount() + 1, 0});
        prizesTable_->scrollToBottom();
        prizingChanged();
    });
    connect(removePrize, &QPushButton::clicked, this, [this] {
        if (prizesTable_->rowCount() > 0) prizesTable_->removeRow(prizesTable_->rowCount() - 1);
        prizingChanged();
    });
    connect(prizesTable_, &QTableWidget::itemChanged, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(roundingBox_, &QComboBox::currentIndexChanged, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(leftoverBox_, &QCheckBox::toggled, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(presetBox_, &QComboBox::currentIndexChanged, this, &TournamentSettingsScreen::onPresetChanged);
    connect(presetName_, &QLineEdit::editingFinished, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(bandBox_, &QComboBox::currentIndexChanged, this, &TournamentSettingsScreen::onBandChanged);
    connect(minPlayers_, &QSpinBox::valueChanged, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(maxPlayers_, &QSpinBox::valueChanged, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(countBuyins_, &QCheckBox::toggled, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(countRebuys_, &QCheckBox::toggled, this, [this] {
        if (!loadingPrizes_) prizingChanged();
    });
    connect(loadJson, &QPushButton::clicked, this, &TournamentSettingsScreen::loadPresetJson);
    connect(saveJson, &QPushButton::clicked, this, &TournamentSettingsScreen::savePresetJson);
    connect(newField, &QPushButton::clicked, this, [this] { newPreset(true); });
    connect(newFixed, &QPushButton::clicked, this, [this] { newPreset(false); });
    connect(deletePreset_, &QPushButton::clicked, this, &TournamentSettingsScreen::deletePreset);
    connect(addBandButton, &QPushButton::clicked, this, &TournamentSettingsScreen::addBand);
    connect(removeBandButton, &QPushButton::clicked, this, &TournamentSettingsScreen::removeBand);
}

QString TournamentSettingsScreen::bandLabel(const PrizeBand& band) const {
    if (band.maxPlayers <= 0) return QStringLiteral("%1+ players").arg(band.minPlayers);
    return QStringLiteral("%1–%2 players").arg(band.minPlayers).arg(band.maxPlayers);
}

void TournamentSettingsScreen::reloadPresetList() {
    const TournamentConfig& config = ctx_.tournament().config();
    const bool blocked = presetBox_->blockSignals(true);
    presetBox_->clear();
    for (const PrizePreset& preset : config.prizePresets) presetBox_->addItem(toQ(preset.name));
    int index = config.activePrizePreset;
    if (index < 0 || index >= presetBox_->count()) index = 0;
    presetBox_->setCurrentIndex(index);
    presetBox_->blockSignals(blocked);
    if (deletePreset_) deletePreset_->setEnabled(config.prizePresets.size() > 1);
}

void TournamentSettingsScreen::showBand(int index) {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
    if (!preset.variableByPlayers || preset.levels.empty()) return;
    if (index < 0 || index >= static_cast<int>(preset.levels.size())) index = 0;
    editingBand_ = index;
    const PrizeBand& band = preset.levels[static_cast<std::size_t>(index)];
    loadingPrizes_ = true;
    minPlayers_->setValue(band.minPlayers);
    maxPlayers_->setValue(band.maxPlayers);
    prizesTable_->blockSignals(true);
    prizesTable_->setRowCount(0);
    for (const PrizeShare& prize : band.prizes) addPrizeRow(prizesTable_, prize);
    if (prizesTable_->rowCount() == 0) addPrizeRow(prizesTable_, PrizeShare{1, 100, true});
    prizesTable_->blockSignals(false);
    if (bandBox_->currentIndex() != index) {
        const bool blocked = bandBox_->blockSignals(true);
        bandBox_->setCurrentIndex(index);
        bandBox_->blockSignals(blocked);
    }
    loadingPrizes_ = false;
}

void TournamentSettingsScreen::showPreset(int index) {
    TournamentConfig& config = ctx_.tournament().config();
    if (config.prizePresets.empty()) config.prizePresets = TournamentConfig::defaultPrizePresets();
    if (index < 0 || index >= static_cast<int>(config.prizePresets.size())) index = 0;
    editingPreset_ = index;
    config.activePrizePreset = index;
    const PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(index)];
    loadingPrizes_ = true;
    if (presetBox_->currentIndex() != index) {
        const bool blocked = presetBox_->blockSignals(true);
        presetBox_->setCurrentIndex(index);
        presetBox_->blockSignals(blocked);
    }
    presetName_->setText(toQ(preset.name));
    variablePanel_->setVisible(preset.variableByPlayers);
    countBuyins_->setChecked(preset.countBuyins);
    countRebuys_->setChecked(preset.countRebuys);
    const std::vector<PrizeShare>* shares = &preset.prizes;
    if (preset.variableByPlayers) {
        const bool blocked = bandBox_->blockSignals(true);
        bandBox_->clear();
        for (const PrizeBand& band : preset.levels) bandBox_->addItem(bandLabel(band));
        bandBox_->blockSignals(blocked);
        const auto stats = ctx_.reports().tournamentStats();
        int band = preset.bandIndexFor(preset.fieldSize(stats.buyIns, stats.rebuys));
        if (band < 0) band = 0;
        if (!preset.levels.empty()) {
            showBand(band);
            shares = nullptr;
        }
    }
    if (shares) {
        prizesTable_->blockSignals(true);
        prizesTable_->setRowCount(0);
        for (const PrizeShare& prize : *shares) addPrizeRow(prizesTable_, prize);
        if (prizesTable_->rowCount() == 0) addPrizeRow(prizesTable_, PrizeShare{1, 100, true});
        prizesTable_->blockSignals(false);
    }
    const std::vector<PrizeShare>& shown = preset.variableByPlayers && !preset.levels.empty()
                                               ? preset.levels[static_cast<std::size_t>(editingBand_)].prizes
                                               : preset.prizes;
    loadingPrizes_ = true;
    for (const PrizeShare& prize : shown) {
        if (prize.shareLeftover) {
            leftoverBox_->setChecked(true);
            break;
        }
    }
    loadingPrizes_ = false;
    if (deletePreset_) deletePreset_->setEnabled(config.prizePresets.size() > 1);
}

bool TournamentSettingsScreen::writeTableToPreset(PrizePreset& preset, QString& problem) {
    std::vector<PrizeShare> prizes;
    if (!readPrizes(prizesTable_, prizes, problem)) return false;
    const bool leftover = leftoverBox_->isChecked();
    for (PrizeShare& prize : prizes) prize.shareLeftover = leftover && prize.place == 1;
    const QString typed = presetName_->text().trimmed();
    if (!typed.isEmpty()) preset.name = toStd(typed);
    if (preset.name.empty()) preset.name = "Prize preset";
    if (preset.variableByPlayers) {
        preset.countBuyins = countBuyins_->isChecked();
        preset.countRebuys = countRebuys_->isChecked();
        if (preset.levels.empty()) preset.levels.push_back(PrizeBand{});
        int index = editingBand_;
        if (index < 0 || index >= static_cast<int>(preset.levels.size())) index = 0;
        editingBand_ = index;
        PrizeBand& band = preset.levels[static_cast<std::size_t>(index)];
        band.minPlayers = minPlayers_->value();
        band.maxPlayers = maxPlayers_->value();
        if (band.maxPlayers > 0 && band.maxPlayers < band.minPlayers) band.maxPlayers = band.minPlayers;
        band.prizes = std::move(prizes);
    } else {
        preset.prizes = std::move(prizes);
    }
    return true;
}

void TournamentSettingsScreen::onPresetChanged(int index) {
    if (loadingPrizes_ || index < 0) return;
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ >= 0 && editingPreset_ < static_cast<int>(config.prizePresets.size()) &&
        editingPreset_ != index) {
        QString problem;
        if (!writeTableToPreset(config.prizePresets[static_cast<std::size_t>(editingPreset_)], problem)) {
            loadingPrizes_ = true;
            presetBox_->setCurrentIndex(editingPreset_);
            loadingPrizes_ = false;
            QMessageBox::warning(this, "Tournament settings", problem);
            return;
        }
    }
    showPreset(index);
    try {
        ctx_.saveTournamentConfig();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Could not save prizing:\n%1").arg(error.what()));
    }
    refreshPayoutPreview();
}

void TournamentSettingsScreen::onBandChanged(int index) {
    if (loadingPrizes_ || index < 0) return;
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
    if (editingBand_ >= 0 && editingBand_ < static_cast<int>(preset.levels.size()) && editingBand_ != index) {
        QString problem;
        if (!writeTableToPreset(preset, problem)) {
            loadingPrizes_ = true;
            bandBox_->setCurrentIndex(editingBand_);
            loadingPrizes_ = false;
            QMessageBox::warning(this, "Tournament settings", problem);
            return;
        }
    }
    showBand(index);
    refreshPayoutPreview();
    try {
        ctx_.saveTournamentConfig();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Could not save prizing:\n%1").arg(error.what()));
    }
}

void TournamentSettingsScreen::addBand() {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
    if (!preset.variableByPlayers) return;
    QString problem;
    if (!writeTableToPreset(preset, problem)) {
        QMessageBox::warning(this, "Tournament settings", problem);
        return;
    }
    PrizeBand band;
    if (!preset.levels.empty()) {
        const PrizeBand& last = preset.levels.back();
        band.minPlayers = last.maxPlayers > 0 ? last.maxPlayers + 1 : last.minPlayers + 1;
        band.maxPlayers = band.minPlayers + 7;
        band.prizes = last.prizes;
    } else {
        band.minPlayers = 1;
        band.maxPlayers = 8;
        band.prizes = {{1, 100, true}};
    }
    preset.levels.push_back(std::move(band));
    const bool blocked = bandBox_->blockSignals(true);
    bandBox_->addItem(bandLabel(preset.levels.back()));
    bandBox_->blockSignals(blocked);
    showBand(static_cast<int>(preset.levels.size()) - 1);
    persistPrizing();
    refreshPayoutPreview();
}

void TournamentSettingsScreen::removeBand() {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
    if (!preset.variableByPlayers || preset.levels.size() <= 1) return;
    if (editingBand_ < 0 || editingBand_ >= static_cast<int>(preset.levels.size())) return;
    preset.levels.erase(preset.levels.begin() + editingBand_);
    int next = editingBand_;
    if (next >= static_cast<int>(preset.levels.size())) next = static_cast<int>(preset.levels.size()) - 1;
    const bool blocked = bandBox_->blockSignals(true);
    bandBox_->clear();
    for (const PrizeBand& band : preset.levels) bandBox_->addItem(bandLabel(band));
    bandBox_->blockSignals(blocked);
    showBand(next);
    persistPrizing();
    refreshPayoutPreview();
}

void TournamentSettingsScreen::newPreset(bool byFieldSize) {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ >= 0 && editingPreset_ < static_cast<int>(config.prizePresets.size())) {
        QString problem;
        if (!writeTableToPreset(config.prizePresets[static_cast<std::size_t>(editingPreset_)], problem)) {
            QMessageBox::warning(this, "Tournament settings", problem);
            return;
        }
    }
    PrizePreset created;
    if (byFieldSize) {
        created.name = "By field size";
        created.variableByPlayers = true;
        created.countBuyins = true;
        created.countRebuys = false;
        PrizeBand band;
        band.minPlayers = 1;
        band.maxPlayers = 8;
        band.prizes = {{1, 100, true}};
        created.levels.push_back(std::move(band));
    } else {
        created.name = "Fixed places";
        created.prizes = TournamentConfig::defaultPrizes();
    }
    int suffix = 2;
    const std::string base = created.name;
    auto taken = [&](const std::string& name) {
        for (const PrizePreset& preset : config.prizePresets) {
            if (preset.name == name) return true;
        }
        return false;
    };
    while (taken(created.name)) created.name = base + " " + std::to_string(suffix++);
    config.prizePresets.push_back(std::move(created));
    config.activePrizePreset = static_cast<int>(config.prizePresets.size()) - 1;
    reloadPresetList();
    showPreset(config.activePrizePreset);
    persistPrizing();
    refreshPayoutPreview();
}

void TournamentSettingsScreen::deletePreset() {
    TournamentConfig& config = ctx_.tournament().config();
    if (config.prizePresets.size() <= 1) return;
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    const QString name = toQ(config.prizePresets[static_cast<std::size_t>(editingPreset_)].name);
    if (QMessageBox::question(this, "Tournament settings", QStringLiteral("Delete preset \"%1\"?").arg(name)) !=
        QMessageBox::Yes) {
        return;
    }
    config.prizePresets.erase(config.prizePresets.begin() + editingPreset_);
    if (config.activePrizePreset >= static_cast<int>(config.prizePresets.size())) {
        config.activePrizePreset = static_cast<int>(config.prizePresets.size()) - 1;
    }
    reloadPresetList();
    showPreset(config.activePrizePreset);
    try {
        ctx_.saveTournamentConfig();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Could not save prizing:\n%1").arg(error.what()));
    }
    refreshPayoutPreview();
}

void TournamentSettingsScreen::loadPresetJson() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Load prize preset"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("Prize JSON (*.json);;All files (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Load prizes"),
                              QStringLiteral("Couldn't open:\n%1").arg(path));
        return;
    }
    std::vector<PrizePreset> loaded = PrizePreset::listFromJson(toStd(QString::fromUtf8(file.readAll())));
    if (loaded.empty()) {
        QMessageBox::critical(
            this, QStringLiteral("Load prizes"),
            QStringLiteral("No prize schedule found. Expected a prizeConfig object, or a presets list."));
        return;
    }
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ >= 0 && editingPreset_ < static_cast<int>(config.prizePresets.size())) {
        QString problem;
        writeTableToPreset(config.prizePresets[static_cast<std::size_t>(editingPreset_)], problem);
    }
    const QString base = QFileInfo(path).completeBaseName();
    bool leftover = false;
    for (PrizePreset& preset : loaded) {
        if (preset.name.empty() || preset.name == "Prize preset") preset.name = toStd(base);
        const auto note = [&](const std::vector<PrizeShare>& prizes) {
            for (const PrizeShare& prize : prizes) leftover = leftover || prize.shareLeftover;
        };
        note(preset.prizes);
        for (const PrizeBand& band : preset.levels) note(band.prizes);
        config.prizePresets.push_back(std::move(preset));
    }
    config.activePrizePreset = static_cast<int>(config.prizePresets.size()) - 1;
    if (leftover) leftoverBox_->setChecked(true);
    reloadPresetList();
    showPreset(config.activePrizePreset);
    persistPrizing();
    refreshPayoutPreview();
}

void TournamentSettingsScreen::savePresetJson() {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    QString problem;
    if (!writeTableToPreset(config.prizePresets[static_cast<std::size_t>(editingPreset_)], problem)) {
        QMessageBox::warning(this, QStringLiteral("Save prizes"), problem);
        return;
    }
    const PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
    QString suggested = toQ(preset.name);
    suggested.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    if (suggested.trimmed().isEmpty()) suggested = QStringLiteral("prizes");
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save prize preset"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/") + suggested + QStringLiteral(".json"),
        QStringLiteral("Prize JSON (*.json);;All files (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Save prizes"),
                              QStringLiteral("Couldn't write:\n%1").arg(path));
        return;
    }
    const std::string json = preset.toJson();
    if (file.write(json.data(), static_cast<qint64>(json.size())) < 0) {
        QMessageBox::critical(this, QStringLiteral("Save prizes"),
                              QStringLiteral("Couldn't write:\n%1").arg(path));
        return;
    }
    persistPrizing();
}

QString PrizingScreen::idlePlayerText() const {
    return QStringLiteral("Scan a player on the main screen, then open Prizing.");
}

void PrizingScreen::refresh() {
    const auto stats = ctx_.reports().tournamentStats();
    const bool bountiesOn = ctx_.tournament().config().hasBounties();
    prizePool_->setText(QStringLiteral("%1\npaid %2")
                            .arg(money(stats.prizeRemaining()), money(stats.prizePaid)));
    bountyPool_->setText(QStringLiteral("%1\npaid %2")
                             .arg(money(stats.bountyRemaining()), money(stats.bountyPaid)));
    bountyPoolCard_->setVisible(bountiesOn);
    bountyButton_->setVisible(bountiesOn);
    bountyHint_->setText(bountiesOn
                             ? QStringLiteral("These pools are prize and bounty money only. Rake and "
                                              "per-membership bounty amounts are set in Pricing Structure.")
                             : QStringLiteral("This pool is prize money only. Rake is set in Pricing Structure."));
    const bool hasPlayer = !playerId_.empty();
    bountyButton_->setEnabled(hasPlayer && bountiesOn && stats.bountyRemaining() > Money{});
    placeButton_->setEnabled(hasPlayer);
}

void TournamentSettingsScreen::refreshPayoutPreview() {
    TournamentConfig sample = ctx_.tournament().config();
    if (sample.prizePresets.empty()) {
        payoutPreview_->setText(QStringLiteral("No places configured."));
        return;
    }
    int index = editingPreset_;
    if (index < 0 || index >= static_cast<int>(sample.prizePresets.size())) index = 0;
    QString problem;
    if (!writeTableToPreset(sample.prizePresets[static_cast<std::size_t>(index)], problem)) {
        payoutPreview_->setText(problem);
        if (fieldHint_) fieldHint_->setText(problem);
        return;
    }
    sample.activePrizePreset = index;
    sample.prizeRounding = static_cast<PrizeRounding>(roundingBox_->currentData().toInt());
    sample.leftoverToFirst = leftoverBox_->isChecked();
    const PrizePreset& preset = sample.prizePresets[static_cast<std::size_t>(index)];
    const auto stats = ctx_.reports().tournamentStats();
    const int field = preset.fieldSize(stats.buyIns, stats.rebuys);
    if (fieldHint_) {
        if (!preset.variableByPlayers) {
            fieldHint_->clear();
        } else {
            const int band = preset.bandIndexFor(field);
            QString counted = preset.countBuyins ? QStringLiteral("buy-ins") : QString();
            if (preset.countRebuys) {
                counted += counted.isEmpty() ? QStringLiteral("rebuys") : QStringLiteral(" + rebuys");
            }
            if (counted.isEmpty()) counted = QStringLiteral("buy-ins");
            if (band >= 0 && band < static_cast<int>(preset.levels.size())) {
                const PrizeBand& level = preset.levels[static_cast<std::size_t>(band)];
                fieldHint_->setText(QStringLiteral("%1 entries (%2). Paying the %3 band.")
                                        .arg(field)
                                        .arg(counted, bandLabel(level)));
            } else {
                fieldHint_->setText(QStringLiteral("%1 entries (%2).").arg(field).arg(counted));
            }
        }
    }
    QStringList lines;
    for (const PrizePayout& prize : sample.prizePayouts(stats.prizePool, stats.buyIns, stats.rebuys)) {
        const QString paid = stats.placePaid(prize.place) ? QStringLiteral("  (paid)") : QString();
        lines += QStringLiteral("%1  %2%  %3%4")
                     .arg(placeOrdinal(prize.place), formatPercent(prize.percent), money(prize.amount), paid);
    }
    payoutPreview_->setText(lines.isEmpty() ? QStringLiteral("No places configured.")
                                            : lines.join('\n'));
}

Money PrizingScreen::bountyUnitForPlayer() const {
    const TournamentConfig& config = ctx_.tournament().config();
    if (const Player* player = ctx_.players().find(playerId_)) {
        const Money unit = config.bountyUnit(*player);
        if (!unit.isZero()) return unit;
    }
    const Money typical = Money::fromCents(config.typicalBountyCents());
    if (!typical.isZero()) return typical;
    return ctx_.reports().tournamentStats().bountyRemaining();
}

void TournamentSettingsScreen::prizingChanged() {
    refreshPayoutPreview();
    persistPrizing();
}

void TournamentSettingsScreen::persistPrizing() {
    TournamentConfig& config = ctx_.tournament().config();
    if (editingPreset_ < 0 || editingPreset_ >= static_cast<int>(config.prizePresets.size())) return;
    QString problem;
    if (!writeTableToPreset(config.prizePresets[static_cast<std::size_t>(editingPreset_)], problem)) return;
    config.activePrizePreset = editingPreset_;
    config.prizeRounding = static_cast<PrizeRounding>(roundingBox_->currentData().toInt());
    config.leftoverToFirst = leftoverBox_->isChecked();
    if (presetBox_ && presetBox_->currentIndex() == editingPreset_) {
        const QString name = toQ(config.prizePresets[static_cast<std::size_t>(editingPreset_)].name);
        if (presetBox_->itemText(editingPreset_) != name) {
            const bool blocked = presetBox_->blockSignals(true);
            presetBox_->setItemText(editingPreset_, name);
            presetBox_->blockSignals(blocked);
        }
    }
    if (bandBox_ && variablePanel_->isVisible() &&
        editingBand_ >= 0 && editingBand_ < bandBox_->count()) {
        const PrizePreset& preset = config.prizePresets[static_cast<std::size_t>(editingPreset_)];
        if (editingBand_ < static_cast<int>(preset.levels.size())) {
            const QString label = bandLabel(preset.levels[static_cast<std::size_t>(editingBand_)]);
            if (bandBox_->itemText(editingBand_) != label) {
                const bool blocked = bandBox_->blockSignals(true);
                bandBox_->setItemText(editingBand_, label);
                bandBox_->blockSignals(blocked);
            }
        }
    }
    try {
        ctx_.saveTournamentConfig();
        emit structureChanged();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Could not save prizing:\n%1").arg(error.what()));
    }
}

void PrizingScreen::openBountyOverlay() {
    if (!ctx_.tournament().config().hasBounties()) return;
    if (playerId_.empty()) {
        QMessageBox::information(this, "Prizing", "Scan a player on the main screen first.");
        return;
    }
    const Money per = bountyUnitForPlayer();
    const auto stats = ctx_.reports().tournamentStats();
    if (per.isZero() || stats.bountyRemaining() <= Money{}) {
        QMessageBox::information(this, "Prizing", "No bounty pool remaining.");
        return;
    }
    bountyMaxCount_ = static_cast<int>(stats.bountyRemaining().cents() / per.cents());
    if (bountyMaxCount_ < 1) {
        QMessageBox::information(this, "Prizing", "No bounty pool remaining.");
        return;
    }
    bountyDigits_.clear();
    bountyCount_ = 0;
    refreshBountyPad();
    bountyOverlay_->open();
}

void PrizingScreen::bountyKey(const QString& key) {
    if (key == QStringLiteral("⌫")) {
        if (!bountyDigits_.isEmpty()) bountyDigits_.chop(1);
    } else if (key == "C") {
        bountyDigits_.clear();
    } else if (key.size() == 1 && key[0].isDigit()) {
        if (bountyDigits_.size() >= 3) return;
        bountyDigits_ += key;
        if (bountyDigits_.startsWith('0')) bountyDigits_ = QString::number(bountyDigits_.toInt());
        bool ok = false;
        const int typed = bountyDigits_.toInt(&ok);
        if (ok && typed > bountyMaxCount_) bountyDigits_ = QString::number(bountyMaxCount_);
    }
    refreshBountyPad();
}

void PrizingScreen::refreshBountyPad() {
    bool ok = false;
    bountyCount_ = bountyDigits_.toInt(&ok);
    if (!ok || bountyCount_ < 0) bountyCount_ = 0;
    bountyCountLabel_->setText(bountyDigits_.isEmpty() ? QStringLiteral("0") : bountyDigits_);
    const Money per = bountyUnitForPlayer();
    const QString pay = bountyCount_ > 0 ? money(per * bountyCount_) : money(Money{});
    bountyPayLabel_->setText(QStringLiteral("%1 each  ·  up to %2  ·  pay %3")
                                 .arg(money(per))
                                 .arg(bountyMaxCount_)
                                 .arg(pay));
}

void PrizingScreen::openPlaceOverlay() {
    if (playerId_.empty()) {
        QMessageBox::information(this, "Prizing", "Scan a player on the main screen first.");
        return;
    }
    clearLayout(placeList_);
    const auto stats = ctx_.reports().tournamentStats();
    const TournamentConfig& config = ctx_.tournament().config();
    bool any = false;
    for (const PrizePayout& prize :
         config.prizePayouts(stats.prizePool, stats.buyIns, stats.rebuys)) {
        if (stats.placePaid(prize.place)) continue;
        any = true;
        auto* button = actionButton(QStringLiteral("%1   %2%   %3")
                                        .arg(placeOrdinal(prize.place), formatPercent(prize.percent),
                                             money(prize.amount)),
                                    "#E8F4FD");
        const int place = prize.place;
        const Money amount = prize.amount;
        connect(button, &QPushButton::clicked, this, [this, place, amount] { payPlace(place, amount); });
        placeList_->addWidget(button);
    }
    if (!any) {
        placeList_->addWidget(makeLabel("Every configured place has already been paid.", 14, false,
                                        color::Gray, Qt::AlignCenter));
    }
    placeOverlay_->open();
}

void PrizingScreen::payBounties() {
    if (bountyCount_ < 1) {
        QMessageBox::information(this, "Prizing", "Enter how many bounties to pay.");
        return;
    }
    const Money per = bountyUnitForPlayer();
    const auto stats = ctx_.reports().tournamentStats();
    if (per.isZero() || per * bountyCount_ > stats.bountyRemaining()) {
        QMessageBox::warning(this, "Prizing", "That would exceed the remaining bounty pool.");
        return;
    }
    bountyOverlay_->hide();
    const auto result = ctx_.prizes().payBounty(playerId_, bountyCount_, per);
    if (!result.success) {
        QMessageBox::critical(this, "Prizing", toQ(result.message));
        return;
    }
    QMessageBox::information(this, "Prizing", toQ(result.message));
    refresh();
}

void PrizingScreen::payPlace(int place, Money amount) {
    placeOverlay_->hide();
    const auto stats = ctx_.reports().tournamentStats();
    if (stats.placePaid(place)) {
        QMessageBox::warning(this, "Prizing", "That place has already been paid.");
        return;
    }
    if (amount > stats.prizeRemaining()) {
        QMessageBox::warning(this, "Prizing", "That would exceed the remaining prize pool.");
        return;
    }
    if (QMessageBox::question(this, "Pay placement",
                              QStringLiteral("Pay %1  %2  to %3?")
                                  .arg(placeOrdinal(place), money(amount), toQ(playerId_))) !=
        QMessageBox::Yes) {
        return;
    }
    const auto result = ctx_.prizes().payPlacement(playerId_, place, amount);
    if (!result.success) {
        QMessageBox::critical(this, "Prizing", toQ(result.message));
        return;
    }
    QMessageBox::information(this, "Prizing", toQ(result.message));
    refresh();
}

// ---------------------------------------------------------------------------

ComingSoonScreen::ComingSoonScreen(SessionContext& context, const QString& title,
                                   const QString& message, QWidget* parent)
    : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader(title, &close));
    connect(close, &QPushButton::clicked, this, [this] { emit closed(false); });

    layout->addStretch(1);
    QLabel* text = makeLabel(message, 20, false, QString(), Qt::AlignCenter);
    text->setWordWrap(true);
    layout->addWidget(text);
    layout->addStretch(1);

    QPushButton* back = makeButton("Back to Main", QString(), 16);
    back->setStyleSheet("QPushButton { padding:12px 20px; }");
    layout->addWidget(back, 0, Qt::AlignHCenter);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });
}

// ---------------------------------------------------------------------------

TournamentBoardControlScreen::TournamentBoardControlScreen(SessionContext& context,
                                                           TournamentClock& clock, QWidget* parent)
    : Screen(context, parent), clock_(clock) {
    namespace sc = settings;
    const TournamentConfig& initial = ctx_.tournament().config();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("tsPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(sc::pageStyleSheet());
    outer->addWidget(page);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto* header = new QWidget;
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(16);
    auto* headings = new QVBoxLayout;
    headings->setSpacing(4);
    headings->addWidget(sc::text("Board controls", "pageTitle"));
    headings->addWidget(sc::text("Run the clock, open the TV board, and choose what it shows.",
                                 "pageSubtitle", true));
    headerRow->addLayout(headings, 1);
    QToolButton* close = sc::iconButton(sc::Glyph::Close, "Close");
    headerRow->addWidget(close, 0, Qt::AlignTop);
    auto* headerBar = new QFrame;
    headerBar->setObjectName(QStringLiteral("headerBar"));
    pageLayout->addWidget(sc::constrain(header, {32, 28, 32, 20}, headerBar));
    connect(close, &QToolButton::clicked, this, [this] { emit closed(true); });

    auto* column = new QWidget;
    auto* cards = new QVBoxLayout(column);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setSpacing(16);

    // ── Status ──────────────────────────────────────────────────────────────
    sc::Card statusCard = sc::card("Clock status", "Live view of the current level and prize pools.");
    auto* statusRow = new QHBoxLayout;
    level_ = sc::text(QString(), "clockMeta");
    status_ = sc::text(QString(), "clockStatus");
    status_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusRow->addWidget(level_, 1);
    statusRow->addWidget(status_);
    statusCard.body->addLayout(statusRow);
    statusCard.body->addSpacing(8);
    time_ = sc::text("00:00", "clockTime");
    time_->setAlignment(Qt::AlignCenter);
    statusCard.body->addWidget(time_);
    statusCard.body->addSpacing(8);
    blinds_ = sc::text(QString(), "clockBlinds");
    blinds_->setAlignment(Qt::AlignCenter);
    nextBlinds_ = sc::text(QString(), "clockNext");
    nextBlinds_->setAlignment(Qt::AlignCenter);
    pot_ = sc::text(QString(), "clockPot");
    pot_->setAlignment(Qt::AlignCenter);
    pot_->setWordWrap(true);
    statusCard.body->addWidget(blinds_);
    statusCard.body->addSpacing(4);
    statusCard.body->addWidget(nextBlinds_);
    statusCard.body->addSpacing(10);
    statusCard.body->addWidget(pot_);
    cards->addWidget(statusCard.frame);

    // ── Clock ───────────────────────────────────────────────────────────────
    sc::Card clockCard =
        sc::card("Clock", "Pause for a floor decision. Skip jumps to the next blinds and restarts that level.");
    start_ = sc::button("Start / Resume", "primary");
    pause_ = sc::button("Pause", "secondary");
    auto* runRow = new QHBoxLayout;
    runRow->setSpacing(8);
    runRow->addWidget(start_, 1);
    runRow->addWidget(pause_, 1);
    clockCard.body->addLayout(runRow);
    clockCard.body->addSpacing(10);
    prevLevel_ = sc::button("Previous level", "secondary");
    skipLevel_ = sc::button("Skip level", "secondary");
    auto* levelRow = new QHBoxLayout;
    levelRow->setSpacing(8);
    levelRow->addWidget(prevLevel_, 1);
    levelRow->addWidget(skipLevel_, 1);
    clockCard.body->addLayout(levelRow);
    clockCard.body->addSpacing(10);
    auto* timeRow = new QHBoxLayout;
    timeRow->setSpacing(8);
    QPushButton* minusMin = sc::button("−1 min", "ghost");
    QPushButton* minusSec = sc::button("−30 s", "ghost");
    QPushButton* reset = sc::button("Reset level", "secondary");
    QPushButton* plusSec = sc::button("+30 s", "ghost");
    QPushButton* plusMin = sc::button("+1 min", "ghost");
    for (QPushButton* button : {minusMin, minusSec, reset, plusSec, plusMin}) {
        timeRow->addWidget(button, 1);
    }
    clockCard.body->addLayout(timeRow);
    cards->addWidget(clockCard.frame);

    // ── Jump to level ───────────────────────────────────────────────────────
    sc::Card jumpCard =
        sc::card("Jump to level", "Jumps restart the clock at the start of that level.");
    levelsHost_ = new QWidget;
    levelsGrid_ = new QGridLayout(levelsHost_);
    levelsGrid_->setContentsMargins(0, 0, 0, 0);
    levelsGrid_->setSpacing(8);
    jumpCard.body->addWidget(levelsHost_);
    cards->addWidget(jumpCard.frame);

    // ── Room display ────────────────────────────────────────────────────────
    sc::Card displayCard =
        sc::card("Room display", "Open the board on the TV, then keep this window for buy-ins and clock control.");
    QPushButton* openBoard = sc::button("Open / show board", "secondary");
    QPushButton* sendTv = sc::button("Send to other screen", "secondary");
    QPushButton* fullscreen = sc::button("Toggle fullscreen", "secondary");
    QPushButton* closeBoard = sc::button("Close board", "danger");
    auto* displayRow1 = new QHBoxLayout;
    displayRow1->setSpacing(8);
    displayRow1->addWidget(openBoard, 1);
    displayRow1->addWidget(sendTv, 1);
    auto* displayRow2 = new QHBoxLayout;
    displayRow2->setSpacing(8);
    displayRow2->addWidget(fullscreen, 1);
    displayRow2->addWidget(closeBoard, 1);
    displayCard.body->addLayout(displayRow1);
    displayCard.body->addSpacing(10);
    displayCard.body->addLayout(displayRow2);
    cards->addWidget(displayCard.frame);

    // ── TV board side items ─────────────────────────────────────────────────
    sc::Card boardCard =
        sc::card("TV board", "Each side-column item can be shown or hidden on the display board.");
    const auto addToggle = [&](const QString& title, const QString& description, bool on) {
        auto* sw = new sc::Switch;
        sw->setChecked(on);
        boardCard.body->addWidget(sc::settingRow(title, description, sw));
        return sw;
    };
    const auto addBoardDivider = [&] {
        boardCard.body->addSpacing(14);
        boardCard.body->addWidget(sc::divider());
        boardCard.body->addSpacing(14);
    };
    showRego_ = addToggle("Registration ends", "Countdown in the left column.", initial.board.registrationEnd);
    addBoardDivider();
    showLevel_ = addToggle("Level", "Current playing level in the right column.", initial.board.level);
    addBoardDivider();
    showBreak_ =
        addToggle("Time to break", "Countdown until the next break starts.", initial.board.timeToBreak);
    addBoardDivider();
    showBreakLen_ =
        addToggle("Next break length", "How long the next (or current) break is.", initial.board.breakLength);
    addBoardDivider();
    showAvg_ = addToggle("Average stack", "Starting stack × entries, divided by remaining players.",
                         initial.board.avgStack);
    addBoardDivider();
    showPlayers_ = addToggle("Players", "Unique players who have bought in.", initial.board.players);
    addBoardDivider();
    showReentries_ = addToggle("Re-entries", "Number of rebuys.", initial.board.reentries);
    addBoardDivider();
    showTotal_ = addToggle("Total", "Buy-ins plus re-entries.", initial.board.total);
    addBoardDivider();
    showRemaining_ =
        addToggle("Remaining", "Players still in, after prize payouts.", initial.board.remaining);
    cards->addWidget(boardCard.frame);
    cards->addStretch(1);

    auto* scroll = new sc::OverlayScrollArea;
    scroll->setObjectName(QStringLiteral("scroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* content = sc::constrain(column, {32, 0, 32, 32});
    content->setObjectName(QStringLiteral("scrollContent"));
    scroll->setWidget(content);
    pageLayout->addWidget(scroll, 1);
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, headerBar, [headerBar](int value) {
        if (headerBar->property("scrolled").toBool() == (value > 0)) return;
        headerBar->setProperty("scrolled", value > 0);
        headerBar->style()->unpolish(headerBar);
        headerBar->style()->polish(headerBar);
        headerBar->update();
    });

    connect(start_, &QPushButton::clicked, &clock_, &TournamentClock::start);
    connect(pause_, &QPushButton::clicked, &clock_, &TournamentClock::pause);
    connect(prevLevel_, &QPushButton::clicked, &clock_, &TournamentClock::goToPreviousLevel);
    connect(skipLevel_, &QPushButton::clicked, &clock_, &TournamentClock::goToNextLevel);
    connect(minusMin, &QPushButton::clicked, this, [this] { clock_.addSeconds(-60); });
    connect(minusSec, &QPushButton::clicked, this, [this] { clock_.addSeconds(-30); });
    connect(reset, &QPushButton::clicked, &clock_, &TournamentClock::resetLevel);
    connect(plusSec, &QPushButton::clicked, this, [this] { clock_.addSeconds(30); });
    connect(plusMin, &QPushButton::clicked, this, [this] { clock_.addSeconds(60); });
    connect(openBoard, &QPushButton::clicked, this, &TournamentBoardControlScreen::openBoardRequested);
    connect(sendTv, &QPushButton::clicked, this, &TournamentBoardControlScreen::sendBoardToTvRequested);
    connect(fullscreen, &QPushButton::clicked, this,
            &TournamentBoardControlScreen::toggleBoardFullscreenRequested);
    connect(closeBoard, &QPushButton::clicked, this, &TournamentBoardControlScreen::closeBoardRequested);
    for (QAbstractButton* sw : {showRego_, showLevel_, showBreak_, showBreakLen_, showAvg_, showPlayers_,
                                showReentries_, showTotal_, showRemaining_}) {
        connect(sw, &QAbstractButton::toggled, this, &TournamentBoardControlScreen::persistBoardDisplay);
    }
    connect(&clock_, &TournamentClock::changed, this, &TournamentBoardControlScreen::refresh);

    sc::installFocusVisible(page);
    rebuildLevels();
    refresh();
}

void TournamentBoardControlScreen::rebuildLevels() {
    clearLayout(levelsGrid_);
    levelButtons_.clear();
    const int count = clock_.levelCount();
    if (count <= 0) {
        levelsGrid_->addWidget(
            settings::text("No blind structure. Add levels in Tournament Settings.", "emptyState", true));
        return;
    }
    constexpr int columns = 4;
    for (int i = 0; i < count; ++i) {
        const BlindLevel* level = ctx_.tournament().config().levelAt(i);
        const QString label = level && level->isBreak
                                  ? QStringLiteral("Break\n%1 min").arg(level->durationMinutes)
                                  : QStringLiteral("Lv %1\n%2")
                                        .arg(ctx_.tournament().config().playingLevelNumber(i))
                                        .arg(level ? QString::fromStdString(level->blindsLabel()) : QString());
        auto* button = new QPushButton(label);
        button->setObjectName(QStringLiteral("levelJump"));
        button->setMinimumHeight(56);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::TabFocus);
        levelsGrid_->addWidget(button, i / columns, i % columns);
        connect(button, &QPushButton::clicked, this, [this, i] { clock_.goToLevel(i); });
        levelButtons_.push_back(button);
    }
    for (int column = 0; column < columns; ++column) levelsGrid_->setColumnStretch(column, 1);
}

void TournamentBoardControlScreen::persistBoardDisplay() {
    TournamentConfig& config = ctx_.tournament().config();
    config.board.registrationEnd = showRego_->isChecked();
    config.board.level = showLevel_->isChecked();
    config.board.timeToBreak = showBreak_->isChecked();
    config.board.breakLength = showBreakLen_->isChecked();
    config.board.avgStack = showAvg_->isChecked();
    config.board.players = showPlayers_->isChecked();
    config.board.reentries = showReentries_->isChecked();
    config.board.total = showTotal_->isChecked();
    config.board.remaining = showRemaining_->isChecked();
    try {
        ctx_.saveTournamentConfig();
    } catch (const std::exception& saveError) {
        QMessageBox::critical(this, QStringLiteral("Board controls"),
                              QStringLiteral("Couldn't save board display: %1")
                                  .arg(QString::fromUtf8(saveError.what())));
        return;
    }
    emit boardDisplayChanged();
}

void TournamentBoardControlScreen::refresh() {
    const int count = clock_.levelCount();
    const int index = clock_.levelIndex();
    time_->setText(clock_.remainingText());
    level_->setText(count <= 0 ? QStringLiteral("No blind structure")
                               : QStringLiteral("Level %1 / %2").arg(index + 1).arg(count));
    const bool ended = count > 0 && !clock_.upcomingLevel() && clock_.remainingSeconds() <= 0;
    const char* timeTone = "paused";
    const char* statusTone = "paused";
    if (ended) {
        status_->setText("ENDED");
        timeTone = "ended";
        statusTone = "ended";
    } else if (clock_.running()) {
        status_->setText("LIVE");
        timeTone = "live";
        statusTone = "live";
    } else {
        status_->setText("PAUSED");
    }

    const BlindLevel* current = clock_.currentLevel();
    if (current && current->isBreak) {
        level_->setText(QStringLiteral("Break"));
        if (clock_.running()) {
            status_->setText("BREAK");
            statusTone = "break";
        }
    }
    time_->setProperty("tone", QLatin1String(timeTone));
    status_->setProperty("tone", QLatin1String(statusTone));
    time_->style()->unpolish(time_);
    time_->style()->polish(time_);
    status_->style()->unpolish(status_);
    status_->style()->polish(status_);

    if (const BlindLevel* level = clock_.activeBlinds()) {
        blinds_->setText(current && current->isBreak
                             ? QStringLiteral("Blinds stay %1    Big blind ante %2")
                                   .arg(QString::fromStdString(level->blindsLabel()),
                                        QString::fromStdString(level->anteLabel()))
                             : QStringLiteral("Blinds %1    Big blind ante %2")
                                   .arg(QString::fromStdString(level->blindsLabel()),
                                        QString::fromStdString(level->anteLabel())));
    } else {
        blinds_->setText("Blinds —");
    }
    if (const BlindLevel* next = clock_.upcomingLevel()) {
        nextBlinds_->setText(next->isBreak
                                 ? QStringLiteral("Next  Break  ·  %1 min").arg(next->durationMinutes)
                                 : QStringLiteral("Next  %1  ·  big blind ante %2")
                                       .arg(QString::fromStdString(next->blindsLabel()),
                                            QString::fromStdString(next->anteLabel())));
    } else {
        nextBlinds_->setText("Final level");
    }

    const auto stats = ctx_.reports().tournamentStats();
    pot_->setText(QStringLiteral("Prize %1    ·    Bounty %2    ·    Rake %3    ·    %4 entries")
                      .arg(money(stats.prizePool), money(stats.bountyPool), money(stats.rakeCollected))
                      .arg(stats.buyIns));

    start_->setEnabled(!clock_.running() && count > 0 && !ended);
    pause_->setEnabled(clock_.running());
    prevLevel_->setEnabled(index > 0);
    skipLevel_->setEnabled(clock_.upcomingLevel() != nullptr);

    for (int i = 0; i < static_cast<int>(levelButtons_.size()); ++i) {
        const bool on = i == index;
        if (levelButtons_[i]->property("current").toBool() == on) continue;
        levelButtons_[i]->setProperty("current", on);
        levelButtons_[i]->style()->unpolish(levelButtons_[i]);
        levelButtons_[i]->style()->polish(levelButtons_[i]);
        levelButtons_[i]->update();
    }
}

} // namespace pokersoc::ui
