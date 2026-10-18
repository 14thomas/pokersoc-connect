#include "ui/qt/TournamentBoardWindow.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShortcut>
#include <QSizePolicy>
#include <QTime>
#include <QTimer>
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
using domain::Money;
using domain::PrizePayout;
using domain::TournamentConfig;

namespace {

constexpr const char* kNavy = "#0A1628";
constexpr const char* kNavyPanel = "#13233A";
constexpr const char* kNavyBorder = "#2A4A6B";
constexpr const char* kWhite = "#FFFFFF";
constexpr const char* kWarn = "#FFB74D";
constexpr const char* kHot = "#FF6B6B";
constexpr const char* kLine = "#FFFFFF";

QLabel* boardLabel(const QString& text, int px, bool bold, const QString& colour, Qt::Alignment align) {
    QLabel* label = makeLabel(text, px, bold, colour, align);
    label->setWordWrap(true);
    return label;
}

QString clockText(int seconds) {
    if (seconds < 0) seconds = 0;
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

QString grouped(int value) { return QLocale(QLocale::English).toString(value); }

QString wallClockText() {
    const QTime now = QTime::currentTime();
    int hour = now.hour() % 12;
    if (hour == 0) hour = 12;
    const QString period = now.hour() >= 12 ? QStringLiteral("PM") : QStringLiteral("AM");
    return QStringLiteral("%1:%2 %3")
        .arg(hour, 2, 10, QChar('0'))
        .arg(now.minute(), 2, 10, QChar('0'))
        .arg(period);
}

QString slashBlinds(const BlindLevel& level) {
    return QStringLiteral("%1/%2").arg(grouped(level.smallBlind), grouped(level.bigBlind));
}

QString placeOrdinal(int place) {
    const int mod100 = place % 100;
    const int mod10 = place % 10;
    const char* suffix = "TH";
    if (mod100 < 11 || mod100 > 13) {
        if (mod10 == 1) suffix = "ST";
        else if (mod10 == 2) suffix = "ND";
        else if (mod10 == 3) suffix = "RD";
    }
    return QStringLiteral("%1%2").arg(place).arg(QLatin1String(suffix));
}

/// Diagonal watermark: lower centre → right (roughly 6 o'clock → 3 o'clock).
class BoardLogoOverlay : public QWidget {
public:
    explicit BoardLogoOverlay(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        reload();
    }

    void reload() { logo_ = QPixmap(assetPath(QStringLiteral("Brand/5.png"))); }

protected:
    void paintEvent(QPaintEvent*) override {
        if (logo_.isNull() || width() < 10 || height() < 10) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setOpacity(0.067); // hehehehe to anyone that finds this

        // Bottom toward 6 o'clock, top toward 3 — nudged a bit CCW (left).
        const QPointF pivot(width() * 0.76, height() * 0.72);
        p.translate(pivot);
        p.rotate(32.0);

        const int targetW = qMax(140, qRound(width() * 0.36));
        const QPixmap scaled =
            logo_.scaledToWidth(targetW, Qt::SmoothTransformation);
        p.drawPixmap(-scaled.width() / 2, -scaled.height() / 2, scaled);
    }

private:
    QPixmap logo_;
};

} // namespace

TournamentBoardWindow::TournamentBoardWindow(SessionContext& context, TournamentClock& clock,
                                             QWidget* parent)
    : QMainWindow(parent), ctx_(context), clock_(clock) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("pokersoc-connect — Tournament Board"));
    resize(1280, 720);

    auto* central = new QWidget;
    central->setObjectName("board");
    central->setStyleSheet(QStringLiteral("QWidget#board { background:%1; }").arg(kNavy));
    setCentralWidget(central);

    logo_ = new BoardLogoOverlay(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(36, 20, 36, 24);
    root->setSpacing(0);

    auto* header = new QWidget;
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(16);
    auto* leftPad = new QWidget;
    leftPad->setFixedWidth(160);
    wallClock_ = boardLabel(QString(), 20, true, kWhite, Qt::AlignRight | Qt::AlignVCenter);
    wallClock_->setWordWrap(false);
    wallClock_->setFixedWidth(160);
    name_ = boardLabel(QString(), 32, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
    headerRow->addWidget(leftPad, 0, Qt::AlignVCenter);
    headerRow->addWidget(name_, 1);
    headerRow->addWidget(wallClock_, 0, Qt::AlignVCenter);
    root->addWidget(header);

    auto* body = new QWidget;
    auto* columns = new QHBoxLayout(body);
    columns->setContentsMargins(0, 24, 0, 16);
    columns->setSpacing(0);

    leftCol_ = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftCol_);
    leftLayout->setContentsMargins(4, 8, 24, 8);
    leftLayout->setSpacing(8);
    leftTopStretch_ = new QWidget;
    leftBottomStretch_ = new QWidget;
    leftTopStretch_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    leftBottomStretch_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rego_ = makeStat(QStringLiteral("REGISTRATION ENDS"), Qt::AlignHCenter | Qt::AlignVCenter);
    leftLayout->addWidget(leftTopStretch_, 1);
    leftLayout->addWidget(rego_.widget, 0);
    leftLayout->addWidget(leftBottomStretch_, 1);

    prizeTitle_ = boardLabel(QStringLiteral("PRIZES"), 14, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
    prizeTitle_->setWordWrap(false);
    prizeTitle_->hide();
    leftLayout->addWidget(prizeTitle_);

    prizeScroll_ = new QScrollArea;
    prizeScroll_->setWidgetResizable(true);
    prizeScroll_->setFrameShape(QFrame::NoFrame);
    prizeScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    prizeScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    prizeScroll_->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; border: none; }"));
    prizeList_ = new QWidget;
    prizeList_->setStyleSheet(QStringLiteral("background: transparent;"));
    prizeLayout_ = new QVBoxLayout(prizeList_);
    prizeLayout_->setContentsMargins(0, 0, 0, 0);
    prizeLayout_->setSpacing(16);
    prizeLayout_->addStretch(1);
    prizeScroll_->setWidget(prizeList_);
    prizeScroll_->hide();
    leftLayout->addWidget(prizeScroll_, 1);

    auto* centre = new QWidget;
    auto* centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(8, 0, 8, 0);
    centreLayout->setSpacing(0);
    time_ = boardLabel("00:00", 160, true, kWhite, Qt::AlignCenter);
    time_->setWordWrap(false);
    time_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    infoBlock_ = new QWidget;
    auto* infoLayout = new QVBoxLayout(infoBlock_);
    // Pull blinds / next-level boxes in from the column dividers.
    infoLayout->setContentsMargins(36, 0, 36, 0);
    infoLayout->setSpacing(14);
    infoBlock_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    blindsBox_ = makePanel(kNavyPanel, kNavyBorder, 1, 22, 26);
    auto* blindsLayout = panelLayout(blindsBox_);
    blindsLayout->setSpacing(10);
    blindsRow_ = makeBoardRow(QStringLiteral("BLINDS:"));
    anteRow_ = makeBoardRow(QStringLiteral("BIG BLIND ANTE:"));
    blindsLayout->addWidget(blindsRow_.widget);
    blindsLayout->addWidget(anteRow_.widget);
    anteRow_.widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    blindsBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    nextBox_ = makePanel(kNavyPanel, kNavyBorder, 1, 22, 22);
    auto* nextLayout = panelLayout(nextBox_);
    nextRow_ = makeBoardRow(QStringLiteral("NEXT LEVEL:"));
    nextLayout->addWidget(nextRow_.widget);
    nextBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    infoLayout->addWidget(blindsBox_);
    infoLayout->addWidget(nextBox_);

    centreLayout->addStretch(2);
    centreLayout->addWidget(time_, 0, Qt::AlignHCenter);
    centreLayout->addSpacing(18);
    centreLayout->addWidget(infoBlock_);
    centreLayout->addStretch(5);

    rightCol_ = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightCol_);
    rightLayout->setContentsMargins(24, 8, 4, 8);
    rightLayout->setSpacing(0);
    level_ = makeStat(QStringLiteral("LEVEL"));
    timeToBreak_ = makeStat(QStringLiteral("TIME TO BREAK"));
    breakLength_ = makeStat(QStringLiteral("NEXT BREAK"));
    avgStack_ = makeStat(QStringLiteral("AVG STACK"));
    players_ = makeStat(QStringLiteral("PLAYERS"));
    reentries_ = makeStat(QStringLiteral("RE-ENTRIES"));
    total_ = makeStat(QStringLiteral("TOTAL"));
    remaining_ = makeStat(QStringLiteral("REMAINING"));
    for (SideStat* stat : {&level_, &timeToBreak_, &breakLength_, &avgStack_, &players_, &reentries_,
                           &total_, &remaining_}) {
        rightLayout->addWidget(stat->widget);
    }
    rightLayout->addStretch(1);

    leftDivider_ = makeDivider();
    rightDivider_ = makeDivider();
    columns->addWidget(leftCol_, 5);
    columns->addWidget(leftDivider_);
    columns->addWidget(centre, 9);
    columns->addWidget(rightDivider_);
    columns->addWidget(rightCol_, 5);
    root->addWidget(body, 1);

    auto* fullscreen = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullscreen->setContext(Qt::WindowShortcut);
    connect(fullscreen, &QShortcut::activated, this, [this] {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WindowShortcut);
    connect(escape, &QShortcut::activated, this, [this] {
        if (isFullScreen()) showNormal();
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

    wallTimer_ = new QTimer(this);
    wallTimer_->setInterval(1000);
    connect(wallTimer_, &QTimer::timeout, this, &TournamentBoardWindow::refreshWallClock);
    wallTimer_->start();

    prizeScrollTimer_ = new QTimer(this);
    prizeScrollTimer_->setInterval(40);
    connect(prizeScrollTimer_, &QTimer::timeout, this, &TournamentBoardWindow::tickPrizeScroll);

    connect(&clock_, &TournamentClock::changed, this, &TournamentBoardWindow::refreshAll);
    layoutLogo();
    refreshAll();
}

TournamentBoardWindow::SideStat TournamentBoardWindow::makeStat(const QString& caption,
                                                               Qt::Alignment align) {
    SideStat stat;
    stat.widget = new QWidget;
    auto* layout = new QVBoxLayout(stat.widget);
    layout->setContentsMargins(0, 0, 0, 18);
    layout->setSpacing(2);
    stat.caption = boardLabel(caption, 12, true, kWhite, align);
    stat.value = boardLabel(QStringLiteral("—"), 28, true, kWhite, align);
    stat.caption->setWordWrap(false);
    stat.value->setWordWrap(false);
    layout->addWidget(stat.caption);
    layout->addWidget(stat.value);
    return stat;
}

TournamentBoardWindow::BoardRow TournamentBoardWindow::makeBoardRow(const QString& caption) {
    BoardRow row;
    row.widget = new QWidget;
    auto* layout = new QHBoxLayout(row.widget);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(16);
    row.label = boardLabel(caption, 20, true, kWhite, Qt::AlignLeft | Qt::AlignVCenter);
    row.value = boardLabel(QStringLiteral("—"), 26, true, kWhite, Qt::AlignRight | Qt::AlignVCenter);
    row.label->setWordWrap(false);
    row.value->setWordWrap(false);
    layout->addWidget(row.label, 0, Qt::AlignVCenter);
    layout->addWidget(row.value, 1, Qt::AlignVCenter);
    return row;
}

QWidget* TournamentBoardWindow::makeDivider() {
    auto* wrap = new QWidget;
    auto* layout = new QVBoxLayout(wrap);
    layout->setContentsMargins(0, 28, 0, 28);
    layout->setSpacing(0);
    auto* line = new QWidget;
    line->setFixedWidth(1);
    line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    line->setStyleSheet(QStringLiteral("background:%1;").arg(kLine));
    layout->addWidget(line, 1);
    wrap->setFixedWidth(1);
    return wrap;
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
    static_cast<BoardLogoOverlay*>(logo_)->reload();
    show();
    layoutLogo();
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
    if (on) showFullScreen();
    else showNormal();
}

void TournamentBoardWindow::refreshStats() { refreshAll(); }

void TournamentBoardWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    layoutLogo();
    applyFonts();
}

void TournamentBoardWindow::layoutLogo() {
    if (!logo_ || !centralWidget()) return;
    logo_->setGeometry(centralWidget()->rect());
    logo_->raise();
    logo_->update();
}

void TournamentBoardWindow::applyFonts() {
    const int h = qMax(1, height());
    const int scale = qMin(h, qMax(1, width()));
    name_->setFont(boardFont(qBound(26, scale / 22, 56), true));
    wallClock_->setFont(boardFont(qBound(16, scale / 36, 32), true));
    time_->setFont(boardFont(qBound(120, h / 3, 300), true));
    const int labelPx = qBound(18, h / 24, 34);
    const int valuePx = qBound(24, h / 18, 44);
    for (BoardRow* row : {&blindsRow_, &anteRow_, &nextRow_}) {
        row->label->setFont(boardFont(labelPx, true));
        row->value->setFont(boardFont(valuePx, true));
    }
    const int rowH = qMax(valuePx, labelPx) + 10;
    anteRow_.widget->setFixedHeight(rowH);
    blindsRow_.widget->setFixedHeight(rowH);
    nextRow_.widget->setFixedHeight(rowH);
    blindsBox_->setFixedHeight(rowH * 2 + 10 + 52);
    nextBox_->setFixedHeight(rowH + 44);
    prizeTitle_->setFont(boardFont(qBound(18, h / 28, 32), true));
    rego_.caption->setFont(boardFont(qBound(16, h / 32, 28), true));
    rego_.value->setFont(boardFont(qBound(48, h / 10, 96), true));
    const int capPx = qBound(11, h / 48, 16);
    const int valPx = qBound(20, h / 22, 36);
    for (SideStat* stat : {&level_, &timeToBreak_, &breakLength_, &avgStack_, &players_,
                           &reentries_, &total_, &remaining_}) {
        stat->caption->setFont(boardFont(capPx, true));
        stat->value->setFont(boardFont(valPx, true));
    }
    const int prizeCap = qBound(22, h / 22, 40);
    const int prizeVal = qBound(28, h / 16, 56);
    for (int i = 0; i < prizeLayout_->count(); ++i) {
        QLayoutItem* item = prizeLayout_->itemAt(i);
        QWidget* widget = item ? item->widget() : nullptr;
        if (!widget) continue;
        if (auto* layout = qobject_cast<QHBoxLayout*>(widget->layout())) {
            if (layout->count() >= 2) {
                if (auto* left = qobject_cast<QLabel*>(layout->itemAt(0)->widget()))
                    left->setFont(boardFont(prizeCap, true));
                if (auto* right = qobject_cast<QLabel*>(layout->itemAt(1)->widget()))
                    right->setFont(boardFont(prizeVal, true));
            } else if (layout->count() == 0) {
                // empty
            } else if (auto* alone = qobject_cast<QLabel*>(widget)) {
                alone->setFont(boardFont(prizeVal, true));
            }
        } else if (auto* alone = qobject_cast<QLabel*>(widget)) {
            alone->setFont(boardFont(prizeVal, true));
        }
    }
}

void TournamentBoardWindow::refreshWallClock() { wallClock_->setText(wallClockText()); }

void TournamentBoardWindow::refreshPrizes(bool regoClosed) {
    prizeTitle_->setVisible(regoClosed);
    prizeScroll_->setVisible(regoClosed);
    leftTopStretch_->setVisible(!regoClosed);
    leftBottomStretch_->setVisible(!regoClosed);
    rego_.widget->setVisible(!regoClosed);

    if (!regoClosed) {
        prizesShowing_ = false;
        prizesFingerprint_.clear();
        prizeScrollTimer_->stop();
        return;
    }

    const TournamentConfig& config = ctx_.tournament().config();
    const auto stats = ctx_.reports().tournamentStats();
    const auto payouts = config.prizePayouts(stats.prizePool);
    QString fingerprint;
    for (const PrizePayout& prize : payouts) {
        fingerprint += QStringLiteral("%1:%2;").arg(prize.place).arg(prize.amount.cents());
    }
    if (prizesShowing_ && fingerprint == prizesFingerprint_) return;
    prizesShowing_ = true;
    prizesFingerprint_ = fingerprint;

    while (QLayoutItem* item = prizeLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    if (payouts.empty()) {
        auto* empty = boardLabel(QStringLiteral("NO PRIZES SET"), 16, true, kWhite,
                                 Qt::AlignHCenter | Qt::AlignVCenter);
        prizeLayout_->addWidget(empty);
        prizeLayout_->addStretch(1);
        prizeScrollTimer_->stop();
        return;
    }

    for (const PrizePayout& prize : payouts) {
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto* place = boardLabel(placeOrdinal(prize.place), 14, true, kWhite,
                                 Qt::AlignLeft | Qt::AlignVCenter);
        auto* amount = boardLabel(money(prize.amount).toUpper(), 18, true, kWhite,
                                  Qt::AlignRight | Qt::AlignVCenter);
        place->setWordWrap(false);
        amount->setWordWrap(false);
        rowLayout->addWidget(place, 0);
        rowLayout->addWidget(amount, 1);
        prizeLayout_->addWidget(row);
    }
    prizeLayout_->addStretch(1);

    prizeScroll_->verticalScrollBar()->setValue(0);
    prizeScrollDir_ = 1;
    QTimer::singleShot(0, this, [this] {
        applyFonts();
        QScrollBar* bar = prizeScroll_->verticalScrollBar();
        if (bar && bar->maximum() > 0) prizeScrollTimer_->start();
        else prizeScrollTimer_->stop();
    });
}

void TournamentBoardWindow::tickPrizeScroll() {
    QScrollBar* bar = prizeScroll_->verticalScrollBar();
    if (!bar || bar->maximum() <= 0 || !prizeScroll_->isVisible()) {
        prizeScrollTimer_->stop();
        return;
    }
    const int next = bar->value() + prizeScrollDir_;
    if (next >= bar->maximum()) {
        bar->setValue(bar->maximum());
        prizeScrollDir_ = -1;
    } else if (next <= bar->minimum()) {
        bar->setValue(bar->minimum());
        prizeScrollDir_ = 1;
    } else {
        bar->setValue(next);
    }
}

void TournamentBoardWindow::refreshAll() {
    const TournamentConfig& config = ctx_.tournament().config();
    const auto& board = config.board;
    const QString title = QString::fromStdString(config.name).trimmed();
    name_->setText(title.isEmpty() ? QStringLiteral("Tournament") : title);
    refreshWallClock();

    const int count = clock_.levelCount();
    const int index = clock_.levelIndex();
    const BlindLevel* current = clock_.currentLevel();
    const bool onBreak = current && current->isBreak;
    const bool last = clock_.upcomingLevel() == nullptr;
    const bool ended = count > 0 && last && clock_.remainingSeconds() <= 0;

    const auto setAnte = [this](int ante) {
        if (ante > 0) {
            anteRow_.label->setText(QStringLiteral("BIG BLIND ANTE:"));
            anteRow_.value->setText(grouped(ante));
        } else {
            anteRow_.label->setText(QString());
            anteRow_.value->setText(QString());
        }
    };

    if (count <= 0) {
        time_->setText(QStringLiteral("—"));
        time_->setStyleSheet(QStringLiteral("color:%1;").arg(kWhite));
        blindsRow_.value->setText(QStringLiteral("—"));
        setAnte(0);
        nextRow_.value->setText(QStringLiteral("—"));
    } else {
        time_->setText(clock_.remainingText());
        const QString timeColour = ended                             ? kHot
                                   : !clock_.running()               ? kWarn
                                   : clock_.remainingSeconds() <= 30 ? kHot
                                   : clock_.remainingSeconds() <= 60 ? kWarn
                                                                     : kWhite;
        time_->setStyleSheet(QStringLiteral("color:%1;").arg(timeColour));

        const BlindLevel* blinds = clock_.activeBlinds();
        if (onBreak) {
            blindsRow_.value->setText(QStringLiteral("BREAK"));
            setAnte(blinds ? blinds->ante : 0);
        } else if (blinds) {
            blindsRow_.value->setText(slashBlinds(*blinds));
            setAnte(blinds->ante);
        } else {
            blindsRow_.value->setText(QStringLiteral("—"));
            setAnte(0);
        }

        const BlindLevel* nextPlaying = nullptr;
        for (int i = index + 1; i < count; ++i) {
            const BlindLevel* level = config.levelAt(i);
            if (level && !level->isBreak) {
                nextPlaying = level;
                break;
            }
        }
        if (nextPlaying) {
            QString next = slashBlinds(*nextPlaying);
            if (nextPlaying->ante > 0) {
                next += QStringLiteral(" (%1)").arg(grouped(nextPlaying->ante));
            }
            nextRow_.value->setText(next);
        } else {
            nextRow_.value->setText(QStringLiteral("FINAL LEVEL"));
        }
    }

    const bool showLeft = board.registrationEnd;
    leftCol_->setVisible(showLeft);
    leftDivider_->setVisible(showLeft);
    bool regoClosed = false;
    if (showLeft) {
        const int closeLevel = config.registrationEndsLevel;
        const int closeIndex = config.indexOfPlayingLevel(closeLevel);
        if (closeLevel < 1 || closeIndex < 0) {
            rego_.value->setText(QStringLiteral("OPEN"));
            regoClosed = false;
        } else if (index >= closeIndex) {
            regoClosed = true;
        } else {
            rego_.value->setText(clockText(clock_.secondsUntilIndex(closeIndex)));
            regoClosed = false;
        }
        refreshPrizes(regoClosed);
    } else {
        prizeScrollTimer_->stop();
    }

    level_.widget->setVisible(board.level);
    if (board.level) {
        if (count <= 0) {
            level_.value->setText(QStringLiteral("—"));
        } else if (onBreak) {
            level_.value->setText(QStringLiteral("BREAK"));
        } else {
            level_.value->setText(QStringLiteral("%1 / %2")
                                      .arg(config.playingLevelNumber(index))
                                      .arg(config.playingLevelCount()));
        }
    }

    timeToBreak_.widget->setVisible(board.timeToBreak);
    if (board.timeToBreak) {
        if (onBreak) {
            timeToBreak_.value->setText(QStringLiteral("NOW"));
        } else {
            const int untilBreak = clock_.secondsUntilNextBreak();
            timeToBreak_.value->setText(untilBreak >= 0 ? clockText(untilBreak) : QStringLiteral("—"));
        }
    }

    breakLength_.widget->setVisible(board.breakLength);
    if (board.breakLength) {
        const int minutes = clock_.nextBreakMinutes();
        if (minutes < 0) breakLength_.value->setText(QStringLiteral("—"));
        else if (minutes == 1) breakLength_.value->setText(QStringLiteral("1 MIN"));
        else breakLength_.value->setText(QStringLiteral("%1 MIN").arg(minutes));
    }

    const auto stats = ctx_.reports().tournamentStats();
    const int totalEntries = stats.buyIns + stats.rebuys;
    const int remaining = stats.remainingPlayers;

    avgStack_.widget->setVisible(board.avgStack);
    if (board.avgStack) {
        if (remaining > 0 && config.startingStack > 0) {
            const long long chips = static_cast<long long>(config.startingStack) * totalEntries;
            avgStack_.value->setText(grouped(static_cast<int>(chips / remaining)));
        } else {
            avgStack_.value->setText(QStringLiteral("—"));
        }
    }

    players_.widget->setVisible(board.players);
    if (board.players) players_.value->setText(grouped(stats.uniquePlayers));
    reentries_.widget->setVisible(board.reentries);
    if (board.reentries) reentries_.value->setText(grouped(stats.rebuys));
    total_.widget->setVisible(board.total);
    if (board.total) total_.value->setText(grouped(totalEntries));
    remaining_.widget->setVisible(board.remaining);
    if (board.remaining) remaining_.value->setText(grouped(remaining));

    const bool showRight = board.level || board.timeToBreak || board.breakLength || board.avgStack ||
                           board.players || board.reentries || board.total || board.remaining;
    rightCol_->setVisible(showRight);
    rightDivider_->setVisible(showRight);

    applyFonts();
}

QFont TournamentBoardWindow::boardFont(int pixelSize, bool bold) const {
    return uiFont(pixelSize, bold);
}

} // namespace pokersoc::ui::qt
