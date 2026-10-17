#include "ui/qt/TournamentBoardWindow.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWidget>

#include "application/ReportService.h"
#include "domain/session/TournamentConfig.h"
#include "domain/session/TournamentMode.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Style.h"
#include "ui/qt/TournamentClock.h"
#include "ui/qt/Widgets.h"

namespace pokersoc::ui::qt {

using domain::BlindLevel;
using domain::PrizePayout;
using domain::TournamentConfig;

namespace {

constexpr const char* kFelt = "#0B2E24";
constexpr const char* kFeltPanel = "#123D31";
constexpr const char* kGold = "#E8C547";
constexpr const char* kCream = "#F5F0E6";
constexpr const char* kMuted = "#9FCBB8";
constexpr const char* kWarn = "#FFB74D";
constexpr const char* kHot = "#FF6B6B";

QString ordinal(int place) {
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

QLabel* creamLabel(const QString& text, int px, bool bold, const QString& colour = kCream) {
    QLabel* label = makeLabel(text, px, bold, colour, Qt::AlignCenter);
    label->setWordWrap(true);
    return label;
}

} // namespace

TournamentBoardWindow::TournamentBoardWindow(SessionContext& context, TournamentClock& clock,
                                             QWidget* parent)
    : QMainWindow(parent), ctx_(context), clock_(clock) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("pokersoc-connect — Tournament Board"));
    resize(1280, 720);

    auto* central = new QWidget;
    central->setObjectName("board");
    central->setStyleSheet(QStringLiteral("QWidget#board { background:%1; }").arg(kFelt));
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(36, 28, 36, 20);
    root->setSpacing(0);

    auto* header = new QHBoxLayout;
    title_ = creamLabel("POKERSOC TOURNAMENT", 28, true, kGold);
    title_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    status_ = creamLabel(QString(), 18, true, kWarn);
    status_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(title_, 1);
    header->addWidget(status_, 0, Qt::AlignRight);
    root->addLayout(header);

    level_ = creamLabel(QString(), 22, true, kMuted);
    level_->setContentsMargins(0, 18, 0, 0);
    root->addWidget(level_);

    time_ = creamLabel("00:00", 120, true, kCream);
    time_->setContentsMargins(0, 4, 0, 8);
    root->addWidget(time_);

    auto* blinds = new QHBoxLayout;
    blinds->setSpacing(18);
    blinds->addWidget(makeBlindCard("SMALL BLIND", &smallBlind_), 1);
    blinds->addWidget(makeBlindCard("BIG BLIND", &bigBlind_), 1);
    blinds->addWidget(makeBlindCard("ANTE", &ante_), 1);
    root->addLayout(blinds);

    nextBlinds_ = creamLabel(QString(), 20, false, kMuted);
    nextBlinds_->setContentsMargins(0, 16, 0, 8);
    root->addWidget(nextBlinds_);

    QFrame* potPanel = makePanel(kFeltPanel, kGold, 2, 12, 18);
    auto* potLayout = panelLayout(potPanel);
    potLayout->addWidget(creamLabel("PRIZE POOL", 16, true, kMuted));
    pot_ = creamLabel("$0.00", 48, true, kGold);
    potLayout->addWidget(pot_);
    auto* counts = new QHBoxLayout;
    entries_ = creamLabel("Entries  0", 18, true, kCream);
    rebuys_ = creamLabel("Rebuys  0", 18, true, kCream);
    counts->addWidget(entries_);
    counts->addWidget(rebuys_);
    potLayout->addSpacing(6);
    potLayout->addLayout(counts);
    root->addSpacing(16);
    root->addWidget(potPanel);

    root->addSpacing(16);
    root->addWidget(creamLabel("PAYOUTS", 14, true, kMuted));
    prizeRow_ = new QWidget;
    prizeLayout_ = new QHBoxLayout(prizeRow_);
    prizeLayout_->setContentsMargins(0, 8, 0, 0);
    prizeLayout_->setSpacing(12);
    root->addWidget(prizeRow_);
    root->addStretch(1);

    hint_ = creamLabel("F11  fullscreen    ·    Space  start/pause    ·    ← →  levels", 12, false,
                       kMuted);
    hint_->setContentsMargins(0, 12, 0, 0);
    root->addWidget(hint_);

    auto* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullscreen->setContext(Qt::WindowShortcut);
    connect(fullscreen, &QShortcut::activated, this, [this] {
        if (isFullScreen()) showNormal();
        else showFullScreen();
        hint_->setVisible(!isFullScreen());
    });
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WindowShortcut);
    connect(escape, &QShortcut::activated, this, [this] {
        if (isFullScreen()) {
            showNormal();
            hint_->show();
        }
    });
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    space->setContext(Qt::WindowShortcut);
    connect(space, &QShortcut::activated, &clock_, &TournamentClock::toggle);
    auto* next = new QShortcut(QKeySequence(Qt::Key_Right), this);
    next->setContext(Qt::WindowShortcut);
    connect(next, &QShortcut::activated, &clock_, &TournamentClock::goToNextLevel);
    auto* prev = new QShortcut(QKeySequence(Qt::Key_Left), this);
    prev->setContext(Qt::WindowShortcut);
    connect(prev, &QShortcut::activated, &clock_, &TournamentClock::goToPreviousLevel);

    connect(&clock_, &TournamentClock::changed, this, &TournamentBoardWindow::refreshClock);
    refreshAll();
}

QFrame* TournamentBoardWindow::makeBlindCard(const QString& caption, QLabel** value) {
    QFrame* card = makePanel(kFeltPanel, "#1F5A48", 2, 12, 16);
    auto* layout = panelLayout(card);
    layout->addWidget(creamLabel(caption, 14, true, kMuted));
    *value = creamLabel("—", 42, true, kCream);
    layout->addWidget(*value);
    return card;
}

void TournamentBoardWindow::showOnRoomDisplay(QWidget* host) {
    QScreen* hostScreen = host ? host->screen() : QGuiApplication::primaryScreen();
    QScreen* room = nullptr;
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen != hostScreen) {
            room = screen;
            break;
        }
    }
    show();
    if (room) {
        setGeometry(room->geometry());
        setBoardFullScreen(true);
    } else {
        setBoardFullScreen(false);
        raise();
        activateWindow();
    }
}

void TournamentBoardWindow::setBoardFullScreen(bool on) {
    if (on) {
        showFullScreen();
        hint_->hide();
    } else {
        showNormal();
        hint_->show();
    }
}

void TournamentBoardWindow::refreshStats() {
    const auto stats = ctx_.reports().tournamentStats();
    pot_->setText(money(stats.prizePool));
    QString counts = QStringLiteral("Entries  %1     Rebuys  %2").arg(stats.buyIns).arg(stats.rebuys);
    if (!stats.bountyPool.isZero()) {
        counts += QStringLiteral("     Bounty  %1").arg(money(stats.bountyPool));
    }
    entries_->setText(counts);
    rebuys_->hide();

    clearLayout(prizeLayout_);
    const TournamentConfig& config = ctx_.tournament().config();
    if (config.prizes.empty()) {
        prizeLayout_->addWidget(creamLabel("Set payouts in Prizing", 16, false, kMuted));
        return;
    }
    prizeLayout_->addStretch(1);
    for (const PrizePayout& prize : config.prizePayouts(stats.prizePool)) {
        QFrame* card = makePanel(kFeltPanel, kGold, 1, 10, 12);
        auto* layout = panelLayout(card);
        const QString place = stats.placePaid(prize.place) ? ordinal(prize.place) + "  PAID"
                                                           : ordinal(prize.place);
        layout->addWidget(creamLabel(place, 16, true, kGold));
        layout->addWidget(creamLabel(money(prize.amount), 22, true, kCream));
        prizeLayout_->addWidget(card, 1);
    }
    prizeLayout_->addStretch(1);
}

void TournamentBoardWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    applyFonts();
}

void TournamentBoardWindow::applyFonts() {
    const int h = qMax(1, height());
    const int scale = qMin(h, qMax(1, width()));
    title_->setFont(boardFont(qBound(18, scale / 32, 40), true));
    status_->setFont(boardFont(qBound(14, scale / 40, 28), true));
    level_->setFont(boardFont(qBound(16, h / 28, 32), true));
    time_->setFont(boardFont(qBound(64, h / 5, 220), true));
    const int blindPx = qBound(28, h / 16, 72);
    for (QLabel* label : {smallBlind_, bigBlind_, ante_}) {
        label->setFont(boardFont(blindPx, true));
    }
    nextBlinds_->setFont(boardFont(qBound(14, h / 32, 28), false));
    pot_->setFont(boardFont(qBound(28, h / 14, 72), true));
    entries_->setFont(boardFont(qBound(14, h / 36, 24), true));
    rebuys_->setFont(boardFont(qBound(14, h / 36, 24), true));
}

void TournamentBoardWindow::refreshClock() {
    const int count = clock_.levelCount();
    const int index = clock_.levelIndex();
    if (count <= 0) {
        level_->setText("NO BLIND STRUCTURE");
        time_->setText("—");
        smallBlind_->setText("—");
        bigBlind_->setText("—");
        ante_->setText("—");
        nextBlinds_->setText("Add blinds in Tournament Settings");
        status_->setText(QString());
        return;
    }

    level_->setText(QStringLiteral("LEVEL  %1  /  %2").arg(index + 1).arg(count));
    time_->setText(clock_.remainingText());

    const bool last = clock_.upcomingLevel() == nullptr;
    const bool ended = last && clock_.remainingSeconds() <= 0;
    if (ended) {
        status_->setText("ENDED");
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(kHot));
    } else if (!clock_.running()) {
        status_->setText("PAUSED");
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(kWarn));
    } else if (clock_.remainingSeconds() <= 30) {
        status_->setText("LEVEL UP");
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(kHot));
    } else {
        status_->setText("LIVE");
        status_->setStyleSheet(QStringLiteral("color:%1;").arg(kGold));
    }

    const QString timeColour = !clock_.running()                      ? kWarn
                               : clock_.remainingSeconds() <= 30      ? kHot
                               : clock_.remainingSeconds() <= 60      ? kWarn
                                                                      : kCream;
    time_->setStyleSheet(QStringLiteral("color:%1;").arg(timeColour));

    if (const BlindLevel* level = clock_.currentLevel()) {
        const QString blinds = QString::fromStdString(level->blindsLabel());
        const QStringList parts = blinds.split(" / ");
        smallBlind_->setText(parts.value(0, QStringLiteral("—")));
        bigBlind_->setText(parts.value(1, QStringLiteral("—")));
        ante_->setText(QString::fromStdString(level->anteLabel()));
    }

    if (const BlindLevel* next = clock_.upcomingLevel()) {
        const QString ante = next->ante > 0
                                 ? QStringLiteral("  ·  ante %1").arg(QString::fromStdString(next->anteLabel()))
                                 : QString();
        nextBlinds_->setText(QStringLiteral("NEXT  %1%2")
                                 .arg(QString::fromStdString(next->blindsLabel()), ante));
    } else {
        nextBlinds_->setText("FINAL LEVEL");
    }
}

void TournamentBoardWindow::refreshAll() {
    refreshClock();
    refreshStats();
    applyFonts();
}

QFont TournamentBoardWindow::boardFont(int pixelSize, bool bold) const {
    return uiFont(pixelSize, bold);
}

} // namespace pokersoc::ui::qt
