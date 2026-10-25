#include "ui/board/TournamentBoardWindow.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
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
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <vector>

#include "application/ReportService.h"
#include "domain/session/TournamentConfig.h"
#include "domain/session/TournamentMode.h"
#include "ui/SessionContext.h"
#include "ui/Style.h"
#include "ui/board/TournamentClock.h"
#include "ui/Widgets.h"

namespace pokersoc::ui {

using domain::BlindLevel;
using domain::BoardChip;
using domain::Money;
using domain::PrizePayout;
using domain::TournamentConfig;

namespace {

constexpr const char* kNavy = "#0A1628";
constexpr const char* kNavyPanel = "#0E1C2E";
constexpr const char* kNavyBorder = "#060E18";
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
    return QStringLiteral("%1/%2")
        .arg(QString::fromStdString(level.smallBlind), QString::fromStdString(level.bigBlind));
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

        // Diagonal watermark, centred enough that a large mark stays on-screen.
        const QPointF pivot(width() * 0.66, height() * 0.595);
        p.translate(pivot);
        p.rotate(32.0);

        const int targetW = qMax(220, qRound(width() * 0.62));
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
    root->setContentsMargins(36, 16, 36, 10);
    root->setSpacing(0);

    auto* header = new QWidget;
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(16);
    auto* leftPad = new QWidget;
    leftPad->setFixedWidth(180);
    wallClock_ = boardLabel(QString(), 20, true, kWhite, Qt::AlignRight | Qt::AlignVCenter);
    wallClock_->setWordWrap(false);
    wallClock_->setFixedWidth(180);
    name_ = boardLabel(QString(), 32, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
    headerRow->addWidget(leftPad, 0, Qt::AlignVCenter);
    headerRow->addWidget(name_, 1);
    headerRow->addWidget(wallClock_, 0, Qt::AlignVCenter);
    root->addWidget(header);

    body_ = new QWidget;
    auto* columns = new QHBoxLayout(body_);
    columns->setContentsMargins(0, 16, 0, 4);
    columns->setSpacing(0);

    leftCol_ = new QWidget;
    leftCol_->setMinimumWidth(0);
    leftCol_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* leftLayout = new QVBoxLayout(leftCol_);
    leftLayout->setContentsMargins(4, 4, 16, 0);
    leftLayout->setSpacing(4);

    // Upper zone: rego (centred) or prizes (scroll). Chips sit below.
    auto* upper = new QWidget;
    auto* upperLayout = new QVBoxLayout(upper);
    upperLayout->setContentsMargins(0, 0, 0, 0);
    upperLayout->setSpacing(8);
    leftTopStretch_ = new QWidget;
    leftBottomStretch_ = new QWidget;
    leftTopStretch_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    leftBottomStretch_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    rego_ = makeStat(QStringLiteral("REGISTRATION ENDS"), Qt::AlignHCenter | Qt::AlignVCenter);
    rego_.widget->setMinimumWidth(0);
    rego_.caption->setMinimumWidth(0);
    rego_.value->setMinimumWidth(0);
    rego_.caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rego_.value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    upperLayout->addWidget(leftTopStretch_, 1);
    upperLayout->addWidget(rego_.widget, 0);
    upperLayout->addWidget(leftBottomStretch_, 1);

    prizeTitle_ = boardLabel(QStringLiteral("PRIZES"), 14, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
    prizeTitle_->setWordWrap(false);
    prizeTitle_->hide();
    upperLayout->addWidget(prizeTitle_);

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
    upperLayout->addWidget(prizeScroll_, 1);
    leftLayout->addWidget(upper, 1);

    chipBlock_ = new QWidget;
    chipBlock_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    chipGrid_ = new QGridLayout(chipBlock_);
    chipGrid_->setContentsMargins(0, 8, 0, 0);
    chipGrid_->setHorizontalSpacing(10);
    chipGrid_->setVerticalSpacing(6);
    for (int i = 0; i < 6; ++i) {
        ChipCell& cell = chipCells_[i];
        cell.widget = new QWidget;
        auto* cellLayout = new QVBoxLayout(cell.widget);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(4);
        cell.image = new QLabel;
        cell.image->setAlignment(Qt::AlignCenter);
        cell.image->setScaledContents(false);
        cell.value = boardLabel(QString(), 18, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
        cell.value->setWordWrap(false);
        cellLayout->addWidget(cell.image, 0, Qt::AlignHCenter);
        cellLayout->addWidget(cell.value, 0, Qt::AlignHCenter);
        cell.widget->hide();
        const int row = i / 2;
        const int col = i % 2;
        chipGrid_->addWidget(cell.widget, row, col);
    }
    chipBlock_->hide();
    leftLayout->addWidget(chipBlock_, 0);

    centreCol_ = new QWidget;
    centreCol_->setMinimumWidth(0);
    centreCol_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* centreLayout = new QVBoxLayout(centreCol_);
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

    constexpr int kBoxBorder = 6;
    constexpr int kBoxPad = 26;
    blindsBox_ = makePanel(kNavyPanel, kNavyBorder, kBoxBorder, 22, kBoxPad);
    auto* blindsLayout = panelLayout(blindsBox_);
    blindsLayout->setSpacing(10);
    blindsRow_ = makeBoardRow(QStringLiteral("BLINDS:"));
    anteRow_ = makeBoardRow(QStringLiteral("BIG BLIND ANTE:"));
    blindsLayout->addWidget(blindsRow_.widget);
    blindsLayout->addWidget(anteRow_.widget);
    anteRow_.widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    blindsBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    nextBox_ = makePanel(kNavyPanel, kNavyBorder, kBoxBorder, 22, kBoxPad);
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
    rightCol_->setMinimumWidth(0);
    rightCol_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* rightLayout = new QVBoxLayout(rightCol_);
    rightLayout->setContentsMargins(16, 4, 4, 0);
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
    columns->addWidget(leftCol_, 0);
    columns->addWidget(leftDivider_, 0);
    columns->addWidget(centreCol_, 0);
    columns->addWidget(rightDivider_, 0);
    columns->addWidget(rightCol_, 0);
    root->addWidget(body_, 1);

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
    prizeScrollTimer_->setInterval(55);
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
    layoutColumns();
    applyFonts();
    if (prizesShowing_) {
        measurePrizeLoop();
        ensurePrizeScrollRunning();
    }
}

void TournamentBoardWindow::layoutLogo() {
    if (!logo_ || !centralWidget()) return;
    logo_->setGeometry(centralWidget()->rect());
    logo_->raise();
    logo_->update();
}

void TournamentBoardWindow::layoutColumns() {
    if (!body_ || !centreCol_) return;
    const int w = body_->width();
    if (w <= 2) return;

    const bool left = leftCol_->isVisibleTo(body_);
    const bool right = rightCol_->isVisibleTo(body_);
    int units = 9;
    if (left) units += 5;
    if (right) units += 5;
    int usable = w;
    if (left) usable -= leftDivider_->width();
    if (right) usable -= rightDivider_->width();
    usable = qMax(0, usable);

    const int side = usable * 5 / units;
    int centre = usable;
    if (left) {
        leftCol_->setFixedWidth(side);
        centre -= side;
    }
    if (right) {
        rightCol_->setFixedWidth(side);
        centre -= side;
    }
    centreCol_->setFixedWidth(qMax(0, centre));
}

int TournamentBoardWindow::chipImageSize() const {
    const int h = qMax(1, height());
    // Cap so three chip rows (image + value + spacing) always stay on-screen.
    const int byHeight = qBound(52, (h - 160) / 8, 110);
    return byHeight;
}

void TournamentBoardWindow::applyFonts() {
    const int h = qMax(1, height());
    const int scale = qMin(h, qMax(1, width()));
    name_->setFont(boardFont(qBound(32, scale / 18, 68), true));
    wallClock_->setFont(boardFont(qBound(20, scale / 28, 40), true));
    // Main countdown — a touch smaller so blinds boxes keep breathing room.
    time_->setFont(boardFont(qBound(140, 6 * h / 22, 280), true));
    const int labelPx = qBound(22, h / 20, 42);
    const int valuePx = qBound(30, h / 15, 54);
    const int nextLabelPx = qBound(18, h / 24, 34);
    const int nextValuePx = qBound(26, h / 19, 44);
    blindsRow_.label->setFont(boardFont(labelPx, true));
    anteRow_.label->setFont(boardFont(labelPx, true));
    blindsRow_.value->setFont(boardFont(valuePx, true));
    anteRow_.value->setFont(boardFont(valuePx, true));
    nextRow_.label->setFont(boardFont(nextLabelPx, true));
    nextRow_.value->setFont(boardFont(nextValuePx, true));
    const int blindsRowH = qMax(valuePx, labelPx) + 12;
    const int nextRowH = qMax(nextValuePx, nextLabelPx) + 12;
    anteRow_.widget->setFixedHeight(blindsRowH);
    blindsRow_.widget->setFixedHeight(blindsRowH);
    nextRow_.widget->setFixedHeight(nextRowH);
    // Keep room for matching borders + padding on both boxes.
    constexpr int kBoxChrome = 26 * 2 + 6 * 2;
    blindsBox_->setFixedHeight(blindsRowH * 2 + 10 + kBoxChrome);
    nextBox_->setFixedHeight(nextRowH + kBoxChrome);
    prizeTitle_->setFont(boardFont(qBound(22, h / 24, 40), true));
    rego_.caption->setFont(boardFont(qBound(20, h / 26, 34), true));
    rego_.value->setFont(boardFont(qBound(60, h / 8, 120), true));
    const int capPx = qBound(14, h / 38, 22);
    const int valPx = qBound(26, h / 18, 46);
    for (SideStat* stat : {&level_, &timeToBreak_, &breakLength_, &avgStack_, &players_,
                           &reentries_, &total_, &remaining_}) {
        stat->caption->setFont(boardFont(capPx, true));
        stat->value->setFont(boardFont(valPx, true));
    }
    const int prizeCap = qBound(28, h / 18, 50);
    const int prizeVal = qBound(36, h / 13, 70);
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
            } else if (auto* alone = qobject_cast<QLabel*>(widget)) {
                alone->setFont(boardFont(prizeVal, true));
            }
        } else if (auto* alone = qobject_cast<QLabel*>(widget)) {
            alone->setFont(boardFont(prizeVal, true));
        }
    }

    const int chipPx = qBound(20, h / 22, 36);
    const int chipImg = chipImageSize();
    for (ChipCell& cell : chipCells_) {
        cell.value->setFont(boardFont(chipPx, true));
        const QPixmap src = cell.image->property("sourcePixmap").value<QPixmap>();
        if (!src.isNull()) {
            cell.image->setPixmap(
                src.scaled(chipImg, chipImg, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            cell.image->setFixedSize(chipImg, chipImg);
        } else if (cell.widget->isVisible()) {
            cell.image->setFixedSize(chipImg, chipImg);
        }
    }
}

void TournamentBoardWindow::refreshWallClock() { wallClock_->setText(wallClockText()); }

void TournamentBoardWindow::refreshChips() {
    const TournamentConfig& config = ctx_.tournament().config();
    std::vector<BoardChip> chips = config.boardChips;
    std::sort(chips.begin(), chips.end(), [](const BoardChip& a, const BoardChip& b) {
        return domain::parseChipAmount(a.value) < domain::parseChipAmount(b.value);
    });
    if (chips.size() > 6) chips.resize(6);

    QString fingerprint;
    for (const BoardChip& chip : chips) {
        fingerprint += QStringLiteral("%1:%2;")
                           .arg(QString::fromStdString(chip.imageFile))
                           .arg(QString::fromStdString(chip.value));
    }
    if (fingerprint == chipsFingerprint_ && chipBlock_->isVisible() == !chips.empty()) {
        // Still resize images with applyFonts.
        return;
    }
    chipsFingerprint_ = fingerprint;

    for (ChipCell& cell : chipCells_) {
        cell.widget->hide();
        cell.image->clear();
        cell.image->setProperty("sourcePixmap", QVariant());
        cell.value->clear();
    }

    if (chips.empty()) {
        chipBlock_->hide();
        return;
    }

    const int count = static_cast<int>(chips.size());
    const int rows = (count + 1) / 2;
    const int startRow = 3 - rows; // anchor used rows to the bottom of the 3×2 grid
    const int chipImg = chipImageSize();

    for (int i = 0; i < count; ++i) {
        const BoardChip& chip = chips[static_cast<std::size_t>(i)];
        const int gridRow = startRow + i / 2;
        const int gridCol = i % 2;
        ChipCell& cell = chipCells_[gridRow * 2 + gridCol];

        const QString imageRef = QString::fromStdString(chip.imageFile);
        QPixmap pix;
        if (!imageRef.isEmpty()) {
            const QFileInfo info(imageRef);
            if (info.isAbsolute() || imageRef.contains(QLatin1Char('/')) ||
                imageRef.contains(QLatin1Char('\\'))) {
                pix = QPixmap(imageRef);
            } else {
                pix = QPixmap(assetPath(QStringLiteral("Chips/") + imageRef));
            }
        }
        cell.image->setProperty("sourcePixmap", QVariant::fromValue(pix));
        if (!pix.isNull()) {
            cell.image->setPixmap(
                pix.scaled(chipImg, chipImg, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            cell.image->setFixedSize(chipImg, chipImg);
        } else {
            cell.image->setFixedSize(chipImg, chipImg);
        }
        const QString label = QString::fromStdString(chip.value).trimmed();
        cell.value->setText(label);
        cell.widget->show();
    }
    chipBlock_->show();
}

void TournamentBoardWindow::refreshPrizes(bool regoClosed) {
    prizeTitle_->setVisible(regoClosed);
    prizeScroll_->setVisible(regoClosed);
    leftTopStretch_->setVisible(!regoClosed);
    leftBottomStretch_->setVisible(!regoClosed);
    rego_.widget->setVisible(!regoClosed);

    if (!regoClosed) {
        prizesShowing_ = false;
        prizesFingerprint_.clear();
        prizeLoopHeight_ = 0;
        prizeLoopCount_ = 0;
        prizeScrollTimer_->stop();
        return;
    }

    const TournamentConfig& config = ctx_.tournament().config();
    const auto stats = ctx_.reports().tournamentStats();
    const auto payouts = config.prizePayouts(stats.prizePool, stats.buyIns, stats.rebuys);
    QString fingerprint;
    std::vector<PrizePayout> unclaimed;
    for (const PrizePayout& prize : payouts) {
        if (stats.placePaid(prize.place)) continue;
        unclaimed.push_back(prize);
        fingerprint += QStringLiteral("%1:%2;").arg(prize.place).arg(prize.amount.cents());
    }
    if (prizesShowing_ && fingerprint == prizesFingerprint_) {
        // Same prize list — do not rebuild (that resets scroll). Just resume if needed.
        if (prizeLoopCount_ > 0 && prizeLoopHeight_ > 0 && !prizeScrollTimer_->isActive()) {
            if (QScrollBar* bar = prizeScroll_->verticalScrollBar(); bar && bar->maximum() > 0) {
                prizeScrollTimer_->start();
            }
        }
        return;
    }
    prizesShowing_ = true;
    prizesFingerprint_ = fingerprint;
    prizeLoopHeight_ = 0;
    prizeLoopCount_ = 0;

    while (QLayoutItem* item = prizeLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }

    if (unclaimed.empty()) {
        auto* empty = boardLabel(payouts.empty() ? QStringLiteral("NO PRIZES SET")
                                                : QStringLiteral("ALL PRIZES PAID"),
                                 16, true, kWhite, Qt::AlignHCenter | Qt::AlignVCenter);
        prizeLayout_->addWidget(empty);
        prizeLayout_->addStretch(1);
        prizeScrollTimer_->stop();
        return;
    }

    // One of each place. A second copy is only added later if the list overflows
    // the viewport (seamless marquee) — otherwise duplicates would show on screen.
    for (const PrizePayout& prize : unclaimed) {
        addPrizeMarqueeRow(placeOrdinal(prize.place), money(prize.amount).toUpper());
    }
    prizeLoopCount_ = static_cast<int>(unclaimed.size());

    prizeScroll_->verticalScrollBar()->setValue(0);
    // Layout/fonts need a frame before height/scrollbar max are valid.
    QTimer::singleShot(0, this, [this] {
        applyFonts();
        ensurePrizeScrollRunning();
    });
    QTimer::singleShot(100, this, [this] { ensurePrizeScrollRunning(); });
}

void TournamentBoardWindow::addPrizeMarqueeRow(const QString& placeText,
                                               const QString& amountText) {
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);
    auto* place = boardLabel(placeText, 14, true, kWhite, Qt::AlignLeft | Qt::AlignVCenter);
    auto* amount = boardLabel(amountText, 18, true, kWhite, Qt::AlignRight | Qt::AlignVCenter);
    place->setWordWrap(false);
    amount->setWordWrap(false);
    rowLayout->addWidget(place, 0);
    rowLayout->addWidget(amount, 1);
    prizeLayout_->addWidget(row);
}

void TournamentBoardWindow::measurePrizeLoop() {
    prizeLoopHeight_ = 0;
    if (prizeLoopCount_ <= 0) return;

    // Prefer geometry of the second copy when present.
    if (prizeLoopCount_ < prizeLayout_->count()) {
        if (QLayoutItem* item = prizeLayout_->itemAt(prizeLoopCount_)) {
            if (QWidget* second = item->widget()) {
                if (second->y() > 0) prizeLoopHeight_ = second->y();
            }
        }
        if (prizeLoopHeight_ > 0) return;
    }

    // Sum size of the first (unique) copy.
    int total = 0;
    for (int i = 0; i < prizeLoopCount_ && i < prizeLayout_->count(); ++i) {
        QLayoutItem* item = prizeLayout_->itemAt(i);
        QWidget* widget = item ? item->widget() : nullptr;
        if (!widget) continue;
        const int h = widget->height() > 0 ? widget->height() : widget->sizeHint().height();
        total += qMax(1, h);
        if (i + 1 < prizeLoopCount_) total += prizeLayout_->spacing();
    }
    prizeLoopHeight_ = total;
}

void TournamentBoardWindow::ensurePrizeScrollRunning() {
    if (!prizeScroll_->isVisible() || prizeLoopCount_ <= 0) {
        prizeScrollTimer_->stop();
        return;
    }

    QScrollBar* bar = prizeScroll_->verticalScrollBar();
    const int savedScroll = bar ? bar->value() : 0;
    const bool hadDuplicate = prizeLayout_->count() >= prizeLoopCount_ * 2;

    // Measure the unique list height without destroying an existing marquee copy.
    int singleH = 0;
    for (int i = 0; i < prizeLoopCount_ && i < prizeLayout_->count(); ++i) {
        QLayoutItem* item = prizeLayout_->itemAt(i);
        QWidget* widget = item ? item->widget() : nullptr;
        if (!widget) continue;
        const int h = widget->height() > 0 ? widget->height() : widget->sizeHint().height();
        singleH += qMax(1, h);
        if (i + 1 < prizeLoopCount_) singleH += prizeLayout_->spacing();
    }
    if (hadDuplicate && prizeLoopHeight_ > 0) singleH = prizeLoopHeight_;

    const int viewH = prizeScroll_->viewport()->height();
    if (!bar || viewH <= 0 || singleH <= viewH) {
        // Fits on screen — drop any marquee duplicate and stop.
        while (prizeLayout_->count() > prizeLoopCount_) {
            QLayoutItem* item = prizeLayout_->takeAt(prizeLayout_->count() - 1);
            if (QWidget* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        prizeLoopHeight_ = 0;
        if (bar) bar->setValue(0);
        prizeScrollTimer_->stop();
        return;
    }

    if (!hadDuplicate) {
        struct RowText {
            QString place;
            QString amount;
        };
        std::vector<RowText> rows;
        rows.reserve(static_cast<std::size_t>(prizeLoopCount_));
        for (int i = 0; i < prizeLoopCount_; ++i) {
            QLayoutItem* item = prizeLayout_->itemAt(i);
            QWidget* row = item ? item->widget() : nullptr;
            auto* lay = row ? qobject_cast<QHBoxLayout*>(row->layout()) : nullptr;
            if (!lay || lay->count() < 2) continue;
            auto* place = qobject_cast<QLabel*>(lay->itemAt(0)->widget());
            auto* amount = qobject_cast<QLabel*>(lay->itemAt(1)->widget());
            if (!place || !amount) continue;
            rows.push_back({place->text(), amount->text()});
        }
        for (const RowText& row : rows) addPrizeMarqueeRow(row.place, row.amount);
        prizeList_->updateGeometry();
        if (prizeScroll_->widget()) prizeScroll_->widget()->adjustSize();
    }

    measurePrizeLoop();
    if (prizeLoopHeight_ <= 0) prizeLoopHeight_ = qMax(1, singleH);

    // Restore scroll after any layout change (mod loop height for seamless wrap).
    if (prizeLoopHeight_ > 0) {
        bar->setValue(savedScroll % prizeLoopHeight_);
    } else {
        bar->setValue(savedScroll);
    }

    if (bar->maximum() <= 0) {
        prizeScrollTimer_->stop();
        return;
    }
    if (!prizeScrollTimer_->isActive()) prizeScrollTimer_->start();
}

void TournamentBoardWindow::tickPrizeScroll() {
    QScrollBar* bar = prizeScroll_->verticalScrollBar();
    if (!bar || !prizeScroll_->isVisible()) {
        prizeScrollTimer_->stop();
        return;
    }
    if (bar->maximum() <= 0) {
        // Viewport may have grown; keep the timer alive and retry next tick.
        return;
    }
    if (prizeLoopHeight_ <= 0) measurePrizeLoop();
    if (prizeLoopHeight_ <= 0) prizeLoopHeight_ = qMax(1, (bar->maximum() + 1) / 2);

    int next = bar->value() + 1;
    // Seamless wrap: content repeats, so subtracting one loop height is invisible.
    if (next >= prizeLoopHeight_) next -= prizeLoopHeight_;
    bar->setValue(next);
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

    const auto setAnte = [this](const BlindLevel* level) {
        if (level && level->hasAnte()) {
            anteRow_.label->setText(QStringLiteral("BIG BLIND ANTE:"));
            anteRow_.value->setText(QString::fromStdString(level->ante));
        } else {
            anteRow_.label->setText(QString());
            anteRow_.value->setText(QString());
        }
    };

    if (count <= 0) {
        time_->setText(QStringLiteral("—"));
        time_->setStyleSheet(QStringLiteral("color:%1;").arg(kWhite));
        blindsRow_.value->setText(QStringLiteral("—"));
        setAnte(nullptr);
        nextRow_.value->setText(QStringLiteral("—"));
    } else {
        time_->setText(clock_.remainingText());
        // White while running/ended; yellow only when paused.
        const QString timeColour = (!ended && !clock_.running()) ? kWarn : kWhite;
        time_->setStyleSheet(QStringLiteral("color:%1;").arg(timeColour));

        const BlindLevel* blinds = clock_.activeBlinds();
        if (onBreak) {
            blindsRow_.value->setText(QStringLiteral("BREAK"));
            setAnte(blinds);
        } else if (blinds) {
            blindsRow_.value->setText(slashBlinds(*blinds));
            setAnte(blinds);
        } else {
            blindsRow_.value->setText(QStringLiteral("—"));
            setAnte(nullptr);
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
            if (nextPlaying->hasAnte()) {
                next += QStringLiteral(" (%1)").arg(QString::fromStdString(nextPlaying->ante));
            }
            nextRow_.value->setText(next);
        } else {
            nextRow_.value->setText(QStringLiteral("FINAL LEVEL"));
        }
    }

    refreshChips();
    const bool hasChips = !config.boardChips.empty();
    const bool showLeft = board.registrationEnd || hasChips;
    leftCol_->setVisible(showLeft);
    leftDivider_->setVisible(showLeft);
    layoutColumns();
    bool regoClosed = false;
    if (showLeft && board.registrationEnd) {
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
    } else if (showLeft) {
        // Chips only — hide rego / prizes.
        refreshPrizes(false);
        rego_.widget->hide();
        leftTopStretch_->hide();
        leftBottomStretch_->hide();
        prizeTitle_->hide();
        prizeScroll_->hide();
        prizeScrollTimer_->stop();
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

    layoutColumns();
    applyFonts();
}

QFont TournamentBoardWindow::boardFont(int pixelSize, bool bold) const {
    static const QString family = [] {
        QString loaded;
        const QStringList files = {
            QStringLiteral("Fonts/Campton/CamptonBook.otf"),
            QStringLiteral("Fonts/Campton/CamptonMedium.otf"),
            QStringLiteral("Fonts/Campton/CamptonSemiBold.otf"),
            QStringLiteral("Fonts/Campton/CamptonBold.otf"),
            QStringLiteral("Fonts/Campton/CamptonExtraBold.otf"),
        };
        for (const QString& relative : files) {
            QFile file(assetPath(relative));
            if (!file.open(QIODevice::ReadOnly)) continue;
            const int id = QFontDatabase::addApplicationFontFromData(file.readAll());
            if (id < 0) continue;
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty() && loaded.isEmpty()) loaded = families.first();
        }
        return loaded.isEmpty() ? QStringLiteral("Segoe UI") : loaded;
    }();

    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setWeight(bold ? QFont::Bold : QFont::Normal);
    return font;
}

} // namespace pokersoc::ui
