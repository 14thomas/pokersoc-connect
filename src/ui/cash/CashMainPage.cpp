#include "ui/cash/CashMainPage.h"
#include "ui/PlayerSwipe.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QSet>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#include "application/AdminService.h"
#include "application/BuyInService.h"
#include "application/Ledger.h"
#include "application/PlayerService.h"
#include "application/ReportService.h"
#include "domain/player/Player.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/FoodSale.h"
#include "domain/transaction/Transaction.h"
#include "domain/transaction/TransactionLog.h"
#include "ui/SessionContext.h"
#include "ui/Style.h"
#include "ui/Widgets.h"
#include "util/Time.h"

namespace pokersoc::ui {

TapFrame::TapFrame(QWidget* parent) : QFrame(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_StyledBackground);
}

void TapFrame::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = true;
        setProperty("pressed", true);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
    QFrame::mousePressEvent(event);
}

void TapFrame::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_ && event->button() == Qt::LeftButton) {
        pressed_ = false;
        setProperty("pressed", false);
        style()->unpolish(this);
        style()->polish(this);
        update();
        if (rect().contains(event->position().toPoint())) emit clicked();
    }
    QFrame::mouseReleaseEvent(event);
}

using namespace domain;

namespace {

constexpr const char* kBg = "#0D1013";
constexpr const char* kPanel = "#151A1F";
constexpr const char* kRaised = "#1D232A";
constexpr const char* kBorder = "#232A32";
constexpr const char* kText = "#EEF2F5";
constexpr const char* kMuted = "#A3AEB9";
constexpr const char* kFaint = "#7D8894";
constexpr const char* kCashIn = "#9FA2E5";
constexpr const char* kCashOut = "#E5E29F";

QString loadFace(const char* path) {
    QFile file(assetPath(QString::fromUtf8(path)));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const int id = QFontDatabase::addApplicationFontFromData(file.readAll());
    if (id < 0) return {};
    const QStringList names = QFontDatabase::applicationFontFamilies(id);
    return names.isEmpty() ? QString() : names.first();
}

struct BarlowFonts {
    QString regular;
    QString medium;
    QString semibold;
    QString bold;
    bool oneFamily = false;
};

const BarlowFonts& barlowFonts() {
    static const BarlowFonts fonts = [] {
        BarlowFonts loaded;
        loaded.regular = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Regular.ttf");
        loaded.medium = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Medium.ttf");
        loaded.semibold = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-SemiBold.ttf");
        loaded.bold = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Bold.ttf");
        loaded.oneFamily = !loaded.regular.isEmpty() && loaded.regular == loaded.medium &&
                           loaded.medium == loaded.semibold && loaded.semibold == loaded.bold;
        return loaded;
    }();
    return fonts;
}

QFont barlow(int px, int weight) {
    const BarlowFonts& fonts = barlowFonts();
    QString family = fonts.regular;
    int applied = weight;
    if (!fonts.oneFamily) {
        applied = QFont::Normal;
        if (weight >= QFont::Bold && !fonts.bold.isEmpty()) {
            family = fonts.bold;
            if (fonts.bold == fonts.regular) applied = QFont::Bold;
        } else if (weight >= QFont::DemiBold && !fonts.semibold.isEmpty()) {
            family = fonts.semibold;
        } else if (weight >= QFont::Medium && !fonts.medium.isEmpty()) {
            family = fonts.medium;
        }
    }
    if (family.isEmpty()) family = QStringLiteral("Arial Narrow");
    QFont font(family);
    font.setPixelSize(px);
    font.setWeight(static_cast<QFont::Weight>(applied));
    return font;
}

QFont ui(int px, int weight) { return barlow(px, weight); }
QFont mono(int px, int weight) { return barlow(px, weight); }

QPixmap brandIcon(int px) {
    const qreal dpr = qApp->devicePixelRatio();
    const QString key = QStringLiteral("brand-white-%1-%2").arg(px).arg(dpr);
    static QHash<QString, QPixmap> cache;
    if (const auto it = cache.constFind(key); it != cache.cend()) return it.value();
    const QPixmap source(assetPath(QStringLiteral("Brand/pokersoc_white_icon.png")));
    QPixmap scaled = source.scaled(qRound(px * dpr), qRound(px * dpr), Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    cache.insert(key, scaled);
    return scaled;
}

QPixmap iconPx(const QString& name, int px, const QColor& color, bool filled = false) {
    const qreal dpr = qApp->devicePixelRatio();
    const QString key = name + QString::number(px) + color.name() + QString::number(filled) +
                        QString::number(dpr);
    static QHash<QString, QPixmap> cache;
    if (const auto it = cache.constFind(key); it != cache.cend()) return it.value();

    QFile file(assetPath(QStringLiteral("Icons/%1.svg").arg(name)));
    QPixmap pixmap(qRound(px * dpr), qRound(px * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    if (file.open(QIODevice::ReadOnly)) {
        QString svg = QString::fromUtf8(file.readAll());
        svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
        if (filled) svg.replace(QStringLiteral("fill=\"none\""), QStringLiteral("fill=\"%1\"").arg(color.name()));
        QSvgRenderer renderer(svg.toUtf8());
        QPainter painter(&pixmap);
        renderer.render(&painter, QRectF(0, 0, px, px));
    }
    cache.insert(key, pixmap);
    return pixmap;
}

QLabel* iconLabel(const QString& name, int px, const QColor& color, bool filled = false) {
    auto* label = new QLabel;
    label->setPixmap(iconPx(name, px, color, filled));
    label->setFixedSize(px, px);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}

QLabel* txt(const QString& text, const QFont& font, const QString& color,
            Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
    auto* label = new QLabel(text);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(color));
    label->setAlignment(align);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    return label;
}

class ElideLabel : public QLabel {
public:
    explicit ElideLabel(const QString& text, const QFont& font, const QString& color, QWidget* parent = nullptr)
        : QLabel(parent), full_(text), color_(color) {
        setFont(font);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(color_);
        const QString shown = fontMetrics().elidedText(full_, Qt::ElideRight, qMax(0, width()));
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, shown);
    }

private:
    QString full_;
    QColor color_;
};

/// One field value. Shrinks only this label's type size until the full string
/// fits the column. Other fields keep their own size.
class FitLabel : public QLabel {
public:
    explicit FitLabel(const QString& text, const QFont& font, const QString& color, QWidget* parent = nullptr)
        : QLabel(parent), full_(text), base_(font), color_(color) {
        setFont(font);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(color_);
        QFont font = base_;
        const int floor = 11;
        int px = base_.pixelSize() > 0 ? base_.pixelSize() : floor;
        const int available = qMax(0, width());
        while (px > floor) {
            font.setPixelSize(px);
            if (QFontMetrics(font).horizontalAdvance(full_) <= available) break;
            --px;
        }
        font.setPixelSize(px);
        painter.setFont(font);
        const QString shown = QFontMetrics(font).horizontalAdvance(full_) <= available
                                  ? full_
                                  : QFontMetrics(font).elidedText(full_, Qt::ElideRight, available);
        painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, shown);
    }

private:
    QString full_;
    QFont base_;
    QColor color_;
};

void polishButton(QPushButton* button) {
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFlat(true);
}

QPushButton* bareButton() {
    auto* button = new QPushButton;
    polishButton(button);
    return button;
}

void fitIconText(QPushButton* button) {
    const int icon = button->icon().isNull() ? 0 : button->iconSize().width();
    const int text = QFontMetrics(button->font()).horizontalAdvance(button->text());
    const int gap = icon > 0 && !button->text().isEmpty() ? 8 : 0;
    button->setMinimumWidth(text + icon + gap + 28);
}

void passClicks(QWidget* host) {
    const auto children = host->findChildren<QWidget*>();
    for (QWidget* child : children) child->setAttribute(Qt::WA_TransparentForMouseEvents);
}

QPushButton* iconTextButton(const QString& id, const QString& iconName, int iconSize, const QColor& iconColor,
                            const QString& text, const QFont& font, int height, const QString& rules) {
    auto* button = new QPushButton(text);
    polishButton(button);
    button->setObjectName(id);
    button->setFont(font);
    button->setIcon(QIcon(iconPx(iconName, iconSize, iconColor)));
    button->setIconSize(QSize(iconSize, iconSize));
    button->setFixedHeight(height);
    button->setStyleSheet(QStringLiteral("#%1 { %2 }").arg(id, rules));
    fitIconText(button);
    return button;
}

class Scrim : public QWidget {
public:
    explicit Scrim(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_StyledBackground);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override { event->accept(); }
    void mouseReleaseEvent(QMouseEvent* event) override { event->accept(); }
    void mouseMoveEvent(QMouseEvent* event) override { event->accept(); }
    void keyPressEvent(QKeyEvent* event) override { event->accept(); }
};

QDateTime localFromUtc(const std::string& utc) {
    QDateTime parsed = QDateTime::fromString(QString::fromStdString(utc), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    parsed.setTimeSpec(Qt::UTC);
    return parsed.toLocalTime();
}

QString clock12(const QDateTime& time) { return time.toString(QStringLiteral("h:mm AP")); }

QString sittingText(const QDateTime& from) {
    const qint64 mins = std::max<qint64>(0, from.secsTo(QDateTime::currentDateTime()) / 60);
    const qint64 hours = mins / 60;
    if (hours > 0) return QStringLiteral("%1h %2m").arg(hours).arg(mins % 60);
    return QStringLiteral("%1m").arg(mins);
}

bool realName(const Player& player) {
    const QString name = QString::fromStdString(player.name()).trimmed();
    return !name.isEmpty() && name != QLatin1String("New Player");
}

bool hasDetail(const QString& value) {
    const QString trimmed = value.trimmed();
    return !trimmed.isEmpty() && trimmed != QLatin1String("None") && trimmed != QLatin1String("N/A");
}

QString initialsOf(const QString& name) {
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) return QString();
    QString letters = words.first().left(1);
    if (words.size() > 1) letters += words.last().left(1);
    return letters.toUpper();
}

QString badgeLabel(TransactionType type) {
    switch (type) {
        case TransactionType::BuyIn: return QStringLiteral("BUY-IN");
        case TransactionType::Rebuy: return QStringLiteral("REBUY");
        case TransactionType::CashOut: return QStringLiteral("CASH-OUT");
        case TransactionType::FoodSale: return QStringLiteral("SALE");
        case TransactionType::CashTip: return QStringLiteral("TIP");
        case TransactionType::Exchange: return QStringLiteral("EXCHANGE");
        case TransactionType::LostChips: return QStringLiteral("LOST");
        case TransactionType::FloatAdd: return QStringLiteral("FLOAT");
        case TransactionType::PrizePayout: return QStringLiteral("PRIZE");
        case TransactionType::BountyPayout: return QStringLiteral("BOUNTY");
    }
    return QStringLiteral("OTHER");
}

enum class AmountTone { In, Out, Neutral };

AmountTone toneFor(TransactionType type) {
    switch (type) {
        case TransactionType::BuyIn:
        case TransactionType::Rebuy:
        case TransactionType::FoodSale: return AmountTone::In;
        case TransactionType::CashOut: return AmountTone::Out;
        default: return AmountTone::Neutral;
    }
}

bool isBuy(TransactionType type) {
    return type == TransactionType::BuyIn || type == TransactionType::Rebuy;
}

QString plainType(TransactionType type) {
    switch (type) {
        case TransactionType::BuyIn: return QStringLiteral("Buy-in");
        case TransactionType::Rebuy: return QStringLiteral("Rebuy");
        case TransactionType::CashOut: return QStringLiteral("Cash-out");
        default: return QStringLiteral("Transaction");
    }
}

class ScanCard : public QWidget {
public:
    ScanCard() {
        setFixedSize(240, 240);
        elapsed_.start();
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] { update(); });
        timer->start(16);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#2A323C"), 2));
        painter.setBrush(QColor("#1A2026"));
        painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 26, 26);

        painter.setPen(QPen(QColor("#9FA2E5"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        const int inset = 18;
        const int arm = 28;
        const QRect box = rect().adjusted(inset, inset, -inset, -inset);
        const QPoint corners[] = {box.topLeft(), box.topRight(), box.bottomLeft(), box.bottomRight()};
        const int dx[] = {1, -1, 1, -1};
        const int dy[] = {1, 1, -1, -1};
        for (int i = 0; i < 4; ++i) {
            const QPoint c = corners[i];
            painter.drawLine(c, c + QPoint(dx[i] * arm, 0));
            painter.drawLine(c, c + QPoint(0, dy[i] * arm));
        }

        const QRect card(58, 48, 124, 144);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#EEF2F5"));
        painter.drawRoundedRect(card, 12, 12);
        painter.setBrush(QColor("#111111"));
        const int pattern[] = {2, 1, 3, 1, 2, 4, 1, 2, 3, 1, 2, 1, 4, 2, 1, 3};
        constexpr int patternCount = 16;
        const int gap = 2;
        const int side = 12;
        int count = 0;
        int ink = 0;
        const int limit = card.width() - side * 2;
        while (count < 48) {
            const int width = pattern[count % patternCount];
            const int gaps = count;
            if (ink + width + gaps * gap > limit) break;
            ink += width;
            ++count;
        }
        const int content = ink + qMax(0, count - 1) * gap;
        const int pad = (card.width() - content) / 2;
        int x = card.left() + pad;
        for (int i = 0; i < count; ++i) {
            const int width = pattern[i % patternCount];
            painter.drawRect(x, card.top() + 22, width, 100);
            x += width + gap;
        }

        const double cycle = std::fmod(elapsed_.elapsed(), 3200.0) / 1600.0;
        const bool returning = cycle >= 1.0;
        double u = returning ? cycle - 1.0 : cycle;
        const double eased = u < 0.5 ? 2 * u * u : 1 - std::pow(-2 * u + 2, 2) / 2;
        const double y = (returning ? 1.0 - eased : eased) * 52.0 - 26.0;
        const int lineY = card.center().y() + static_cast<int>(std::lround(y));
        QColor glow("#9FA2E5");
        glow.setAlpha(90);
        painter.setPen(QPen(glow, 10));
        painter.drawLine(card.left() + 10, lineY, card.right() - 10, lineY);
        painter.setPen(QPen(QColor("#9FA2E5"), 3));
        painter.drawLine(card.left() + 10, lineY, card.right() - 10, lineY);
    }

private:
    QElapsedTimer elapsed_;
};

} // namespace

CashMainPage::CashMainPage(SessionContext& context, QWidget* parent)
    : QWidget(parent), ctx_(context) {
    setObjectName(QStringLiteral("cashMain"));
    setAttribute(Qt::WA_StyledBackground);
    setFont(ui(18, QFont::Normal));
    setStyleSheet(QStringLiteral(
        "QWidget#cashMain { background:%1; }"
        "QLabel { background: transparent; border: none; }"
        "QWidget#cashMain QPushButton { min-height: 0px; min-width: 0px; padding: 0px; }")
                      .arg(kBg));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);
    buildTopBar(root);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(16);
    auto* left = new QWidget;
    left->setFixedWidth(744);
    auto* leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(16);
    buildActivity(left);
    leftLay->addWidget(left->findChild<QFrame*>(QStringLiteral("activityPanel")), 1);
    buildPlayerCard(left);
    leftLay->addWidget(playerCard_, 0);

    auto* right = new QVBoxLayout;
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(16);
    buildTiles(right);
    buildActions(right);
    body->addWidget(left, 0);
    body->addLayout(right, 1);
    root->addLayout(body, 1);

    buildScanner();
    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, [this] {
        updateClock();
        if (!playerId_.empty()) updateSummary();
    });
    clockTimer_->start(1000);
    undoTimer_ = new QTimer(this);
    connect(undoTimer_, &QTimer::timeout, this, &CashMainPage::tickUndo);
    updateClock();
    refresh();
}

void CashMainPage::buildTopBar(QVBoxLayout* root) {
    auto* bar = new QFrame;
    bar->setObjectName(QStringLiteral("topBar"));
    bar->setFixedHeight(72);
    bar->setStyleSheet(QStringLiteral(
        "QFrame#topBar { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(14, 0, 14, 0);
    row->setSpacing(20);

    auto* logo = new QLabel;
    logo->setFixedSize(46, 46);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(brandIcon(46));
    logo->setObjectName(QStringLiteral("logoTile"));
    logo->setStyleSheet(QStringLiteral("#logoTile { background:transparent; border:none; }"));
    row->addWidget(logo);
    row->addWidget(txt(QStringLiteral("PokerSoc Connect"), ui(26, QFont::Bold), kText));
    row->addStretch(1);

    auto* attendanceHost = new QWidget;
    auto* attendance = new QVBoxLayout(attendanceHost);
    attendance->setSpacing(4);
    attendance->setContentsMargins(0, 0, 0, 0);
    auto* attendanceLabel = txt(QStringLiteral("ATTENDANCE"), ui(13, QFont::DemiBold), kMuted,
                                Qt::AlignRight | Qt::AlignVCenter);
    QFont attendanceFont = attendanceLabel->font();
    attendanceFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    attendanceLabel->setFont(attendanceFont);
    attendanceValue_ = txt(QStringLiteral("0"), mono(23, QFont::DemiBold), kText, Qt::AlignRight | Qt::AlignVCenter);
    attendance->addWidget(attendanceLabel, 0, Qt::AlignRight);
    attendance->addWidget(attendanceValue_, 0, Qt::AlignRight);
    row->addWidget(attendanceHost, 0, Qt::AlignVCenter);

    auto* divider = new QFrame;
    divider->setFixedSize(1, 40);
    divider->setObjectName(QStringLiteral("topDivider"));
    divider->setStyleSheet(QStringLiteral("#topDivider { background:#2A323C; border:none; }"));
    row->addWidget(divider);

    auto* settings = new TapFrame;
    settings->setObjectName(QStringLiteral("settingsButton"));
    settings->setFixedHeight(52);
    settings->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    settings->setStyleSheet(QStringLiteral(
        "#settingsButton { background:%1; border:1px solid #2A323C; border-radius:26px; }"
        "#settingsButton[pressed=\"true\"] { background:#232A32; }").arg(kRaised));
    auto* settingsRow = new QHBoxLayout(settings);
    settingsRow->setContentsMargins(16, 0, 18, 0);
    settingsRow->setSpacing(10);
    settingsRow->addWidget(iconLabel(QStringLiteral("settings"), 24, QColor("#C9D1D9")));
    clockTime_ = txt(QStringLiteral("12:59"), mono(30, QFont::DemiBold), kText, Qt::AlignVCenter | Qt::AlignLeft);
    QFont timeFont = clockTime_->font();
    timeFont.setLetterSpacing(QFont::AbsoluteSpacing, -0.6);
    clockTime_->setFont(timeFont);
    clockPeriod_ = txt(QStringLiteral("PM"), ui(18, QFont::DemiBold), kMuted, Qt::AlignVCenter | Qt::AlignLeft);
    settingsRow->addWidget(clockTime_);
    settingsRow->addWidget(clockPeriod_);
    settings->setMinimumWidth(settings->sizeHint().width());
    passClicks(settings);
    connect(settings, &TapFrame::clicked, this, &CashMainPage::openSettings);
    row->addWidget(settings);
    root->addWidget(bar, 0);
}

void CashMainPage::buildActivity(QWidget* host) {
    auto* panel = new QFrame(host);
    panel->setObjectName(QStringLiteral("activityPanel"));
    panel->setStyleSheet(QStringLiteral(
        "#activityPanel { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    auto* header = new QFrame;
    header->setObjectName(QStringLiteral("activityHeader"));
    header->setFixedHeight(76);
    header->setStyleSheet(QStringLiteral(
        "#activityHeader { background:transparent; border:none; border-bottom:1px solid #232A32; }"));
    auto* headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(24, 0, 16, 0);
    headerRow->setSpacing(12);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(txt(QStringLiteral("Activity"), ui(26, QFont::Bold), kText));
    auto* leftCount = txt(QString(), ui(16, QFont::Normal), kMuted);
    titles->addWidget(leftCount);
    headerRow->addLayout(titles);

    playerChip_ = new TapFrame;
    playerChip_->setObjectName(QStringLiteral("playerChip"));
    playerChip_->setFixedHeight(44);
    playerChip_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    playerChip_->hide();
    auto* chipRow = new QHBoxLayout(playerChip_);
    chipRow->setContentsMargins(12, 0, 10, 0);
    chipRow->setSpacing(8);
    chipRow->addWidget(iconLabel(QStringLiteral("user"), 18, QColor("#C5C7F2")));
    playerChipText_ = txt(QString(), ui(18, QFont::DemiBold), "#C5C7F2");
    chipRow->addWidget(playerChipText_);
    chipRow->addWidget(iconLabel(QStringLiteral("x"), 18, QColor("#C5C7F2")));
    playerChip_->setStyleSheet(QStringLiteral(
        "#playerChip { background:rgba(159,162,229,0.16); border:1px solid #4B4E7A; border-radius:12px; }"
        "#playerChip[pressed=\"true\"] { background:rgba(159,162,229,0.28); }"));
    connect(playerChip_, &TapFrame::clicked, this, [this] { clearPlayer(); });
    passClicks(playerChip_);
    headerRow->addWidget(playerChip_);
    headerRow->addStretch(1);

    filterRow_ = new QWidget;
    auto* filters = new QHBoxLayout(filterRow_);
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);
    const char* names[] = {"All", "Buy-ins", "Cash-outs", "Shop", "Other"};
    for (int i = 0; i < 5; ++i) {
        auto* chip = bareButton();
        chip->setObjectName(QStringLiteral("filterChip"));
        chip->setText(QString::fromUtf8(names[i]));
        chip->setFont(ui(17, QFont::DemiBold));
        chip->setFixedHeight(44);
        chip->setStyleSheet(QStringLiteral(
            "#filterChip { background:%1; color:#C9D1D9; border:1px solid #2A323C; border-radius:12px; padding:0 16px; }"
            "#filterChip[on=\"true\"] { background:#EEF2F5; color:#0D1013; border:none; }").arg(kRaised));
        fitIconText(chip);
        connect(chip, &QPushButton::clicked, this, [this, i] {
            filter_ = i;
            updateHeader();
            rebuildList();
        });
        filters->addWidget(chip);
        filterButtons_[i] = chip;
    }
    headerRow->addWidget(filterRow_);
    activityCount_ = txt(QString(), ui(16, QFont::Normal), kMuted, Qt::AlignRight | Qt::AlignVCenter);
    headerRow->addWidget(activityCount_);
    column->addWidget(header);
    // The subtitle under "Activity" is only shown when no player is loaded.
    leftCount->setObjectName(QStringLiteral("idleCount"));

    summary_ = new QFrame;
    summary_->setObjectName(QStringLiteral("summaryStrip"));
    summary_->setFixedHeight(124);
    summary_->setStyleSheet(QStringLiteral(
        "#summaryStrip { background:#12171B; border:none; border-bottom:1px solid #232A32; }"));
    auto* summaryRow = new QHBoxLayout(summary_);
    summaryRow->setContentsMargins(6, 0, 6, 0);
    summaryRow->setSpacing(0);
    const auto addStat = [&](QLabel*& value, QLabel*& sub, const QString& caption, const QString& valueColor) {
        auto* cell = new QVBoxLayout;
        cell->setSpacing(6);
        cell->setContentsMargins(18, 0, 18, 0);
        auto* cap = txt(caption, ui(13, QFont::Bold), kMuted);
        QFont capFont = cap->font();
        capFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
        cap->setFont(capFont);
        value = txt(QStringLiteral("$0.00"), mono(34, QFont::DemiBold), valueColor);
        sub = txt(QString(), ui(17, QFont::Normal), kMuted);
        cell->addStretch(1);
        cell->addWidget(cap);
        cell->addWidget(value);
        cell->addWidget(sub);
        cell->addStretch(1);
        summaryRow->addLayout(cell, 1);
    };
    addStat(boughtValue_, boughtSub_, QStringLiteral("BOUGHT IN"), kCashIn);
    auto* line1 = new QFrame;
    line1->setObjectName(QStringLiteral("summaryRule"));
    line1->setFixedWidth(1);
    line1->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    line1->setStyleSheet(QStringLiteral("#summaryRule { background:#232A32; border:none; }"));
    summaryRow->addWidget(line1);
    addStat(cashedValue_, cashedSub_, QStringLiteral("CASHED OUT"), kText);
    auto* line2 = new QFrame;
    line2->setObjectName(QStringLiteral("summaryRule"));
    line2->setFixedWidth(1);
    line2->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    line2->setStyleSheet(QStringLiteral("#summaryRule { background:#232A32; border:none; }"));
    summaryRow->addWidget(line2);
    addStat(sittingValue_, sittingSub_, QStringLiteral("SITTING"), kText);
    summary_->hide();
    column->addWidget(summary_);

    auto* scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("logScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral("#logScroll { background:transparent; border:none; }"));
    auto* list = new QWidget;
    list->setObjectName(QStringLiteral("logList"));
    list->setStyleSheet(QStringLiteral("#logList { background:%1; }").arg(kPanel));
    listLayout_ = new QVBoxLayout(list);
    listLayout_->setContentsMargins(0, 0, 0, 0);
    listLayout_->setSpacing(0);
    listLayout_->addStretch(1);
    scroll->setWidget(list);
    QScroller::grabGesture(scroll->viewport(), QScroller::LeftMouseButtonGesture);
    column->addWidget(scroll, 1);

    toastWrap_ = new QWidget;
    auto* toastOuter = new QVBoxLayout(toastWrap_);
    toastOuter->setContentsMargins(12, 0, 12, 12);
    auto* toast = new QFrame;
    toast->setObjectName(QStringLiteral("undoToast"));
    toast->setFixedHeight(68);
    toast->setStyleSheet(QStringLiteral(
        "#undoToast { background:#15241B; border:1px solid #24402D; border-radius:14px; }"));
    auto* toastRow = new QHBoxLayout(toast);
    toastRow->setContentsMargins(16, 0, 10, 0);
    toastRow->setSpacing(14);
    QPixmap badge(34, 34);
    badge.fill(Qt::transparent);
    {
        QPainter painter(&badge);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor("#4ADE80"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(0, 0, 34, 34);
        painter.drawPixmap(7, 7, iconPx(QStringLiteral("check"), 20, QColor("#111111")));
    }
    auto* badgeLabel = new QLabel;
    badgeLabel->setPixmap(badge);
    badgeLabel->setFixedSize(34, 34);
    badgeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    toastRow->addWidget(badgeLabel);
    toastText_ = txt(QString(), ui(19, QFont::Normal), "#B7C7BE");
    toastText_->setTextFormat(Qt::RichText);
    toastText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toastRow->addWidget(toastText_, 1);
    undoButton_ = iconTextButton(QStringLiteral("undoButton"), QStringLiteral("undo-2"), 18, QColor("#0D1013"),
                                 QStringLiteral("Undo  8s"), ui(18, QFont::Bold), 48,
                                 QStringLiteral("background:#EEF2F5; color:#0D1013; border-radius:12px; padding:0 16px;"));
    connect(undoButton_, &QPushButton::clicked, this, &CashMainPage::askUndoPassword);
    toastRow->addWidget(undoButton_);
    toastOuter->addWidget(toast);
    toastWrap_->hide();
    column->addWidget(toastWrap_);
}

void CashMainPage::buildPlayerCard(QWidget* host) {
    playerCard_ = new QFrame(host);
    playerCard_->setObjectName(QStringLiteral("playerCard"));
    playerCard_->setFixedHeight(340);
    auto* lay = new QVBoxLayout(playerCard_);
    lay->setContentsMargins(0, 0, 0, 0);
    playerStack_ = new QStackedWidget;
    lay->addWidget(playerStack_);

    auto* idle = new QWidget;
    auto* idleRow = new QHBoxLayout(idle);
    idleRow->setContentsMargins(30, 28, 32, 28);
    idleRow->setSpacing(36);
    idleRow->addWidget(new ScanCard);
    auto* copy = new QVBoxLayout;
    copy->setSpacing(14);
    copy->addStretch(1);
    auto* headline = txt(QStringLiteral("Scan a member card"), ui(42, QFont::Bold), kText);
    headline->setWordWrap(true);
    copy->addWidget(headline);
    auto* typeId = new TapFrame;
    typeId->setObjectName(QStringLiteral("typeIdButton"));
    typeId->setFixedHeight(60);
    typeId->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    typeId->setStyleSheet(QStringLiteral(
        "#typeIdButton { background:%1; border:1px solid #2F3843; border-radius:18px; }"
        "#typeIdButton[pressed=\"true\"] { background:#232A32; }").arg(kRaised));
    auto* typeRow = new QHBoxLayout(typeId);
    typeRow->setContentsMargins(22, 0, 22, 0);
    typeRow->setSpacing(8);
    typeRow->addWidget(iconLabel(QStringLiteral("keyboard"), 22, QColor("#DCE3EA")));
    typeRow->addWidget(txt(QStringLiteral("Type ID"), ui(20, QFont::DemiBold), kText));
    typeId->setMinimumWidth(typeId->sizeHint().width());
    passClicks(typeId);
    connect(typeId, &TapFrame::clicked, this, &CashMainPage::showKeypad);
    copy->addWidget(typeId, 0, Qt::AlignLeft);
    copy->addStretch(1);
    idleRow->addLayout(copy, 1);
    playerStack_->addWidget(idle);

    auto* known = new QWidget;
    knownLayout_ = new QVBoxLayout(known);
    knownLayout_->setContentsMargins(0, 0, 0, 0);
    playerStack_->addWidget(known);

    swipeHandle_ = new QFrame(playerCard_);
    swipeHandle_->setObjectName(QStringLiteral("swipeHandle"));
    swipeHandle_->setFixedSize(48, 5);
    swipeHandle_->setAttribute(Qt::WA_TransparentForMouseEvents);
    swipeHandle_->setStyleSheet(QStringLiteral(
        "#swipeHandle { background:#3A424C; border:none; border-radius:3px; }"));
    swipeHandle_->hide();
    swipe_ = new PlayerSwipe(
        playerCard_, [this] { return !playerId_.empty() && !overlayOpen_; }, [this] { clearPlayer(); }, this);
    swipe_->watch(playerCard_);
    placeHandle();
}

void CashMainPage::buildTiles(QVBoxLayout* column) {
    auto* grid = new QGridLayout;
    grid->setSpacing(16);
    struct Spec {
        const char* icon;
        const char* title;
        const char* sub;
        void (CashMainPage::*signal)();
    };
    const Spec specs[] = {
        {"users", "Players", "", &CashMainPage::openPlayers},
        {"coins", "Lost chips", "Record lost chips as tips", &CashMainPage::openLostChips},
        {"shopping-bag", "Shop", "", &CashMainPage::openShop},
        {"banknote", "Cashbox", "", &CashMainPage::openCashbox},
    };
    for (int i = 0; i < 4; ++i) {
        auto* tile = new TapFrame;
        tile->setObjectName(QStringLiteral("homeTile"));
        tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        tile->setStyleSheet(QStringLiteral(
            "#homeTile { background:%1; border:1px solid %2; border-radius:20px; }"
            "#homeTile[pressed=\"true\"] { background:#1D232A; }").arg(kPanel, kBorder));
        auto* lay = new QVBoxLayout(tile);
        lay->setContentsMargins(30, 30, 30, 28);
        lay->setSpacing(6);
        auto* box = new QLabel;
        box->setFixedSize(72, 72);
        box->setAlignment(Qt::AlignCenter);
        box->setPixmap(iconPx(QString::fromUtf8(specs[i].icon), 34, QColor("#DCE3EA")));
        box->setObjectName(QStringLiteral("iconBox"));
        box->setStyleSheet(QStringLiteral(
            "#iconBox { background:%1; border:1px solid #2C3540; border-radius:18px; }").arg(kRaised));
        box->setAttribute(Qt::WA_TransparentForMouseEvents);
        lay->addWidget(box, 0, Qt::AlignLeft);
        lay->addStretch(1);
        if (specs[i].sub[0] != '\0') {
            auto* sub = txt(QString::fromUtf8(specs[i].sub), ui(20, QFont::Normal), kMuted);
            sub->setWordWrap(true);
            lay->addWidget(sub);
        }
        lay->addWidget(txt(QString::fromUtf8(specs[i].title), ui(36, QFont::Bold), kText));
        connect(tile, &TapFrame::clicked, this, specs[i].signal);
        passClicks(tile);
        grid->addWidget(tile, i / 2, i % 2);
    }
    for (int i = 0; i < 2; ++i) {
        grid->setRowStretch(i, 1);
        grid->setColumnStretch(i, 1);
    }
    column->addLayout(grid, 1);
}

CashMainPage::Action CashMainPage::makeAction(const QString& iconName) {
    Action action;
    action.button = new TapFrame;
    action.button->setObjectName(iconName == QLatin1String("arrow-down-to-line") ? QStringLiteral("buyInAction")
                                                                                  : QStringLiteral("cashOutAction"));
    action.button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    action.button->setFixedHeight(340);
    auto* column = new QVBoxLayout(action.button);
    column->setContentsMargins(32, 30, 32, 30);
    column->setSpacing(0);
    auto* top = new QHBoxLayout;
    top->setSpacing(12);
    action.icon = new QLabel;
    action.icon->setObjectName(QStringLiteral("actionIcon"));
    action.icon->setFixedSize(80, 80);
    action.icon->setAlignment(Qt::AlignCenter);
    action.icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    top->addWidget(action.icon);
    top->addStretch(1);
    column->addLayout(top);
    column->addStretch(1);
    action.title = txt(QString(), ui(68, QFont::Bold), kText);
    QFont titleFont = action.title->font();
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, -0.7);
    action.title->setFont(titleFont);
    action.sub = txt(QString(), ui(21, QFont::Medium), kMuted);
    column->addWidget(action.sub);
    column->addSpacing(6);
    column->addWidget(action.title);
    action.icon->setPixmap(iconPx(iconName, 40, QColor("#14152B")));
    passClicks(action.button);
    return action;
}

void CashMainPage::buildActions(QVBoxLayout* column) {
    auto* primary = new QWidget;
    primary->setFixedHeight(340);
    auto* row = new QHBoxLayout(primary);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    buy_ = makeAction(QStringLiteral("arrow-down-to-line"));
    cash_ = makeAction(QStringLiteral("arrow-up-from-line"));
    connect(buy_.button, &TapFrame::clicked, this, [this] {
        if (playerId_.empty() || loadedMinor()) return;
        if (!ctx_.cashGame().config().hasRoom(ctx_.buyIns().history(playerId_))) return;
        ensureRecord();
        emit openBuyIn(QString::fromStdString(playerId_));
    });
    connect(cash_.button, &TapFrame::clicked, this, [this] {
        if (playerId_.empty() || loadedMinor()) return;
        ensureRecord();
        emit openCashOut(QString::fromStdString(playerId_));
    });
    row->addWidget(buy_.button, 1);
    row->addWidget(cash_.button, 1);
    column->addWidget(primary, 0);
}

void CashMainPage::buildScanner() {
    scanner_ = new QLineEdit(this);
    scanner_->setFixedSize(1, 1);
    scanner_->move(-40, -40);
    scanner_->setFocusPolicy(Qt::StrongFocus);
    connect(scanner_, &QLineEdit::returnPressed, this, [this] {
        const QString id = scanner_->text().trimmed();
        scanner_->clear();
        if (overlayOpen_ || id.isEmpty()) return;
        lookup(id);
    });
}

void CashMainPage::focusScanner() {
    if (overlayOpen_ || !scanner_) return;
    scanner_->setFocus(Qt::OtherFocusReason);
}

void CashMainPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    focusScanner();
}

void CashMainPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    placeHandle();
    if (!dim_ || !dim_->parentWidget()) return;
    dim_->setGeometry(dim_->parentWidget()->rect());
    if (idDialog_) {
        idDialog_->move((dim_->width() - idDialog_->width()) / 2, (dim_->height() - idDialog_->height()) / 2);
    }
}

void CashMainPage::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (!overlayOpen_) return;
    holdUndo_ = false;
    pendingId_.clear();
    closeOverlay();
}

void CashMainPage::refresh() {
    updateAttendance();
    updateHeader();
    updateSummary();
    rebuildList();
    rebuildPlayer();
    updateActions();
}

void CashMainPage::lookup(const QString& rawId) {
    if (overlayOpen_) return;
    const QString id = rawId.trimmed();
    if (id.isEmpty()) return;
    if (!ctx_.players().find(id.toStdString())) {
        showIdCheck(id.toStdString());
        return;
    }
    playerId_ = id.toStdString();
    refresh();
    focusScanner();
}

void CashMainPage::clearPlayer() {
    playerId_.clear();
    if (scanner_) scanner_->clear();
    // The empty card, chips and buttons update now. Rebuilding the log is the
    // slow part, so it happens after this frame instead of holding the drag.
    updateAttendance();
    updateHeader();
    updateSummary();
    rebuildPlayer();
    updateActions();
    QTimer::singleShot(0, this, [this] {
        if (!playerId_.empty()) return;
        rebuildList();
    });
    focusScanner();
}

void CashMainPage::ensureRecord() {
    if (playerId_.empty() || ctx_.players().find(playerId_)) return;
    ctx_.players().save(Player(playerId_, ""));
}

bool CashMainPage::knownPlayer() const {
    const Player* player = playerId_.empty() ? nullptr : ctx_.players().find(playerId_);
    if (!player) return false;
    if (realName(*player) || player->membership() != MembershipType::None) return true;
    return hasDetail(toQ(player->email())) || hasDetail(toQ(player->studentNumber())) ||
           hasDetail(toQ(player->degree())) || hasDetail(toQ(player->studyYear()));
}

const QString CashMainPage::displayName() const {
    const Player* player = ctx_.players().find(playerId_);
    if (player && realName(*player)) return toQ(player->name());
    return QString::fromStdString(playerId_);
}

void CashMainPage::updateAttendance() {
    std::set<std::string> buyers;
    for (const auto& transaction : ctx_.log().entries()) {
        if (isBuy(transaction->type()) && !transaction->playerId().empty()) {
            buyers.insert(transaction->playerId());
        }
    }
    attendanceValue_->setText(QString::number(static_cast<int>(buyers.size())));
}

void CashMainPage::updateClock() {
    const QTime now = QTime::currentTime();
    clockTime_->setText(now.toString(QStringLiteral("h:mm")));
    clockPeriod_->setText(now.toString(QStringLiteral("AP")));
}

void CashMainPage::updateHeader() {
    const bool loaded = !playerId_.empty();
    filterRow_->setVisible(!loaded);
    playerChip_->setVisible(loaded);
    activityCount_->setVisible(loaded);
    if (auto* idle = findChild<QLabel*>(QStringLiteral("idleCount"))) {
        idle->setVisible(!loaded);
    }
        if (loaded) {
            playerChipText_->setText(displayName());
            playerChip_->setMinimumWidth(playerChip_->sizeHint().width());
        }
    for (int i = 0; i < 5; ++i) {
        filterButtons_[i]->setProperty("on", i == filter_);
        filterButtons_[i]->style()->unpolish(filterButtons_[i]);
        filterButtons_[i]->style()->polish(filterButtons_[i]);
    }
}

void CashMainPage::updateSummary() {
    summary_->setVisible(!playerId_.empty());
    if (playerId_.empty()) return;

    int buyIns = 0;
    int rebuys = 0;
    int cashOuts = 0;
    Money bought;
    Money cashed;
    const Transaction* firstBuy = nullptr;
    const Transaction* lastBuy = nullptr;
    const Transaction* lastOut = nullptr;
    for (const auto& transaction : ctx_.log().entries()) {
        if (transaction->playerId() != playerId_) continue;
        const TransactionType type = transaction->type();
        if (type == TransactionType::BuyIn) {
            ++buyIns;
            bought += transaction->amount();
        } else if (type == TransactionType::Rebuy) {
            ++rebuys;
            bought += transaction->amount();
        } else if (type == TransactionType::CashOut) {
            ++cashOuts;
            if (const auto* cashOut = dynamic_cast<const CashOut*>(transaction.get())) {
                cashed += cashOut->chipValue();
            } else {
                cashed += transaction->amount();
            }
        } else {
            continue;
        }
        const Transaction* current = isBuy(type) ? lastBuy : lastOut;
        if (!current || transaction->time() > current->time() ||
            (transaction->time() == current->time() && transaction->id() > current->id())) {
            if (isBuy(type)) lastBuy = transaction.get();
            else lastOut = transaction.get();
        }
        if (isBuy(type) && (!firstBuy || transaction->time() < firstBuy->time() ||
                            (transaction->time() == firstBuy->time() && transaction->id() < firstBuy->id()))) {
            firstBuy = transaction.get();
        }
    }
    boughtValue_->setText(money(bought));
    const QString buyWord = buyIns == 1 ? QStringLiteral("buy-in") : QStringLiteral("buy-ins");
    const QString rebuyWord = rebuys == 1 ? QStringLiteral("rebuy") : QStringLiteral("rebuys");
    boughtSub_->setText(QStringLiteral("%1 %2, %3 %4").arg(buyIns).arg(buyWord).arg(rebuys).arg(rebuyWord));
    cashedValue_->setText(money(cashed));
    const bool stillPlaying = !lastOut || (lastBuy && lastOut->time() < lastBuy->time());
    if (stillPlaying) {
        cashedSub_->setText(QStringLiteral("Still playing"));
    } else {
        cashedSub_->setText(cashOuts == 1 ? QStringLiteral("1 cash-out")
                                          : QStringLiteral("%1 cash-outs").arg(cashOuts));
    }
    if (firstBuy) {
        const QDateTime when = localFromUtc(firstBuy->time());
        sittingValue_->setText(sittingText(when));
        sittingSub_->setText(QStringLiteral("since %1").arg(clock12(when)));
    } else {
        sittingValue_->setText(QStringLiteral("0m"));
        sittingSub_->setText(QStringLiteral("since —"));
    }
}

void CashMainPage::rebuildList() {
    std::vector<const Transaction*> rows;
    for (const auto& transaction : ctx_.log().entries()) {
        if (!playerId_.empty() && transaction->playerId() != playerId_) continue;
        if (playerId_.empty()) {
            const TransactionType type = transaction->type();
            const bool buy = isBuy(type);
            const bool out = type == TransactionType::CashOut;
            const bool shop = type == TransactionType::FoodSale;
            const bool keep = filter_ == 0 || (filter_ == 1 && buy) || (filter_ == 2 && out) ||
                              (filter_ == 3 && shop) || (filter_ == 4 && !buy && !out && !shop);
            if (!keep) continue;
        }
        rows.push_back(transaction.get());
    }
    std::sort(rows.begin(), rows.end(), [](const Transaction* a, const Transaction* b) {
        if (a->time() != b->time()) return a->time() > b->time();
        return a->id() > b->id();
    });

    if (auto* idle = findChild<QLabel*>(QStringLiteral("idleCount"))) {
        idle->setText(QStringLiteral("%1 entries").arg(rows.size()));
    }
    activityCount_->setText(QStringLiteral("%1 entries").arg(rows.size()));

    QSet<qlonglong> keep;
    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
        const Transaction* transaction = rows[static_cast<size_t>(index)];
        const qlonglong id = transaction->id();
        keep.insert(id);
        const bool scoped = !playerId_.empty();
        TapFrame* row = logRows_.value(id, nullptr);
        if (row && row->property("scoped").toBool() != scoped) {
            listLayout_->removeWidget(row);
            row->deleteLater();
            logRows_.remove(id);
            row = nullptr;
        }
        if (!row) {
            row = new TapFrame;
            row->setObjectName(QStringLiteral("logRow"));
            row->setFixedHeight(54);
            row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            row->setStyleSheet(QStringLiteral(
                "#logRow { background:transparent; border:none; border-bottom:1px solid #1E252C; border-radius:0; }"
                "#logRow[pressed=\"true\"] { background:#1D232A; }"));
            auto* line = new QHBoxLayout(row);
            line->setContentsMargins(24, 0, 24, 0);
            line->setSpacing(14);
            const QDateTime when = localFromUtc(transaction->time());
            auto* time = txt(when.toString(QStringLiteral("HH:mm")), mono(16, QFont::Normal), kMuted);
            time->setFixedWidth(64);
            line->addWidget(time);

            const AmountTone tone = toneFor(transaction->type());
            const QString badgeBg = tone == AmountTone::In    ? QStringLiteral("rgba(159,162,229,0.16)")
                                    : tone == AmountTone::Out ? QStringLiteral("rgba(229,226,159,0.14)")
                                                              : QStringLiteral("#232A32");
            const QString badgeFg = tone == AmountTone::In ? QStringLiteral("#9FA2E5")
                                    : tone == AmountTone::Out ? QStringLiteral("#E5E29F")
                                                              : QStringLiteral("#C9D1D9");
            auto* badgeHost = new QWidget;
            badgeHost->setFixedWidth(112);
            auto* badgeLay = new QHBoxLayout(badgeHost);
            badgeLay->setContentsMargins(0, 0, 0, 0);
            auto* badge = txt(badgeLabel(transaction->type()), ui(15, QFont::Bold), badgeFg, Qt::AlignCenter);
            badge->setObjectName(QStringLiteral("typeBadge"));
            QFont badgeFont = badge->font();
            badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
            badge->setFont(badgeFont);
            badge->setContentsMargins(8, 0, 8, 0);
            badge->setFixedHeight(30);
            badge->setMinimumWidth(qMin(112, QFontMetrics(badgeFont).horizontalAdvance(badge->text()) + 16));
            badge->setStyleSheet(QStringLiteral(
                "#typeBadge { color:%1; background:%2; border-radius:8px; }").arg(badgeFg, badgeBg));
            badgeLay->addWidget(badge, 0, Qt::AlignLeft | Qt::AlignVCenter);
            badgeLay->addStretch(1);
            line->addWidget(badgeHost);

            auto* middle = new QWidget;
            auto* middleLay = new QHBoxLayout(middle);
            middleLay->setContentsMargins(0, 0, 0, 0);
            middleLay->setSpacing(10);
            if (playerId_.empty()) {
                if (!transaction->playerId().empty()) {
                    const Player* player = ctx_.players().find(transaction->playerId());
                    if (player && realName(*player)) {
                        middleLay->addWidget(txt(toQ(player->name()), ui(20, QFont::Medium), kText), 0);
                        middleLay->addWidget(txt(toQ(transaction->playerId()), mono(14, QFont::Normal), kFaint), 1);
                    } else {
                        middleLay->addWidget(txt(toQ(transaction->playerId()), mono(16, QFont::Medium), kText), 1);
                    }
                } else {
                    QString description = QStringLiteral("Shop sale");
                    if (transaction->type() == TransactionType::LostChips) {
                        description = QStringLiteral("Lost chips");
                    } else if (const auto* sale = dynamic_cast<const FoodSale*>(transaction)) {
                        const QString items = toQ(describeSaleLines(sale->lines()));
                        description = items.isEmpty() ? QStringLiteral("Shop sale") : items;
                    } else if (transaction->type() == TransactionType::CashTip) {
                        description = QStringLiteral("Tip");
                    } else if (transaction->type() == TransactionType::Exchange) {
                        description = QStringLiteral("Exchange");
                    } else if (transaction->type() == TransactionType::FloatAdd) {
                        description = QStringLiteral("Float");
                    }
                    auto* desc = txt(description, ui(19, QFont::Normal), kMuted);
                    desc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                    middleLay->addWidget(desc, 1);
                }
            }
            middleLay->addStretch(1);
            line->addWidget(middle, 1);

            Money shown = transaction->amount();
            if (const auto* cashOut = dynamic_cast<const CashOut*>(transaction)) shown = cashOut->chipValue();
            QString amount = money(shown);
            QString amountColor = QStringLiteral("#C9D1D9");
            const bool plain = isBuy(transaction->type()) ||
                               transaction->type() == TransactionType::CashOut ||
                               transaction->type() == TransactionType::FoodSale;
            if (transaction->type() == TransactionType::CashOut) {
                amountColor = kCashOut;
            } else if (isBuy(transaction->type()) || tone == AmountTone::In) {
                if (!plain) amount.prepend(QLatin1Char('+'));
                amountColor = kCashIn;
            } else if (tone == AmountTone::Out) {
                amount = QStringLiteral("−") + money(std::llabs(shown.cents()));
                amountColor = kCashOut;
            }
            auto* amountLabel = txt(amount, mono(19, QFont::DemiBold), amountColor, Qt::AlignRight | Qt::AlignVCenter);
            amountLabel->setFixedWidth(132);
            line->addWidget(amountLabel);
            row->setProperty("scoped", scoped);
            passClicks(row);
            connect(row, &TapFrame::clicked, this, [this, id] { emit openEntry(id); });
            logRows_.insert(id, row);
        }
        if (listLayout_->indexOf(row) != index) {
            listLayout_->removeWidget(row);
            listLayout_->insertWidget(index, row);
        }
    }
    for (auto it = logRows_.begin(); it != logRows_.end();) {
        if (keep.contains(it.key())) {
            ++it;
            continue;
        }
        listLayout_->removeWidget(it.value());
        it.value()->deleteLater();
        it = logRows_.erase(it);
    }
}

void CashMainPage::rebuildPlayer() {
    const bool loaded = !playerId_.empty();
    const Player* player = loaded ? ctx_.players().find(playerId_) : nullptr;
    const bool minor = player && player->isUnderage();
    playerCard_->setStyleSheet(QStringLiteral(
        "QFrame#playerCard { background:%1; border:%2px solid %3; border-radius:18px; }")
                                   .arg(kPanel)
                                   .arg(loaded ? 2 : 1)
                                   .arg(!loaded ? QString::fromLatin1(kBorder)
                                                : (minor ? QStringLiteral("#F28B82") : QString::fromLatin1(kCashIn))));
    playerStack_->setCurrentIndex(loaded ? 1 : 0);
    placeHandle();
    if (!loaded) return;

    clearLayout(knownLayout_);
    const bool known = knownPlayer();
    auto* body = new QWidget;
    auto* column = new QVBoxLayout(body);
    column->setContentsMargins(26, 14, 14, 22);
    column->setSpacing(0);

    auto* header = new QHBoxLayout;
    header->setSpacing(16);
    if (!minor && known) {
        auto* avatar = txt(initialsOf(player && realName(*player) ? toQ(player->name()) : displayName()),
                           ui(30, QFont::Bold), "#D9DBF7", Qt::AlignCenter);
        avatar->setObjectName(QStringLiteral("avatar"));
        avatar->setFixedSize(76, 76);
        avatar->setStyleSheet(QStringLiteral(
            "#avatar { background:#262A40; color:#D9DBF7; border-radius:38px; }"));
        header->addWidget(avatar);
    } else if (!minor) {
        auto* avatar = new QLabel;
        avatar->setObjectName(QStringLiteral("avatar"));
        avatar->setFixedSize(76, 76);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setPixmap(iconPx(QStringLiteral("user"), 34, QColor("#7D8894")));
        avatar->setStyleSheet(QStringLiteral(
            "#avatar { background:%1; border:1px dashed #3A424C; border-radius:38px; }").arg(kRaised));
        header->addWidget(avatar);
    }

    auto* nameBlock = new QVBoxLayout;
    nameBlock->setSpacing(6);
    const bool named = player && realName(*player);
    if (minor) {
        if (named) {
            auto* avatar = txt(initialsOf(toQ(player->name())), ui(30, QFont::Bold), "#D9DBF7", Qt::AlignCenter);
            avatar->setObjectName(QStringLiteral("avatar"));
            avatar->setFixedSize(76, 76);
            avatar->setStyleSheet(QStringLiteral(
                "#avatar { background:#262A40; color:#D9DBF7; border-radius:38px; }"));
            header->addWidget(avatar);
            auto* name = new ElideLabel(toQ(player->name()), ui(40, QFont::Bold), kText);
            name->setFixedHeight(48);
            nameBlock->addWidget(name);
        } else {
            auto* avatar = new QLabel;
            avatar->setObjectName(QStringLiteral("avatar"));
            avatar->setFixedSize(76, 76);
            avatar->setAlignment(Qt::AlignCenter);
            avatar->setPixmap(iconPx(QStringLiteral("user"), 34, QColor("#7D8894")));
            avatar->setStyleSheet(QStringLiteral(
                "#avatar { background:%1; border:1px dashed #3A424C; border-radius:38px; }").arg(kRaised));
            header->addWidget(avatar);
            auto* title = new QHBoxLayout;
            title->setSpacing(8);
            title->addWidget(txt(QStringLiteral("Card"), ui(40, QFont::Bold), kText));
            title->addWidget(txt(QString::fromStdString(playerId_), mono(36, QFont::DemiBold), kText));
            title->addStretch(1);
            nameBlock->addLayout(title);
        }
        auto* meta = new QHBoxLayout;
        meta->setSpacing(10);
        if (named) {
            meta->addWidget(iconLabel(QStringLiteral("scan-barcode"), 18, QColor(kMuted)));
            meta->addWidget(txt(QString::fromStdString(playerId_), mono(16, QFont::Normal), kMuted));
        }
        auto* badge = new QFrame;
        badge->setObjectName(QStringLiteral("underageBadge"));
        badge->setFixedHeight(30);
        badge->setStyleSheet(QStringLiteral(
            "#underageBadge { background:rgba(242,139,130,0.14); border-radius:999px; }"));
        auto* badgeRow = new QHBoxLayout(badge);
        badgeRow->setContentsMargins(8, 0, 12, 0);
        badgeRow->setSpacing(6);
        badgeRow->addWidget(iconLabel(QStringLiteral("ban"), 17, QColor("#F28B82")));
        badgeRow->addWidget(txt(QStringLiteral("Under 18"), ui(16, QFont::Bold), "#F28B82"));
        meta->addWidget(badge);
        const QString status = named
                                   ? (player->membership() != MembershipType::None
                                          ? membershipTypeLabel(player->membership())
                                          : QStringLiteral("Member"))
                                   : QStringLiteral("New player");
        meta->addWidget(txt(status, ui(17, QFont::Normal), kMuted));
        meta->addStretch(1);
        nameBlock->addLayout(meta);
    } else if (known) {
        auto* name = new ElideLabel(player && realName(*player) ? toQ(player->name()) : displayName(),
                                    ui(40, QFont::Bold), kText);
        name->setFixedHeight(48);
        nameBlock->addWidget(name);
        auto* meta = new QHBoxLayout;
        meta->setSpacing(10);
        meta->addWidget(iconLabel(QStringLiteral("scan-barcode"), 18, QColor(kMuted)));
        meta->addWidget(txt(QString::fromStdString(playerId_), mono(16, QFont::Normal), kMuted));
        auto* membership = new TapFrame;
        membership->setObjectName(QStringLiteral("membershipChip"));
        membership->setFixedHeight(44);
        membership->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        const bool typed = player && player->membership() != MembershipType::None;
        membership->setStyleSheet(typed
            ? QStringLiteral("#membershipChip { background:#2A3038; border:1px solid #3D4651; border-radius:22px; }"
                             "#membershipChip[pressed=\"true\"] { background:#343B44; }")
            : QStringLiteral("#membershipChip { background:transparent; border:1px dashed #4A535E; border-radius:22px; }"
                             "#membershipChip[pressed=\"true\"] { background:#1D232A; }"));
        auto* memRow = new QHBoxLayout(membership);
        memRow->setContentsMargins(10, 0, 12, 0);
        memRow->setSpacing(8);
        memRow->addWidget(iconLabel(QStringLiteral("award"), 18, QColor(typed ? "#DCE2E8" : "#C9D1D9")));
        memRow->addWidget(txt(typed ? QStringLiteral("%1 member").arg(membershipTypeLabel(player->membership()))
                                    : QStringLiteral("Set membership"),
                              ui(17, QFont::DemiBold), typed ? "#DCE2E8" : "#C9D1D9"));
        memRow->addWidget(iconLabel(QStringLiteral("pencil"), 16, QColor("#A3AEB9")));
        membership->setMinimumWidth(membership->sizeHint().width());
        passClicks(membership);
        connect(membership, &TapFrame::clicked, this, &CashMainPage::chooseMembership);
        meta->addWidget(membership);
        if (player && player->arcStatus() != ArcStatus::Unknown) {
            const bool arc = player->arcStatus() == ArcStatus::Arc;
            auto* arcBadge = new QFrame;
            arcBadge->setObjectName(QStringLiteral("arcBadge"));
            arcBadge->setFixedHeight(30);
            arcBadge->setStyleSheet(arc
                ? QStringLiteral("#arcBadge { background:rgba(74,222,128,0.14); border-radius:15px; }")
                : QStringLiteral("#arcBadge { background:transparent; border:1px solid #3A424C; border-radius:15px; }"));
            auto* arcRow = new QHBoxLayout(arcBadge);
            arcRow->setContentsMargins(8, 0, 12, 0);
            arcRow->setSpacing(6);
            if (arc) arcRow->addWidget(iconLabel(QStringLiteral("badge-check"), 17, QColor("#86EFAC")));
            arcRow->addWidget(txt(arc ? QStringLiteral("ARC member") : QStringLiteral("Not ARC"),
                                  ui(16, QFont::DemiBold), arc ? "#86EFAC" : kMuted));
            meta->addWidget(arcBadge);
        }
        meta->addStretch(1);
        nameBlock->addLayout(meta);
    } else {
        auto* title = new QHBoxLayout;
        title->setSpacing(8);
        title->addWidget(txt(QStringLiteral("Card"), ui(40, QFont::Bold), kText));
        title->addWidget(txt(QString::fromStdString(playerId_), mono(36, QFont::DemiBold), kText));
        title->addStretch(1);
        nameBlock->addLayout(title);
        nameBlock->addWidget(txt(QStringLiteral("No member details on file"), ui(18, QFont::Normal), kMuted));
    }
    header->addLayout(nameBlock, 1);

    auto* clear = new TapFrame;
    clear->setObjectName(QStringLiteral("clearButton"));
    clear->setFixedHeight(52);
    clear->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    clear->setStyleSheet(QStringLiteral(
        "#clearButton { background:%1; border:1px solid #2F3843; border-radius:26px; }"
        "#clearButton[pressed=\"true\"] { background:#232A32; }").arg(kRaised));
    auto* clearRow = new QHBoxLayout(clear);
    clearRow->setContentsMargins(16, 0, 18, 0);
    clearRow->setSpacing(8);
    clearRow->addWidget(iconLabel(QStringLiteral("x"), 22, QColor("#DCE3EA")));
    clearRow->addWidget(txt(QStringLiteral("Clear"), ui(20, QFont::Bold), kText));
    clear->setMinimumWidth(clear->sizeHint().width());
    passClicks(clear);
    connect(clear, &TapFrame::clicked, this, [this] { clearPlayer(); });
    header->addWidget(clear, 0, Qt::AlignTop);
    column->addLayout(header);

    if (minor) {
        auto* banner = new QFrame;
        banner->setObjectName(QStringLiteral("underageBanner"));
        banner->setStyleSheet(QStringLiteral(
            "#underageBanner { background:rgba(242,139,130,0.08); border:1px solid rgba(242,139,130,0.35); border-radius:14px; }"));
        auto* bannerRow = new QHBoxLayout(banner);
        bannerRow->setContentsMargins(24, 0, 20, 0);
        bannerRow->setSpacing(16);
        auto* notice = new QLabel;
        notice->setTextFormat(Qt::RichText);
        notice->setWordWrap(true);
        notice->setFont(ui(20, QFont::Normal));
        notice->setStyleSheet(QStringLiteral("color:#C9D1D9; background:transparent; border:none;"));
        notice->setText(QStringLiteral(
            "<span style=\"color:#F28B82; font-weight:700;\">Under 18.</span> Buy-in and cash-out are locked."));
        bannerRow->addWidget(notice, 1);
        auto* again = iconTextButton(QStringLiteral("checkIdAgain"), QStringLiteral("id-card"), 22, QColor("#DCE3EA"),
                                     QStringLiteral("Check ID again"), ui(20, QFont::DemiBold), 60,
                                     QStringLiteral("background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; padding:0 18px;")
                                         .arg(kRaised, kText));
        connect(again, &QPushButton::clicked, this, [this] { showIdCheck(playerId_); });
        bannerRow->addWidget(again);
        column->addSpacing(18);
        column->addWidget(banner, 1);
    } else if (known) {
        auto* gridHost = new QWidget;
        gridHost->setObjectName(QStringLiteral("detailGrid"));
        gridHost->setStyleSheet(QStringLiteral(
            "#detailGrid { border-top:1px solid %1; background:transparent; }").arg(kBorder));
        auto* grid = new QGridLayout(gridHost);
        grid->setContentsMargins(0, 18, 0, 0);
        grid->setHorizontalSpacing(24);
        grid->setVerticalSpacing(16);
        struct Field { QString caption; QString value; bool mono; };
        QVector<Field> fields;
        if (player) {
            if (hasDetail(toQ(player->studentNumber())))
                fields.push_back({QStringLiteral("STUDENT #"), toQ(player->studentNumber()), true});
            if (hasDetail(toQ(player->studyYear())))
                fields.push_back({QStringLiteral("YEAR"), toQ(player->studyYear()), false});
            if (hasDetail(toQ(player->degree())))
                fields.push_back({QStringLiteral("DEGREE"), toQ(player->degree()), false});
            if (hasDetail(toQ(player->email())))
                fields.push_back({QStringLiteral("EMAIL"), toQ(player->email()), false});
        }
        if (fields.isEmpty()) {
            grid->addWidget(txt(QStringLiteral("No other details on file"), ui(20, QFont::Normal), kMuted), 0, 0);
        } else {
            for (int i = 0; i < fields.size(); ++i) {
                auto* cell = new QVBoxLayout;
                cell->setSpacing(4);
                auto* cap = txt(fields[i].caption, ui(13, QFont::Bold), kMuted);
                QFont capFont = cap->font();
                capFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
                cap->setFont(capFont);
                cell->addWidget(cap);
                auto* value = new FitLabel(fields[i].value, fields[i].mono ? mono(20, QFont::Normal) : ui(22, QFont::Normal), kText);
                value->setFixedHeight(28);
                cell->addWidget(value);
                auto* wrap = new QWidget;
                wrap->setLayout(cell);
                grid->addWidget(wrap, i / 2, i % 2);
            }
        }
        column->addSpacing(18);
        column->addWidget(gridHost, 1);
    } else {
        auto* box = new QFrame;
        box->setObjectName(QStringLiteral("emptyFile"));
        box->setStyleSheet(QStringLiteral(
            "#emptyFile { background:transparent; border:1px dashed #2F3843; border-radius:14px; }"));
        auto* boxRow = new QHBoxLayout(box);
        boxRow->setContentsMargins(24, 0, 20, 0);
        boxRow->addWidget(txt(QStringLiteral("Nothing on file for this card yet."), ui(20, QFont::Normal), kMuted));
        boxRow->addStretch(1);
        auto* set = iconTextButton(QStringLiteral("setMembership"), QStringLiteral("award"), 22, QColor("#DCE3EA"),
                                   QStringLiteral("Set membership"), ui(20, QFont::DemiBold), 60,
                                   QStringLiteral("background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; padding:0 18px;")
                                       .arg(kRaised, kText));
        connect(set, &QPushButton::clicked, this, &CashMainPage::chooseMembership);
        boxRow->addWidget(set);
        column->addSpacing(18);
        column->addWidget(box, 1);
    }
    knownLayout_->addWidget(body);
    if (swipe_) swipe_->watch(playerCard_);
    placeHandle();
}

void CashMainPage::updateActions() {
    const bool loaded = !playerId_.empty();
    const Player* player = loaded ? ctx_.players().find(playerId_) : nullptr;
    const bool minor = player && player->isUnderage();
    const bool active = loaded && !minor;
    int buyIns = 0;
    int rebuys = 0;
    if (loaded) {
        for (const auto& transaction : ctx_.log().entries()) {
            if (transaction->playerId() != playerId_ || !isBuy(transaction->type())) continue;
            if (transaction->type() == TransactionType::BuyIn) ++buyIns;
            else ++rebuys;
        }
    }
    const bool rebuy = buyIns + rebuys > 0;
    const BuyInHistory history = loaded ? ctx_.buyIns().history(playerId_) : BuyInHistory{};
    const bool capped = active && !ctx_.cashGame().config().hasRoom(history);
    const auto paint = [&](Action& action, bool in, bool enabled, const QString& label, const QString& sub,
                           const QString& iconName) {
        const QString bg = !enabled ? (in ? "#1C1F28" : "#232521") : (in ? "#9FA2E5" : "#E5E29F");
        const QString border = !enabled ? (in ? "#393C52" : "#4E4F3D") : (in ? "#B4B6EE" : "#F0EDB9");
        const QString ink = !enabled ? (in ? "#9FA2E5" : "#E5E29F") : (in ? "#14152B" : "#1F1D08");
        const QString subColor = !enabled ? (in ? "#8D90C9" : "#B9B78A") : (in ? "#2A2C52" : "#3B3916");
        const QString iconBg = !enabled ? (in ? "rgba(159,162,229,0.14)" : "rgba(229,226,159,0.12)")
                                        : (in ? "rgba(20,21,43,0.10)" : "rgba(31,29,8,0.08)");
        const QString id = action.button->objectName();
        action.button->setStyleSheet(QStringLiteral(
            "#%1 { background:%2; border:2px solid %3; border-radius:22px; }"
            "#%1[pressed=\"true\"] { border-color:%2; }").arg(id, bg, border));
        action.icon->setStyleSheet(QStringLiteral("#actionIcon { background:%1; border-radius:20px; }").arg(iconBg));
        action.icon->setPixmap(iconPx(iconName, 40, QColor(ink)));
        action.title->setText(label);
        action.title->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(ink));
        action.sub->setText(sub);
        action.sub->setVisible(!sub.isEmpty());
        action.sub->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(subColor));
        action.button->setGraphicsEffect(nullptr);
    };

    QString buySub;
    if (minor) {
        buySub = QStringLiteral("Locked for this player");
    } else if (capped) {
        buySub = QStringLiteral("Daily maximum reached");
    } else if (active) {
        const auto ceiling = ctx_.cashGame().config().ceiling(history);
        buySub = ceiling ? QStringLiteral("Up to %1").arg(money(*ceiling)) : QStringLiteral("No limit");
    }
    const QString cashSub = minor ? QStringLiteral("Locked for this player") : QString();
    paint(buy_, true, active && !capped, active && rebuy ? QStringLiteral("Rebuy") : QStringLiteral("Buy-in"),
          buySub, QStringLiteral("arrow-down-to-line"));
    paint(cash_, false, active, QStringLiteral("Cash-out"), cashSub, QStringLiteral("arrow-up-from-line"));
}

bool CashMainPage::loadedMinor() const {
    const Player* player = playerId_.empty() ? nullptr : ctx_.players().find(playerId_);
    return player && player->isUnderage();
}

void CashMainPage::placeHandle() {
    if (!swipeHandle_ || !playerCard_) return;
    swipeHandle_->setVisible(!playerId_.empty());
    swipeHandle_->move((playerCard_->width() - swipeHandle_->width()) / 2,
                       playerCard_->height() - 8 - swipeHandle_->height());
    swipeHandle_->raise();
}

void CashMainPage::resolveIdCheck(bool adult) {
    const std::string id = pendingId_;
    pendingId_.clear();
    if (id.empty()) {
        closeOverlay();
        focusScanner();
        return;
    }
    try {
        if (ctx_.players().find(id)) {
            ctx_.players().setUnderage(id, !adult);
        } else {
            Player player(id, "New Player");
            player.setUnderage(!adult);
            ctx_.players().save(std::move(player));
            ctx_.players().logAttendance(id);
        }
    } catch (const std::exception& error) {
        closeOverlay();
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("Error adding player: %1").arg(error.what()));
        focusScanner();
        return;
    }
    closeOverlay();
    playerId_ = id;
    refresh();
    focusScanner();
}

void CashMainPage::showIdCheck(const std::string& id) {
    if (id.empty()) return;
    closeOverlay();
    pendingId_ = id;
    overlayOpen_ = true;
    idCheck_ = true;
    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    auto* scrim = new Scrim(host);
    dim_ = scrim;
    dim_->setObjectName(QStringLiteral("idScrim"));
    dim_->setStyleSheet(QStringLiteral("#idScrim { background:rgba(5,7,9,0.72); }"));
    dim_->setGeometry(host->rect());

    auto* card = new QFrame(dim_);
    idDialog_ = card;
    card->setObjectName(QStringLiteral("idDialog"));
    card->setFixedWidth(880);
    card->setStyleSheet(QStringLiteral(
        "#idDialog { background:%1; border:1px solid #2A323C; border-radius:24px; }").arg(kPanel));
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(120);
    shadow->setOffset(0, 40);
    shadow->setColor(QColor(0, 0, 0, 153));
    card->setGraphicsEffect(shadow);

    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(40, 40, 40, 40);
    column->setSpacing(28);

    auto* header = new QHBoxLayout;
    header->setSpacing(22);
    auto* iconBox = new QLabel;
    iconBox->setObjectName(QStringLiteral("idIcon"));
    iconBox->setAttribute(Qt::WA_StyledBackground);
    iconBox->setFixedSize(76, 76);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setPixmap(iconPx(QStringLiteral("id-card"), 38, QColor("#9FA2E5")));
    iconBox->setStyleSheet(QStringLiteral("#idIcon { background:rgba(159,162,229,0.16); border-radius:20px; }"));
    header->addWidget(iconBox, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(6);
    auto* kicker = txt(QStringLiteral("NEW PLAYER"), ui(14, QFont::Bold), "#9FA2E5");
    QFont kickerFont = kicker->font();
    kickerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    kicker->setFont(kickerFont);
    text->addWidget(kicker);
    text->addWidget(txt(QStringLiteral("Check their ID"), ui(48, QFont::Bold), kText));
    auto* sub = new QLabel;
    sub->setTextFormat(Qt::RichText);
    sub->setWordWrap(true);
    sub->setFont(ui(20, QFont::Normal));
    sub->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(kMuted));
    sub->setText(QStringLiteral("Card %1 · first time here, they'll be added as a new player")
                     .arg(QString::fromStdString(id).toHtmlEscaped()));
    text->addWidget(sub);
    header->addLayout(text, 1);
    column->addLayout(header);

    auto* cutoff = new QFrame;
    cutoff->setObjectName(QStringLiteral("cutoffBlock"));
    cutoff->setStyleSheet(QStringLiteral(
        "#cutoffBlock { background:#12171B; border:1px solid #2A323C; border-radius:18px; }"));
    auto* cutoffRow = new QHBoxLayout(cutoff);
    cutoffRow->setContentsMargins(30, 26, 30, 26);
    cutoffRow->setSpacing(16);
    auto* cutoffText = new QVBoxLayout;
    cutoffText->setSpacing(6);
    auto* cutoffLabel = txt(QStringLiteral("18+ IF BORN ON OR BEFORE"), ui(14, QFont::Bold), kMuted);
    QFont cutoffFont = cutoffLabel->font();
    cutoffFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    cutoffLabel->setFont(cutoffFont);
    cutoffText->addWidget(cutoffLabel);
    const QDate adultOn = QDate::currentDate().addYears(-18);
    cutoffText->addWidget(txt(adultOn.toString(QStringLiteral("d MMM yyyy")), ui(76, QFont::Bold), kText));
    cutoffRow->addLayout(cutoffText, 1);
    auto* mark = txt(QStringLiteral("18+"), ui(34, QFont::Bold), kText, Qt::AlignCenter);
    mark->setObjectName(QStringLiteral("ageMark"));
    mark->setAttribute(Qt::WA_StyledBackground);
    mark->setFixedSize(88, 88);
    mark->setStyleSheet(QStringLiteral(
        "#ageMark { background:transparent; color:%1; border:2px solid #2F3843; border-radius:44px; }").arg(kText));
    cutoffRow->addWidget(mark, 0, Qt::AlignVCenter);
    column->addWidget(cutoff);

    auto* digital = new QFrame;
    digital->setObjectName(QStringLiteral("digitalId"));
    digital->setStyleSheet(QStringLiteral(
        "#digitalId { background:%1; border:1px solid #2F3843; border-radius:16px; }").arg(kRaised));
    auto* digitalRow = new QHBoxLayout(digital);
    digitalRow->setContentsMargins(22, 18, 22, 18);
    digitalRow->setSpacing(18);
    auto* phone = new QLabel;
    phone->setObjectName(QStringLiteral("phoneIcon"));
    phone->setAttribute(Qt::WA_StyledBackground);
    phone->setFixedSize(48, 48);
    phone->setAlignment(Qt::AlignCenter);
    phone->setPixmap(iconPx(QStringLiteral("smartphone"), 26, QColor("#DCE3EA")));
    phone->setStyleSheet(QStringLiteral("#phoneIcon { background:#262D35; border-radius:12px; }"));
    digitalRow->addWidget(phone);
    auto* rule = new QLabel;
    rule->setWordWrap(true);
    rule->setFont(ui(21, QFont::Normal));
    rule->setTextFormat(Qt::RichText);
    rule->setStyleSheet(QStringLiteral("color:#C9D1D9; background:transparent; border:none;"));
    rule->setText(QStringLiteral(
        "<span style=\"color:#EEF2F5; font-weight:700;\">Digital ID?</span> They must also show their student ID card."));
    digitalRow->addWidget(rule, 1);
    column->addWidget(digital);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(16);
    buttons->setContentsMargins(0, 4, 0, 0);
    auto* cancel = bareButton();
    cancel->setObjectName(QStringLiteral("idCancel"));
    cancel->setText(QStringLiteral("Cancel"));
    cancel->setFont(ui(22, QFont::DemiBold));
    cancel->setFixedSize(168, 88);
    cancel->setStyleSheet(QStringLiteral(
        "#idCancel { background:transparent; color:#C9D1D9; border:1px solid #2F3843; border-radius:18px; padding:0 16px; min-height:88px; }"
        "#idCancel:hover, #idCancel:pressed { background:#1D232A; color:#C9D1D9; border:1px solid #2F3843; }"));
    connect(cancel, &QPushButton::clicked, this, [this] {
        pendingId_.clear();
        closeOverlay();
        clearPlayer();
    });

    auto* under = bareButton();
    under->setObjectName(QStringLiteral("idUnder"));
    under->setText(QStringLiteral("Under 18"));
    under->setFont(ui(26, QFont::Bold));
    under->setIcon(QIcon(iconPx(QStringLiteral("ban"), 26, QColor("#F28B82"))));
    under->setIconSize(QSize(26, 26));
    under->setFixedHeight(88);
    under->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    under->setStyleSheet(QStringLiteral(
        "#idUnder { background:rgba(242,139,130,0.10); color:#F28B82; border:2px solid #845250; border-radius:18px; padding:0 16px; min-height:88px; }"
        "#idUnder:hover, #idUnder:pressed { background:rgba(242,139,130,0.16); color:#F28B82; border:2px solid #845250; }"));
    connect(under, &QPushButton::clicked, this, [this] { resolveIdCheck(false); });

    auto* adultBtn = bareButton();
    adultBtn->setObjectName(QStringLiteral("idAdult"));
    adultBtn->setText(QStringLiteral("ID checked · 18+"));
    adultBtn->setFont(ui(26, QFont::Bold));
    adultBtn->setIcon(QIcon(iconPx(QStringLiteral("check"), 30, QColor("#052E16"))));
    adultBtn->setIconSize(QSize(30, 30));
    adultBtn->setFixedHeight(88);
    adultBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    adultBtn->setStyleSheet(QStringLiteral(
        "#idAdult { background:#4ADE80; color:#052E16; border:2px solid #6EE7A0; border-radius:18px; padding:0 16px; min-height:88px; }"
        "#idAdult:hover, #idAdult:pressed { background:#4ADE80; color:#052E16; border:2px solid #6EE7A0; }"));
    connect(adultBtn, &QPushButton::clicked, this, [this] { resolveIdCheck(true); });

    buttons->addWidget(cancel);
    buttons->addWidget(under, 10);
    buttons->addWidget(adultBtn, 14);
    column->addLayout(buttons);

    card->adjustSize();
    card->move((dim_->width() - card->width()) / 2, (dim_->height() - card->height()) / 2);
    dim_->show();
    dim_->raise();
    dim_->setFocus();
    if (scanner_) scanner_->clear();
}

void CashMainPage::chooseMembership() {
    if (playerId_.empty()) return;
    ensureRecord();
    closeOverlay();
    overlayOpen_ = true;
    dim_ = new QWidget(this);
    dim_->setGeometry(rect());
    dim_->setObjectName(QStringLiteral("scrim"));
    dim_->setStyleSheet(QStringLiteral("#scrim { background:rgba(0,0,0,0.62); }"));
    auto* card = new QFrame(dim_);
    card->setObjectName(QStringLiteral("memberDialog"));
    card->setFixedWidth(680);
    card->setStyleSheet(QStringLiteral(
        "#memberDialog { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(28, 28, 28, 28);
    column->setSpacing(16);
    column->addWidget(txt(QStringLiteral("Membership"), ui(32, QFont::Bold), kText));
    const MembershipType types[] = {MembershipType::Bronze, MembershipType::Silver, MembershipType::Gold,
                                    MembershipType::Platinum, MembershipType::Diamond};
    for (MembershipType type : types) {
        auto* button = bareButton();
        button->setFixedHeight(96);
        button->setText(membershipTypeLabel(type));
        button->setFont(ui(28, QFont::Bold));
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; color:%2; border:1px solid #2F3843; border-radius:18px; min-height:96px; }")
                                  .arg(kRaised, kText));
        connect(button, &QPushButton::clicked, this, [this, type] {
            ctx_.players().setMembership(playerId_, type);
            closeOverlay();
            refresh();
            focusScanner();
        });
        column->addWidget(button);
    }
    auto* cancel = bareButton();
    cancel->setFixedHeight(80);
    cancel->setText(QStringLiteral("Cancel"));
    cancel->setFont(ui(22, QFont::DemiBold));
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; border:1px solid #2F3843; border-radius:18px; min-height:80px; }").arg(kMuted));
    connect(cancel, &QPushButton::clicked, this, [this] {
        closeOverlay();
        focusScanner();
    });
    column->addWidget(cancel);
    card->adjustSize();
    card->move((width() - card->width()) / 2, (height() - card->height()) / 2);
    dim_->show();
    dim_->raise();
}

void CashMainPage::showKeypad() {
    closeOverlay();
    overlayOpen_ = true;
    dim_ = new QWidget(this);
    dim_->setGeometry(rect());
    dim_->setObjectName(QStringLiteral("scrim"));
    dim_->setStyleSheet(QStringLiteral("#scrim { background:rgba(0,0,0,0.62); }"));
    auto* card = new QFrame(dim_);
    card->setObjectName(QStringLiteral("keypadDialog"));
    card->setFixedWidth(460);
    card->setStyleSheet(QStringLiteral(
        "#keypadDialog { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(12);
    column->addWidget(txt(QStringLiteral("Type ID"), ui(28, QFont::Bold), kText));
    auto* entry = new QLineEdit;
    entry->setFont(mono(32, QFont::DemiBold));
    entry->setFixedHeight(64);
    entry->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; padding:0 16px; }")
                             .arg(kRaised, kText));
    column->addWidget(entry);
    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "OK"};
    for (int i = 0; i < 12; ++i) {
        auto* key = bareButton();
        key->setFixedHeight(72);
        key->setText(QString::fromUtf8(keys[i]));
        key->setFont(ui(24, QFont::Bold));
        key->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; }").arg(kRaised, kText));
        const QString label = key->text();
        connect(key, &QPushButton::clicked, this, [this, entry, label] {
            if (label == QLatin1String("⌫")) {
                entry->backspace();
            } else if (label == QLatin1String("OK")) {
                const QString id = entry->text().trimmed();
                closeOverlay();
                if (!id.isEmpty()) lookup(id);
                else focusScanner();
            } else {
                entry->insert(label);
            }
        });
        grid->addWidget(key, i / 3, i % 3);
    }
    column->addLayout(grid);
    auto* cancel = bareButton();
    cancel->setFixedHeight(52);
    cancel->setText(QStringLiteral("Cancel"));
    cancel->setFont(ui(18, QFont::DemiBold));
    cancel->setStyleSheet(QStringLiteral("QPushButton { color:%1; }").arg(kMuted));
    connect(cancel, &QPushButton::clicked, this, [this] {
        closeOverlay();
        focusScanner();
    });
    column->addWidget(cancel);
    card->adjustSize();
    card->move((width() - card->width()) / 2, (height() - card->height()) / 2);
    dim_->show();
    dim_->raise();
    entry->setFocus();
}

void CashMainPage::closeOverlay() {
    overlayOpen_ = false;
    idCheck_ = false;
    idDialog_ = nullptr;
    if (dim_) {
        dim_->hide();
        dim_->deleteLater();
        dim_ = nullptr;
    }
    if (holdUndo_) return;
    if (undoId_ != 0 && undoLeft_ > 0 && undoTimer_ && !undoTimer_->isActive()) undoTimer_->start(1000);
}

void CashMainPage::afterMoneyTransaction() {
    const auto& entries = ctx_.log().entries();
    if (!entries.empty()) {
        const Transaction* transaction = entries.back().get();
        undoId_ = transaction->id();
        undoLeft_ = 8;
        const AmountTone tone = toneFor(transaction->type());
        QString amount = money(transaction->amount());
        if (tone == AmountTone::In) amount.prepend(QLatin1Char('+'));
        else if (tone == AmountTone::Out) amount = QStringLiteral("−") + money(std::llabs(transaction->amount().cents()));
        const Player* player = ctx_.players().find(transaction->playerId());
        const QString who = player && realName(*player) ? toQ(player->name())
                                                        : QString::fromStdString(transaction->playerId());
        toastText_->setText(QStringLiteral(
            "<span style=\"color:#EEF2F5; font-weight:700;\">%1 %2</span>"
            "<span style=\"color:#B7C7BE;\"> recorded for %3</span>")
                                .arg(plainType(transaction->type()), amount.toHtmlEscaped(), who.toHtmlEscaped()));
        undoButton_->setText(QStringLiteral("Undo  8s"));
        fitIconText(undoButton_);
        toastWrap_->show();
        undoTimer_->start(1000);
    }
    playerId_.clear();
    if (scanner_) scanner_->clear();
    refresh();
    focusScanner();
}

void CashMainPage::tickUndo() {
    if (undoLeft_ <= 0) return;
    --undoLeft_;
    undoButton_->setText(QStringLiteral("Undo  %1s").arg(undoLeft_));
    fitIconText(undoButton_);
    if (undoLeft_ <= 0) {
        undoTimer_->stop();
        toastWrap_->hide();
        undoId_ = 0;
    }
}

void CashMainPage::askUndoPassword() {
    if (undoId_ == 0) return;
    closeOverlay();
    holdUndo_ = true;
    if (undoTimer_) undoTimer_->stop();
    overlayOpen_ = true;

    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    auto* scrim = new Scrim(host);
    dim_ = scrim;
    dim_->setObjectName(QStringLiteral("idScrim"));
    dim_->setStyleSheet(QStringLiteral("#idScrim { background:rgba(5,7,9,0.72); }"));
    dim_->setGeometry(host->rect());

    auto* card = new QFrame(dim_);
    idDialog_ = card;
    card->setObjectName(QStringLiteral("passwordDialog"));
    card->setFixedWidth(460);
    card->setStyleSheet(QStringLiteral(
        "#passwordDialog { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* column = new QVBoxLayout(card);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(12);

    auto* heading = new QHBoxLayout;
    heading->setSpacing(12);
    auto* lock = new QLabel;
    lock->setObjectName(QStringLiteral("undoLock"));
    lock->setAttribute(Qt::WA_StyledBackground);
    lock->setFixedSize(48, 48);
    lock->setAlignment(Qt::AlignCenter);
    lock->setPixmap(iconPx(QStringLiteral("lock"), 24, QColor("#DCE3EA")));
    lock->setStyleSheet(QStringLiteral("#undoLock { background:%1; border-radius:14px; }").arg(kRaised));
    heading->addWidget(lock);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(txt(QStringLiteral("Admin password"), ui(28, QFont::Bold), kText));
    titles->addWidget(txt(QStringLiteral("Required to undo this transaction."), ui(16, QFont::Normal), kMuted));
    heading->addLayout(titles, 1);
    column->addLayout(heading);

    auto* entry = new QLineEdit;
    entry->setEchoMode(QLineEdit::Password);
    entry->setFont(mono(32, QFont::DemiBold));
    entry->setAlignment(Qt::AlignCenter);
    entry->setFixedHeight(64);
    entry->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; padding:0 16px; }")
                             .arg(kRaised, kText));
    column->addWidget(entry);

    auto* error = txt(QStringLiteral("Incorrect password"), ui(16, QFont::DemiBold), "#F28B82", Qt::AlignCenter);
    error->hide();
    column->addWidget(error);

    const auto confirm = [this, entry, error] {
        if (ctx_.admin().checkPassword(toStd(entry->text()))) {
            holdUndo_ = false;
            undoLast();
            closeOverlay();
            focusScanner();
            return;
        }
        entry->clear();
        error->show();
        entry->setFocus();
    };

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "OK"};
    for (int i = 0; i < 12; ++i) {
        auto* key = bareButton();
        key->setFixedHeight(72);
        key->setText(QString::fromUtf8(keys[i]));
        key->setFont(ui(24, QFont::Bold));
        key->setStyleSheet(QStringLiteral(
            "QPushButton { background:%1; color:%2; border:1px solid #2F3843; border-radius:14px; }")
                               .arg(kRaised, kText));
        const QString label = key->text();
        connect(key, &QPushButton::clicked, this, [entry, error, label, confirm] {
            if (label == QLatin1String("⌫")) {
                entry->backspace();
            } else if (label == QLatin1String("OK")) {
                confirm();
                return;
            } else {
                entry->insert(label);
            }
            error->hide();
            entry->setFocus();
        });
        grid->addWidget(key, i / 3, i % 3);
    }
    column->addLayout(grid);
    connect(entry, &QLineEdit::returnPressed, this, confirm);

    auto* cancel = bareButton();
    cancel->setFixedHeight(52);
    cancel->setText(QStringLiteral("Cancel"));
    cancel->setFont(ui(18, QFont::DemiBold));
    cancel->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; color:%1; border:1px solid #2F3843; border-radius:14px; }")
                              .arg(kMuted));
    connect(cancel, &QPushButton::clicked, this, [this] {
        holdUndo_ = false;
        closeOverlay();
        focusScanner();
    });
    column->addWidget(cancel);
    card->adjustSize();
    card->move((dim_->width() - card->width()) / 2, (dim_->height() - card->height()) / 2);
    dim_->show();
    dim_->raise();
    entry->setFocus();
}

void CashMainPage::undoLast() {
    if (undoId_ == 0) return;
    const auto result = ctx_.ledger().undo(undoId_);
    if (!result.success) {
        toastText_->setText(QStringLiteral("<span style=\"color:#B7C7BE;\">%1</span>")
                                .arg(toQ(result.message).toHtmlEscaped()));
        return;
    }
    undoTimer_->stop();
    undoId_ = 0;
    toastWrap_->hide();
    refresh();
    focusScanner();
}

} // namespace pokersoc::ui
