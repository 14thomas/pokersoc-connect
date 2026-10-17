#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

#include "application/ReportService.h"
#include "domain/session/TournamentMode.h"
#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Widgets.h"

namespace pokersoc::ui::qt {

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

QString dollars(std::int64_t cents) { return QString::number(cents / 100.0, 'f', 2); }

bool parseDollars(const QString& text, std::int64_t& cents) {
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    if (!ok || value < 0 || !std::isfinite(value)) return false;
    cents = static_cast<std::int64_t>(std::llround(value * 100.0));
    return true;
}

/// A "Label ($): [box]" grid of price fields bound to config members.
struct PriceField {
    QString label;
    std::int64_t TournamentConfig::*member;
    QLineEdit* box = nullptr;
};

QGroupBox* makePriceGroup(const QString& title, std::vector<PriceField>& fields,
                          const TournamentConfig& config) {
    QGroupBox* group = makeGroup(title);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    int row = 0;
    for (PriceField& field : fields) {
        grid->addWidget(makeLabel(field.label), row, 0);
        field.box = new QLineEdit(dollars(config.*field.member));
        field.box->setFixedWidth(120);
        grid->addWidget(field.box, row, 1);
        ++row;
    }
    grid->setColumnStretch(2, 1);
    groupLayout(group)->addLayout(grid);
    return group;
}

} // namespace

// ---------------------------------------------------------------------------

TournamentSettingsScreen::TournamentSettingsScreen(SessionContext& context, QWidget* parent)
    : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Tournament Settings", &close));
    layout->addSpacing(16);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(false); });

    const TournamentConfig& config = ctx_.tournament().config();
    auto* content = new QWidget;
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(0, 0, 8, 0);
    sections->setSpacing(16);

    QGroupBox* pricing = makeGroup("Pricing Structure");
    QLabel* summary = makeLabel("Current: " + toQ(config.pricingSummary()), 14);
    summary->setWordWrap(true);
    summary->setContentsMargins(0, 0, 0, 12);
    groupLayout(pricing)->addWidget(summary);
    QPushButton* configure = makeButton(QStringLiteral("Configure Pricing Structure…"), QString(), 15, true);
    configure->setStyleSheet(buttonStyle("#E8F4FD", "#707070", "1px", "padding:14px 16px;"));
    groupLayout(pricing)->addWidget(configure);
    groupLayout(pricing)->addWidget(hint(
        "Choose ARC vs Non-ARC, or membership-tier pricing, then set buy-in and rebuy amounts."));
    sections->addWidget(pricing);
    connect(configure, &QPushButton::clicked, this, [this] { emit configurePricing(); });

    QGroupBox* rebuys = makeGroup("Rebuy Limit");
    auto* unlimited = new QCheckBox("Unlimited rebuys");
    unlimited->setFont(uiFont(14));
    groupLayout(rebuys)->addWidget(unlimited);
    groupLayout(rebuys)->addSpacing(8);
    auto* capRow = new QHBoxLayout;
    capRow->addWidget(makeLabel("Max rebuys per player:"));
    capRow->addSpacing(10);
    auto* cap = new QLineEdit;
    cap->setFixedWidth(80);
    cap->setValidator(new QIntValidator(0, 999, cap));
    capRow->addWidget(cap);
    capRow->addStretch(1);
    groupLayout(rebuys)->addLayout(capRow);
    groupLayout(rebuys)->addWidget(hint("Each player gets one initial buy-in plus up to this many rebuys."));
    sections->addWidget(rebuys);
    sections->addStretch(1);
    layout->addWidget(makeScroll(content), 1);

    const bool isUnlimited = config.rebuyCap <= 0;
    unlimited->setChecked(isUnlimited);
    cap->setText(isUnlimited ? "2" : QString::number(config.rebuyCap));
    cap->setEnabled(!isUnlimited);
    connect(unlimited, &QCheckBox::toggled, cap, [cap](bool on) { cap->setEnabled(!on); });

    layout->addSpacing(16);
    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    QPushButton* save = footerButton("Save Settings", 140, true);
    footer->addWidget(save);
    layout->addLayout(footer);
    connect(save, &QPushButton::clicked, this, [this, unlimited, cap] {
        int rebuyCap = 0;
        if (!unlimited->isChecked()) {
            bool ok = false;
            rebuyCap = cap->text().trimmed().toInt(&ok);
            if (!ok || rebuyCap < 1) {
                QMessageBox::warning(this, "Validation Error",
                                     "Please enter a valid rebuy cap (1 or more), or enable unlimited rebuys.");
                return;
            }
        }
        ctx_.tournament().config().rebuyCap = rebuyCap;
        try {
            ctx_.saveTournamentConfig();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Error",
                                  QStringLiteral("Could not save settings:\n%1").arg(error.what()));
            return;
        }
        emit closed(true);
    });
}

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
    auto* arcType = new TileButton(QString(), "ARC vs Non-ARC", "Two prices: ARC member and non-ARC member");
    auto* tierType = new TileButton(QString(), "Membership Tiers",
                                    "Five prices: Bronze, Silver, Gold, Platinum, Diamond");
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

    auto arcBuyIns = std::make_shared<std::vector<PriceField>>(std::vector<PriceField>{
        {"ARC Member Buy-in ($):", &TournamentConfig::arcBuyInCents},
        {"Non-ARC Member Buy-in ($):", &TournamentConfig::nonArcBuyInCents},
    });
    auto arcRebuys = std::make_shared<std::vector<PriceField>>(std::vector<PriceField>{
        {"ARC Member Rebuy ($):", &TournamentConfig::arcRebuyCents},
        {"Non-ARC Member Rebuy ($):", &TournamentConfig::nonArcRebuyCents},
    });
    arcLayout->addWidget(makePriceGroup("Initial Buy-in Amounts", *arcBuyIns, config));
    arcLayout->addWidget(makePriceGroup("Rebuy Amounts", *arcRebuys, config));
    sections->addWidget(arcPanel);

    // Membership tier fields
    auto* tierPanel = new QWidget;
    auto* tierLayout = new QVBoxLayout(tierPanel);
    tierLayout->setContentsMargins(0, 0, 0, 0);
    tierLayout->setSpacing(16);
    auto tierBuyIns = std::make_shared<std::vector<PriceField>>(std::vector<PriceField>{
        {"Bronze Buy-in ($):", &TournamentConfig::bronzeBuyInCents},
        {"Silver Buy-in ($):", &TournamentConfig::silverBuyInCents},
        {"Gold Buy-in ($):", &TournamentConfig::goldBuyInCents},
        {"Platinum Buy-in ($):", &TournamentConfig::platinumBuyInCents},
        {"Diamond Buy-in ($):", &TournamentConfig::diamondBuyInCents},
    });
    auto tierRebuys = std::make_shared<std::vector<PriceField>>(std::vector<PriceField>{
        {"Bronze Rebuy ($):", &TournamentConfig::bronzeRebuyCents},
        {"Silver Rebuy ($):", &TournamentConfig::silverRebuyCents},
        {"Gold Rebuy ($):", &TournamentConfig::goldRebuyCents},
        {"Platinum Rebuy ($):", &TournamentConfig::platinumRebuyCents},
        {"Diamond Rebuy ($):", &TournamentConfig::diamondRebuyCents},
    });
    tierLayout->addWidget(makePriceGroup("Initial Buy-in Amounts", *tierBuyIns, config));
    tierLayout->addWidget(makePriceGroup("Rebuy Amounts", *tierRebuys, config));
    sections->addWidget(tierPanel);
    sections->addStretch(1);
    layout->addWidget(makeScroll(content), 1);

    auto selected = std::make_shared<TournamentPricing>(config.pricing);
    const auto applyType = [=] {
        const bool arc = *selected == TournamentPricing::ArcVsNonArc;
        arcPanel->setVisible(arc);
        tierPanel->setVisible(!arc);
        arcType->setColors(arc ? "#E8F4FD" : "#F5F5F5", arc ? "#2E6DA4" : color::Gray, "3px");
        tierType->setColors(arc ? "#F5F5F5" : "#E8F4FD", arc ? color::Gray : "#2E6DA4", "3px");
    };
    applyType();
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
                const auto read = [&updated](const std::vector<PriceField>& fields) {
                    for (const PriceField& field : fields) {
                        std::int64_t cents = 0;
                        if (!parseDollars(field.box->text(), cents)) return false;
                        updated.*field.member = cents;
                    }
                    return true;
                };
                const bool valid = arc ? read(*arcBuyIns) && read(*arcRebuys)
                                       : read(*tierBuyIns) && read(*tierRebuys);
                if (!valid) {
                    QMessageBox::warning(this, "Validation Error",
                                         arc ? "Please enter valid ARC buy-in and rebuy amounts."
                                             : "Please enter valid membership buy-in and rebuy amounts.");
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

TournamentInfoScreen::TournamentInfoScreen(SessionContext& context, QWidget* parent)
    : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Tournament Info", &close));
    layout->addSpacing(24);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(false); });

    const auto stats = ctx_.reports().tournamentStats();
    const auto makeCard = [&](const QString& title, int total, const QString& accent,
                              const QString& background, bool rebuys) {
        QFrame* card = makePanel(background, accent, 2, 4, 24);
        card->setFixedWidth(240);
        QVBoxLayout* cardLayout = panelLayout(card);
        cardLayout->addWidget(makeLabel(title, 16, true, "rgba(0,0,0,204)", Qt::AlignCenter));
        cardLayout->addSpacing(8);
        cardLayout->addWidget(makeLabel(QString::number(total), 48, true, accent, Qt::AlignCenter));
        cardLayout->addSpacing(12);
        for (const auto& tier : stats.tiers) {
            const int count = rebuys ? tier.rebuys : tier.buyIns;
            if (count <= 0) continue;
            QLabel* row = makeLabel(QStringLiteral("%1: %2").arg(toQ(tier.tier)).arg(count), 14, false,
                                    accent, Qt::AlignCenter);
            QFont semiBold = row->font();
            semiBold.setWeight(QFont::DemiBold);
            row->setFont(semiBold);
            row->setContentsMargins(0, 2, 0, 2);
            cardLayout->addWidget(row);
        }
        return card;
    };

    auto* cards = new QHBoxLayout;
    cards->addStretch(1);
    cards->addWidget(makeCard("Total Buy-ins", stats.buyIns, "#2E6DA4", "#E8F4FD", false), 0, Qt::AlignTop);
    cards->addSpacing(40);
    cards->addWidget(makeCard("Total Rebuys", stats.rebuys, "#B8860B", "#FFF8E8", true), 0, Qt::AlignTop);
    cards->addStretch(1);
    layout->addStretch(1);
    layout->addLayout(cards);
    layout->addStretch(1);

    layout->addSpacing(16);
    QPushButton* back = makeButton("Back to Main", QString(), 16);
    back->setStyleSheet("QPushButton { padding:12px 20px; }");
    layout->addWidget(back, 0, Qt::AlignHCenter);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });
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

} // namespace pokersoc::ui::qt
