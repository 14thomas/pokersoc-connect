#include "ui/screens/Screens.h"

#include "ui/SessionContext.h"
#include "ui/Style.h"
#include "ui/cash/CashMainPage.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <unordered_map>

#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHash>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QAbstractListModel>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "application/ImportExportService.h"
#include "application/PlayerService.h"
#include "domain/player/Attendance.h"
#include "domain/player/Player.h"
#include "domain/transaction/CashOut.h"
#include "util/Csv.h"
#include "util/Time.h"

namespace pokersoc::ui {

using namespace domain;

namespace {

constexpr const char* kBg = "#020509";
constexpr const char* kPanel = "#040b13";
constexpr const char* kRaised = "#1D232A";
constexpr const char* kBorder = "#1e262f";
constexpr const char* kText = "#EEF2F5";
constexpr const char* kMuted = "#A3AEB9";
constexpr const char* kFaint = "#7D8894";
constexpr const char* kCashIn = "#9FA2E5";
constexpr const char* kCashOut = "#E5E29F";
constexpr const char* kInset = "#020509";
constexpr const char* kStrip = "#030810";
constexpr const char* kDivider = "#141B23";
constexpr const char* kBorderRaised = "#2A323C";
constexpr const char* kBorderButton = "#2F3843";
constexpr const char* kSoftText = "#C9D1D9";
constexpr const char* kIcon = "#DCE3EA";

QString loadFace(const char* path) {
    QFile file(assetPath(QString::fromUtf8(path)));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const int id = QFontDatabase::addApplicationFontFromData(file.readAll());
    if (id < 0) return {};
    const QStringList names = QFontDatabase::applicationFontFamilies(id);
    return names.isEmpty() ? QString() : names.first();
}

struct FaceSet {
    QString regular, medium, semibold, bold;
    bool one = false;
};

const FaceSet& barlowFaces() {
    static const FaceSet faces = [] {
        FaceSet loaded;
        loaded.regular = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Regular.ttf");
        loaded.medium = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Medium.ttf");
        loaded.semibold = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-SemiBold.ttf");
        loaded.bold = loadFace("Fonts/BarlowSemiCondensed/BarlowSemiCondensed-Bold.ttf");
        loaded.one = !loaded.regular.isEmpty() && loaded.regular == loaded.medium &&
                     loaded.medium == loaded.semibold && loaded.semibold == loaded.bold;
        return loaded;
    }();
    return faces;
}

const FaceSet& monoFaces() {
    static const FaceSet faces = [] {
        FaceSet loaded;
        loaded.regular = loadFace("Fonts/JetBrainsMono/JetBrainsMono-Regular.ttf");
        loaded.medium = loadFace("Fonts/JetBrainsMono/JetBrainsMono-Medium.ttf");
        loaded.semibold = loadFace("Fonts/JetBrainsMono/JetBrainsMono-SemiBold.ttf");
        loaded.bold = loaded.semibold;
        loaded.one = !loaded.regular.isEmpty() && loaded.regular == loaded.medium &&
                     loaded.medium == loaded.semibold;
        return loaded;
    }();
    return faces;
}

QFont faceFont(const FaceSet& faces, int px, int weight, const QString& fallback) {
    QString family = faces.regular;
    int applied = weight;
    if (!faces.one) {
        applied = QFont::Normal;
        if (weight >= QFont::Bold && !faces.bold.isEmpty()) {
            family = faces.bold;
            if (faces.bold == faces.regular) applied = QFont::Bold;
        } else if (weight >= QFont::DemiBold && !faces.semibold.isEmpty()) {
            family = faces.semibold;
        } else if (weight >= QFont::Medium && !faces.medium.isEmpty()) {
            family = faces.medium;
        }
    }
    if (family.isEmpty()) family = fallback;
    QFont font(family);
    font.setPixelSize(px);
    font.setWeight(static_cast<QFont::Weight>(applied));
    return font;
}

QFont ui(int px, int weight) { return faceFont(barlowFaces(), px, weight, QStringLiteral("Arial Narrow")); }
QFont mono(int px, int weight) { return faceFont(monoFaces(), px, weight, QStringLiteral("Consolas")); }

QPixmap iconPx(const QString& name, int px, const QColor& color) {
    const qreal dpr = qApp->devicePixelRatio();
    const QString key = name + QString::number(px) + color.name() + QString::number(dpr);
    static QHash<QString, QPixmap> cache;
    if (const auto it = cache.constFind(key); it != cache.cend()) return it.value();
    QFile file(assetPath(QStringLiteral("Icons/%1.svg").arg(name)));
    QPixmap pixmap(qRound(px * dpr), qRound(px * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    if (file.open(QIODevice::ReadOnly)) {
        QString svg = QString::fromUtf8(file.readAll());
        svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
        QSvgRenderer renderer(svg.toUtf8());
        QPainter painter(&pixmap);
        renderer.render(&painter, QRectF(0, 0, px, px));
    }
    cache.insert(key, pixmap);
    return pixmap;
}

QLabel* iconLabel(const QString& name, int px, const QColor& color) {
    auto* label = new QLabel;
    label->setPixmap(iconPx(name, px, color));
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

QLabel* caps(const QString& text, int px = 13, const QString& color = QString::fromLatin1(kMuted)) {
    auto* label = txt(text, ui(px, QFont::Bold), color);
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    label->setFont(font);
    return label;
}

class ElideLabel : public QLabel {
public:
    explicit ElideLabel(const QString& text, const QFont& font, const QString& color,
                        Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter, QWidget* parent = nullptr)
        : QLabel(parent), full_(text), color_(color), align_(align) {
        setFont(font);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(QColor(color_));
        const QString shown = fontMetrics().elidedText(full_, Qt::ElideRight, qMax(0, width()));
        painter.drawText(rect(), static_cast<int>(align_), shown);
    }

private:
    QString full_;
    QString color_;
    Qt::Alignment align_;
};

void polish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void passClicks(QWidget* host) {
    const auto children = host->findChildren<QWidget*>();
    for (QWidget* child : children) child->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void wipe(QLayout* layout) {
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        } else if (QLayout* child = item->layout()) {
            wipe(child);
        }
        delete item;
    }
}

// Stretch columns must ignore their text width. A card number's size hint
// otherwise makes the row wider than the header and shifts every column right.
void stretchColumn(QWidget* widget) {
    widget->setMinimumWidth(0);
    widget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void pinColumn(QWidget* widget, int width) {
    widget->setMinimumWidth(width);
    widget->setMaximumWidth(width);
    widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
}

// The app stylesheet paints every QLineEdit white. An opaque dark base is what
// actually shows the typed text.
void darkLine(QLineEdit* edit, const char* background) {
    edit->setFrame(false);
    edit->setAttribute(Qt::WA_StyledBackground);
    edit->setAutoFillBackground(true);
    const QColor bg(background);
    const QColor fg(kText);
    QPalette pal = edit->palette();
    pal.setColor(QPalette::Base, bg);
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::Text, fg);
    pal.setColor(QPalette::PlaceholderText, QColor(kFaint));
    pal.setColor(QPalette::Highlight, QColor(kBorderRaised));
    pal.setColor(QPalette::HighlightedText, fg);
    edit->setPalette(pal);
}

QPushButton* iconButton(const QString& id, const QString& icon, int iconPxSize, const QColor& iconColor,
                        const QString& text, const QFont& font, int height, const QString& rules) {
    auto* button = new QPushButton(text);
    button->setObjectName(id);
    button->setFont(font);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFlat(true);
    if (!icon.isEmpty()) {
        button->setIcon(QIcon(iconPx(icon, iconPxSize, iconColor)));
        button->setIconSize(QSize(iconPxSize, iconPxSize));
    }
    button->setFixedHeight(height);
    button->setStyleSheet(QStringLiteral("#%1 { %2 }").arg(id, rules));
    const int iconW = icon.isEmpty() ? 0 : iconPxSize;
    const int gap = iconW > 0 && !text.isEmpty() ? 10 : 0;
    const int textW = text.isEmpty() ? 0 : QFontMetrics(font).horizontalAdvance(text);
    button->setMinimumWidth(textW + iconW + gap + 8);
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

bool realName(const Player& player) {
    const QString name = QString::fromStdString(player.name()).trimmed();
    return !name.isEmpty() && name.compare(QLatin1String("New Player"), Qt::CaseInsensitive) != 0;
}

bool blankText(const QString& value, bool name) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed.compare(QLatin1String("None"), Qt::CaseInsensitive) == 0 ||
        trimmed.compare(QLatin1String("N/A"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    return name && trimmed.compare(QLatin1String("New Player"), Qt::CaseInsensitive) == 0;
}

bool hasDetail(const QString& value) { return !blankText(value, false); }

QString initialsOf(const QString& name) {
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) return {};
    QString letters = words.first().left(1);
    if (words.size() > 1) letters += words.last().left(1);
    return letters.toUpper();
}

QDateTime localFromUtc(const std::string& utc) {
    QDateTime parsed = QDateTime::fromString(QString::fromStdString(utc), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    parsed.setTimeSpec(Qt::UTC);
    return parsed.toLocalTime();
}

QString clock12(const QDateTime& time) { return time.toString(QStringLiteral("h:mm AP")); }

QString joinAnd(const QStringList& parts) {
    if (parts.size() <= 1) return parts.value(0);
    if (parts.size() == 2) return parts[0] + QStringLiteral(" and ") + parts[1];
    return parts.mid(0, parts.size() - 1).join(QStringLiteral(", ")) + QStringLiteral(" and ") + parts.last();
}

const char* tierColor(MembershipType type) {
    switch (type) {
        case MembershipType::Bronze: return "#C99068";
        case MembershipType::Silver: return "#B9C2CC";
        case MembershipType::Gold: return "#E3C66E";
        case MembershipType::Platinum: return "#E7E5E4";
        case MembershipType::Diamond: return "#93C5FD";
        case MembershipType::None: break;
    }
    return "#7D8894";
}

QLabel* dot(int px, const QString& color) {
    auto* mark = new QLabel;
    mark->setObjectName(QStringLiteral("dot"));
    mark->setAttribute(Qt::WA_StyledBackground);
    mark->setAttribute(Qt::WA_TransparentForMouseEvents);
    mark->setFixedSize(px, px);
    mark->setStyleSheet(QStringLiteral("#dot { background:%1; border-radius:%2px; }").arg(color).arg(px / 2));
    return mark;
}

enum class NightStatus { Playing, Cashed, Locked };

struct Night {
    Money bought;
    Money cashed;
    int buyIns = 0;
    int rebuys = 0;
    QDateTime firstBuy;
    QDateTime lastRebuy;
    QDateTime lastOut;
    bool hasBuy = false;
    bool hasRebuy = false;
    bool hasOut = false;
    bool cashedAfter = false;
    NightStatus status = NightStatus::Playing;
};

bool after(const QDateTime& a, long long aId, const QDateTime& b, long long bId) {
    if (a != b) return a > b;
    return aId > bId;
}

struct NightWork {
    Night night;
    QDateTime lastBuy;
    long long lastBuyId = 0;
    long long lastOutId = 0;
};

void absorb(NightWork& work, const Transaction& transaction) {
    const QDateTime when = localFromUtc(transaction.time());
    const TransactionType type = transaction.type();
    if (type == TransactionType::BuyIn || type == TransactionType::Rebuy) {
        work.night.bought += transaction.amount();
        if (type == TransactionType::BuyIn) {
            ++work.night.buyIns;
            if (!work.night.hasBuy || when < work.night.firstBuy) work.night.firstBuy = when;
            work.night.hasBuy = true;
        } else {
            ++work.night.rebuys;
            if (!work.night.hasRebuy || when > work.night.lastRebuy) work.night.lastRebuy = when;
            work.night.hasRebuy = true;
        }
        if (!work.lastBuy.isValid() || after(when, transaction.id(), work.lastBuy, work.lastBuyId)) {
            work.lastBuy = when;
            work.lastBuyId = transaction.id();
        }
    } else if (type == TransactionType::CashOut) {
        if (const auto* cashOut = dynamic_cast<const CashOut*>(&transaction)) {
            work.night.cashed += cashOut->chipValue();
        } else {
            work.night.cashed += transaction.amount();
        }
        if (!work.night.hasOut || after(when, transaction.id(), work.night.lastOut, work.lastOutId)) {
            work.night.lastOut = when;
            work.lastOutId = transaction.id();
        }
        work.night.hasOut = true;
    }
}

Night finishNight(NightWork work, bool underage) {
    work.night.cashedAfter =
        work.night.hasOut && (!work.lastBuy.isValid() || !after(work.lastBuy, work.lastBuyId, work.night.lastOut, work.lastOutId));
    if (underage) work.night.status = NightStatus::Locked;
    else if (work.night.cashedAfter) work.night.status = NightStatus::Cashed;
    else work.night.status = NightStatus::Playing;
    return work.night;
}

Night nightFor(const SessionContext& ctx, const Player& player) {
    NightWork work;
    for (const auto& transaction : ctx.log().entries()) {
        if (transaction->playerId() != player.id()) continue;
        absorb(work, *transaction);
    }
    return finishNight(work, player.isUnderage());
}

std::unordered_map<std::string, Night> gNights;

const Night& nightOf(const std::string& id) {
    static const Night empty;
    const auto it = gNights.find(id);
    return it == gNights.end() ? empty : it->second;
}

QString moneyOrDash(Money amount) {
    return amount.cents() == 0 ? QStringLiteral("—") : money(amount);
}

std::string orNone(const std::string& value) { return value.empty() ? "None" : value; }

const char* arcText(ArcStatus status) {
    switch (status) {
        case ArcStatus::Arc: return "Yes";
        case ArcStatus::NonArc: return "No";
        case ArcStatus::Unknown: return "None";
    }
    return "None";
}

ArcStatus arcFrom(const QString& text) {
    const QString value = text.trimmed().toUpper();
    if (value == "YES" || value == "Y" || value == "TRUE" || value == "1" || value == "ARC") return ArcStatus::Arc;
    if (value == "NO" || value == "N" || value == "FALSE" || value == "0" || value == "NON_ARC" || value == "NON-ARC") {
        return ArcStatus::NonArc;
    }
    return ArcStatus::Unknown;
}

QString arcLabel(ArcStatus status) {
    switch (status) {
        case ArcStatus::Arc: return QStringLiteral("ARC");
        case ArcStatus::NonArc: return QStringLiteral("Not ARC");
        case ArcStatus::Unknown: return {};
    }
    return {};
}

struct PlanChange {
    QString kind;
    QString title;
    QString sub;
    QString what;
};

struct Plan {
    int added = 0;
    int updated = 0;
    int same = 0;
    int skipped = 0;
    std::vector<PlanChange> rows;
};

bool takeField(bool fillBlanks, bool currentBlank, bool incomingBlank) {
    if (incomingBlank) return false;
    return !fillBlanks || currentBlank;
}

Plan planImport(const application::PlayerService& players, const QVector<QStringList>& rows, bool fillBlanks) {
    Plan plan;
    for (const QStringList& row : rows) {
        const QString id = row.value(1).trimmed();
        if (id.isEmpty()) {
            ++plan.skipped;
            PlanChange change;
            change.kind = QStringLiteral("SKIPPED");
            change.title = QStringLiteral("Row %1").arg(row.value(0));
            change.what = QStringLiteral("No player ID");
            plan.rows.push_back(change);
            continue;
        }
        const QString name = row.value(2);
        const QString email = row.value(3);
        const QString student = row.value(4);
        const QString degree = row.value(5);
        const QString year = row.value(6);
        const ArcStatus arc = arcFrom(row.value(7));
        const MembershipType tier = membershipTypeFromName(row.value(9).trimmed().toUpper().toStdString());
        const Player* current = players.find(id.toStdString());
        const QString shownName = blankText(name, true)
                                      ? (current && realName(*current) ? toQ(current->name()) : QStringLiteral("New player"))
                                      : name.trimmed();
        if (!current) {
            ++plan.added;
            PlanChange change;
            change.kind = QStringLiteral("NEW");
            change.title = shownName;
            change.sub = id;
            change.what = QStringLiteral("Added to the database");
            plan.rows.push_back(change);
            continue;
        }
        QStringList addedFields;
        QStringList replaced;
        const auto consider = [&](const QString& label, const QString& currentValue, const QString& incoming, bool nameField) {
            const bool currentBlank = blankText(currentValue, nameField);
            if (!takeField(fillBlanks, currentBlank, blankText(incoming, nameField))) return;
            const QString next = incoming.trimmed();
            if (!currentBlank && currentValue.trimmed().compare(next, Qt::CaseInsensitive) == 0) return;
            (currentBlank ? addedFields : replaced) << label;
        };
        consider(QStringLiteral("Name"), toQ(current->name()), name, true);
        consider(QStringLiteral("Email"), toQ(current->email()), email, false);
        consider(QStringLiteral("Student number"), toQ(current->studentNumber()), student, false);
        consider(QStringLiteral("Degree"), toQ(current->degree()), degree, false);
        consider(QStringLiteral("Year"), toQ(current->studyYear()), year, false);
        QString membershipPhrase;
        if (tier != MembershipType::None && tier != current->membership() &&
            takeField(fillBlanks, current->membership() == MembershipType::None, false)) {
            membershipPhrase = QStringLiteral("Membership %1 → %2")
                                   .arg(QString::fromLatin1(membershipTypeLabel(current->membership())),
                                        QString::fromLatin1(membershipTypeLabel(tier)));
        }
        QString arcPhrase;
        if (arc != ArcStatus::Unknown && arc != current->arcStatus() &&
            takeField(fillBlanks, current->arcStatus() == ArcStatus::Unknown, false)) {
            const QString from = current->arcStatus() == ArcStatus::Unknown ? QStringLiteral("—") : arcLabel(current->arcStatus());
            arcPhrase = QStringLiteral("ARC %1 → %2").arg(from, arcLabel(arc));
        }
        QStringList parts;
        if (!addedFields.isEmpty()) parts << joinAnd(addedFields) + QStringLiteral(" added");
        if (!replaced.isEmpty()) parts << joinAnd(replaced) + QStringLiteral(" replaced");
        if (!membershipPhrase.isEmpty()) parts << membershipPhrase;
        if (!arcPhrase.isEmpty()) parts << arcPhrase;
        if (parts.isEmpty()) {
            ++plan.same;
            continue;
        }
        ++plan.updated;
        PlanChange change;
        change.kind = QStringLiteral("UPDATED");
        change.title = realName(*current) ? toQ(current->name()) : QStringLiteral("New player");
        change.sub = id;
        change.what = parts.join(QStringLiteral(", "));
        change.what[0] = change.what[0].toUpper();
        plan.rows.push_back(change);
    }
    return plan;
}

void applyRow(application::PlayerService& players, const QStringList& row, bool fillBlanks) {
    const QString id = row.value(1).trimmed();
    if (id.isEmpty()) return;
    const Player* current = players.find(id.toStdString());
    const QString name = row.value(2);
    const QString email = row.value(3);
    const QString student = row.value(4);
    const QString degree = row.value(5);
    const QString year = row.value(6);
    const ArcStatus arc = arcFrom(row.value(7));
    const QString under = row.value(8).trimmed().toUpper();
    const MembershipType tier = membershipTypeFromName(row.value(9).trimmed().toUpper().toStdString());
    Player player = current ? *current : Player(id.toStdString(), "New Player");
    const auto assign = [&](bool nameField, const QString& incoming, const QString& currentValue, auto setter) {
        if (!takeField(fillBlanks || !current, blankText(currentValue, nameField), blankText(incoming, nameField))) return;
        setter(incoming.trimmed().toStdString());
    };
    assign(true, name, current ? toQ(current->name()) : QString(), [&](const std::string& value) { player.setName(value); });
    if (!current && blankText(name, true)) player.setName("New Player");
    assign(false, email, current ? toQ(current->email()) : QString(), [&](const std::string& value) { player.setEmail(value); });
    assign(false, student, current ? toQ(current->studentNumber()) : QString(),
           [&](const std::string& value) { player.setStudentNumber(value); });
    assign(false, degree, current ? toQ(current->degree()) : QString(), [&](const std::string& value) { player.setDegree(value); });
    assign(false, year, current ? toQ(current->studyYear()) : QString(), [&](const std::string& value) { player.setStudyYear(value); });
    if (arc != ArcStatus::Unknown && (!current || takeField(fillBlanks, current->arcStatus() == ArcStatus::Unknown, false))) {
        player.setArcStatus(arc);
    }
    if (tier != MembershipType::None &&
        (!current || takeField(fillBlanks, current->membership() == MembershipType::None, false))) {
        player.setMembership(tier);
    }
    if (!current) player.setUnderage(under == "1" || under == "TRUE");
    if (current) player.setCreatedAt(current->createdAt());
    players.save(std::move(player));
}

struct MemberRow {
    std::string id;
    QString name;
    QString sub;
    bool named = false;
    bool loaded = false;
    bool minor = false;
    bool inSession = false;
    QString student;
    QString degree;
    QString year;
    bool studentMissing = true;
    bool degreeMissing = true;
    bool yearMissing = true;
    QString membership;
    QColor tier;
    int arc = 0;
    NightStatus status = NightStatus::Playing;
    QString bought;
    QString cashed;
    QColor boughtColor = QColor(kFaint);
    QColor cashedColor = QColor(kFaint);
    QString cashedWhen;
    QString hay;
};

struct Bands {
    int x = 0;
    int w = 0;
};

struct Columns {
    Bands player, status, bought, cashed, student, degree, year, member, arc, age;
};

Columns columnsFor(int width, bool session) {
    const int gap = 16;
    const int content = width - 48;
    Columns columns;
    int x = 24;
    const auto take = [&](Bands& band, int bandWidth) {
        band.x = x;
        band.w = bandWidth;
        x += bandWidth + gap;
    };
    if (session) {
        const int fixed = 150 + 120 + 120 + 140 + 96 + 120;
        int playerW = content - fixed - 6 * gap;
        if (playerW < 0) playerW = 0;
        take(columns.player, playerW);
        take(columns.status, 150);
        take(columns.bought, 120);
        take(columns.cashed, 120);
    } else {
        const int fixed = 130 + 120 + 140 + 96 + 120;
        int stretch = content - fixed - 6 * gap;
        if (stretch < 0) stretch = 0;
        const int playerW = stretch * 10 / 19;
        take(columns.player, playerW);
        take(columns.student, 130);
        take(columns.degree, stretch - playerW);
        take(columns.year, 120);
    }
    take(columns.member, 140);
    take(columns.arc, 96);
    take(columns.age, 120);
    return columns;
}

void drawElide(QPainter& painter, const QRect& rect, const QString& text, const QFont& font, const QColor& color,
               Qt::Alignment align) {
    painter.setFont(font);
    painter.setPen(color);
    const QString shown = QFontMetrics(font).elidedText(text, Qt::ElideRight, qMax(0, rect.width()));
    painter.drawText(rect, static_cast<int>(align), shown);
}

} // namespace

class PlayerListModel : public QAbstractListModel {
public:
    struct Shown {
        bool header = false;
        int source = -1;
        QString title;
        QString color;
        int count = 0;
    };

    explicit PlayerListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(shown_.size());
    }

    QVariant data(const QModelIndex&, int) const override { return {}; }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid() || shown_.at(index.row()).header) return Qt::NoItemFlags;
        return Qt::ItemIsEnabled;
    }

    void setRows(std::vector<MemberRow> rows, bool session) {
        rows_ = std::move(rows);
        session_ = session;
    }

    void refilter(const QString& query, int filter) {
        beginResetModel();
        shown_.clear();
        QVector<int> loaded;
        QVector<int> rest;
        const QString folded = query.trimmed().toCaseFolded();
        for (int i = 0; i < static_cast<int>(rows_.size()); ++i) {
            const MemberRow& row = rows_[static_cast<size_t>(i)];
            if (session_) {
                if (filter == 3 && !row.minor) continue;
                if (filter == 1 && row.status != NightStatus::Playing) continue;
                if (filter == 2 && row.status != NightStatus::Cashed) continue;
            }
            if (!folded.isEmpty() && !row.hay.contains(folded)) continue;
            (row.loaded ? loaded : rest).push_back(i);
        }
        const auto addHeader = [&](const QString& title, const char* color, const QVector<int>& group) {
            Shown header;
            header.header = true;
            header.title = title;
            header.color = QString::fromLatin1(color);
            header.count = group.size();
            shown_.push_back(header);
            for (int source : group) {
                Shown item;
                item.source = source;
                shown_.push_back(item);
            }
        };
        if (!loaded.isEmpty()) {
            addHeader(QStringLiteral("LOADED"), kCashIn, loaded);
            if (!rest.isEmpty()) addHeader(QStringLiteral("EVERYONE ELSE"), kMuted, rest);
        } else {
            for (int source : rest) {
                Shown item;
                item.source = source;
                shown_.push_back(item);
            }
        }
        matchCount_ = loaded.size() + rest.size();
        endResetModel();
    }

    void setSelected(const std::string& id) {
        if (selected_ == id) return;
        const int previous = rowOf(selected_);
        selected_ = id;
        const int current = rowOf(id);
        const auto ping = [&](int row) {
            if (row < 0) return;
            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx);
        };
        ping(previous);
        ping(current);
    }

    int rowOf(const std::string& id) const {
        if (id.empty()) return -1;
        for (int i = 0; i < shown_.size(); ++i) {
            if (shown_.at(i).header) continue;
            if (rows_[static_cast<size_t>(shown_.at(i).source)].id == id) return i;
        }
        return -1;
    }

    std::string idAt(int row) const {
        if (row < 0 || row >= shown_.size() || shown_.at(row).header) return {};
        return rows_[static_cast<size_t>(shown_.at(row).source)].id;
    }

    bool isHeader(int row) const { return row >= 0 && row < shown_.size() && shown_.at(row).header; }
    const Shown& shownAt(int row) const { return shown_.at(row); }
    const MemberRow& playerAt(int row) const { return rows_[static_cast<size_t>(shown_.at(row).source)]; }
    bool session() const { return session_; }
    const std::string& selected() const { return selected_; }
    int matchCount() const { return matchCount_; }

private:
    std::vector<MemberRow> rows_;
    QVector<Shown> shown_;
    std::string selected_;
    bool session_ = true;
    int matchCount_ = 0;
};

class PlayerDelegate : public QStyledItemDelegate {
public:
    explicit PlayerDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent),
          nameFont_(ui(20, QFont::Medium)),
          subFont_(ui(14, QFont::Normal)),
          subStrong_(ui(14, QFont::DemiBold)),
          bodyFont_(ui(18, QFont::Normal)),
          bodyStrong_(ui(18, QFont::DemiBold)),
          moneyFont_(mono(18, QFont::DemiBold)),
          smallMono_(mono(13, QFont::Normal)),
          studentFont_(mono(16, QFont::Normal)),
          cardFont_(mono(18, QFont::Medium)),
          initialsFont_(ui(16, QFont::Bold)),
          pillFont_(ui(15, QFont::DemiBold)),
          ageFont_(ui(15, QFont::Bold)),
          capsFont_(ui(13, QFont::Bold)),
          countFont_(mono(13, QFont::Medium)),
          userIcon_(iconPx(QStringLiteral("user"), 20, QColor(kFaint))),
          lockIcon_(iconPx(QStringLiteral("lock"), 16, QColor("#F28B82"))),
          badgeIcon_(iconPx(QStringLiteral("badge-check"), 16, QColor("#86EFAC"))),
          banIcon_(iconPx(QStringLiteral("ban"), 15, QColor("#F28B82"))) {
        capsFont_.setLetterSpacing(QFont::AbsoluteSpacing, 1.8);
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
        const auto* model = static_cast<const PlayerListModel*>(index.model());
        return QSize(0, model->isHeader(index.row()) ? 44 : 64);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* model = static_cast<const PlayerListModel*>(index.model());
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        if (model->isHeader(index.row())) paintHeader(*painter, option.rect, model->shownAt(index.row()));
        else paintPlayer(*painter, option.rect, model->playerAt(index.row()), model->session(),
                         model->playerAt(index.row()).id == model->selected());
        painter->restore();
    }

private:
    void paintHeader(QPainter& painter, const QRect& rect, const PlayerListModel::Shown& header) const {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(kStrip));
        painter.drawRect(rect);
        painter.setPen(QPen(QColor(kDivider), 1));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        painter.setFont(capsFont_);
        painter.setPen(QColor(header.color));
        const QString title = header.title;
        const int titleW = QFontMetrics(capsFont_).horizontalAdvance(title);
        painter.drawText(QRect(rect.left() + 24, rect.top(), titleW + 8, rect.height()), Qt::AlignVCenter | Qt::AlignLeft, title);
        painter.setFont(countFont_);
        painter.setPen(QColor(kFaint));
        painter.drawText(QRect(rect.left() + 24 + titleW + 18, rect.top(), 80, rect.height()), Qt::AlignVCenter | Qt::AlignLeft,
                         QString::number(header.count));
    }

    void paintPlayer(QPainter& painter, const QRect& rect, const MemberRow& row, bool session, bool selected) const {
        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(159, 162, 229, 33));
            painter.drawRect(rect);
        }
        painter.setPen(QPen(QColor(kDivider), 1));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());

        const Columns columns = columnsFor(rect.width(), session);
        const QRect avatar(columns.player.x, rect.center().y() - 20, 40, 40);
        painter.setPen(Qt::NoPen);
        if (row.named) {
            painter.setBrush(QColor("#262A40"));
            painter.drawEllipse(avatar);
            painter.setFont(initialsFont_);
            painter.setPen(QColor("#D9DBF7"));
            painter.drawText(avatar, Qt::AlignCenter, initialsOf(row.name));
        } else {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor("#3A424C"), 1, Qt::DashLine));
            painter.drawEllipse(avatar.adjusted(1, 1, -1, -1));
            painter.drawPixmap(avatar.center().x() - 10, avatar.center().y() - 10, userIcon_);
        }

        int textW = columns.player.w - 54;
        if (textW < 0) textW = 0;
        const int textX = columns.player.x + 54;
        const int textTop = rect.center().y() - 20;
        drawElide(painter, QRect(textX, textTop, textW, 22), row.named ? row.name : row.sub, row.named ? nameFont_ : cardFont_,
                  QColor(kText), Qt::AlignLeft | Qt::AlignVCenter);
        int subW = textW;
        if (!session && row.inSession) {
            const int badgeW = QFontMetrics(subStrong_).horizontalAdvance(QStringLiteral("This session")) + 8;
            subW = qMax(0, textW - badgeW);
            drawElide(painter, QRect(textX + subW, textTop + 20, badgeW, 18), QStringLiteral("This session"), subStrong_,
                      QColor("#86EFAC"), Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (row.named) {
            drawElide(painter, QRect(textX, textTop + 20, subW, 18), row.sub, smallMono_, QColor(kFaint),
                      Qt::AlignLeft | Qt::AlignVCenter);
        } else {
            drawElide(painter, QRect(textX, textTop + 20, subW, 18), QStringLiteral("New player · no details"), subFont_,
                      QColor(kMuted), Qt::AlignLeft | Qt::AlignVCenter);
        }

        const auto field = [&](const Bands& band, const QString& text, bool missing, const QFont& font) {
            drawElide(painter, QRect(band.x, rect.top(), band.w, rect.height()), missing ? QStringLiteral("—") : text, font,
                      QColor(missing ? kFaint : kSoftText), Qt::AlignLeft | Qt::AlignVCenter);
        };
        if (session) {
            paintStatus(painter, rect, columns.status, row);
            drawElide(painter, QRect(columns.bought.x, rect.top(), columns.bought.w, rect.height()), row.bought, moneyFont_,
                      row.boughtColor, Qt::AlignRight | Qt::AlignVCenter);
            drawElide(painter, QRect(columns.cashed.x, rect.top(), columns.cashed.w, rect.height()), row.cashed, moneyFont_,
                      row.cashedColor, Qt::AlignRight | Qt::AlignVCenter);
        } else {
            field(columns.student, row.student, row.studentMissing, studentFont_);
            field(columns.degree, row.degree, row.degreeMissing, bodyFont_);
            field(columns.year, row.year, row.yearMissing, bodyFont_);
        }
        paintMembership(painter, rect, columns.member, row);
        paintArc(painter, rect, columns.arc, row);
        paintAge(painter, rect, columns.age, row);
    }

    void paintStatus(QPainter& painter, const QRect& rect, const Bands& band, const MemberRow& row) const {
        if (row.status == NightStatus::Locked) {
            painter.drawPixmap(band.x, rect.center().y() - 8, lockIcon_);
            drawElide(painter, QRect(band.x + 24, rect.top(), band.w - 24, rect.height()), QStringLiteral("Locked"), bodyStrong_,
                      QColor("#F28B82"), Qt::AlignLeft | Qt::AlignVCenter);
            return;
        }
        if (row.status == NightStatus::Playing) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#4ADE80"));
            painter.drawEllipse(QPointF(band.x + 4.5, rect.center().y()), 4.5, 4.5);
            drawElide(painter, QRect(band.x + 18, rect.top(), band.w - 18, rect.height()), QStringLiteral("Playing"), bodyStrong_,
                      QColor("#86EFAC"), Qt::AlignLeft | Qt::AlignVCenter);
            return;
        }
        drawElide(painter, QRect(band.x, rect.center().y() - 18, band.w, 20), QStringLiteral("Cashed out"), bodyStrong_,
                  QColor(kSoftText), Qt::AlignLeft | Qt::AlignVCenter);
        drawElide(painter, QRect(band.x, rect.center().y(), band.w, 16), row.cashedWhen.isEmpty() ? QStringLiteral("—") : row.cashedWhen,
                  smallMono_, QColor(kFaint), Qt::AlignLeft | Qt::AlignVCenter);
    }

    void paintMembership(QPainter& painter, const QRect& rect, const Bands& band, const MemberRow& row) const {
        if (row.membership.isEmpty()) {
            drawElide(painter, QRect(band.x, rect.top(), band.w, rect.height()), QStringLiteral("—"), bodyFont_, QColor(kFaint),
                      Qt::AlignLeft | Qt::AlignVCenter);
            return;
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(row.tier);
        painter.drawEllipse(QPointF(band.x + 5, rect.center().y()), 5, 5);
        drawElide(painter, QRect(band.x + 18, rect.top(), qMax(0, band.w - 18), rect.height()), row.membership, bodyFont_,
                  QColor("#DCE3EA"), Qt::AlignLeft | Qt::AlignVCenter);
    }

    void paintArc(QPainter& painter, const QRect& rect, const Bands& band, const MemberRow& row) const {
        if (row.arc == 1) {
            paintPill(painter, band.x, rect.center().y() - 14, badgeIcon_, QStringLiteral("ARC"), pillFont_, QColor("#86EFAC"),
                      QColor(74, 222, 128, 36));
            return;
        }
        drawElide(painter, QRect(band.x, rect.top(), band.w, rect.height()), row.arc == 2 ? QStringLiteral("No") : QStringLiteral("—"),
                  bodyFont_, QColor(row.arc == 2 ? kMuted : kFaint), Qt::AlignLeft | Qt::AlignVCenter);
    }

    void paintAge(QPainter& painter, const QRect& rect, const Bands& band, const MemberRow& row) const {
        if (row.minor) {
            paintPill(painter, band.x, rect.center().y() - 14, banIcon_, QStringLiteral("Under 18"), ageFont_, QColor("#F28B82"),
                      QColor(242, 139, 130, 36));
            return;
        }
        drawElide(painter, QRect(band.x, rect.top(), band.w, rect.height()), QStringLiteral("18+"), bodyFont_, QColor(kMuted),
                  Qt::AlignLeft | Qt::AlignVCenter);
    }

    void paintPill(QPainter& painter, int x, int y, const QPixmap& icon, const QString& text, const QFont& font, const QColor& fg,
                   const QColor& bg) const {
        const int textW = QFontMetrics(font).horizontalAdvance(text);
        const int width = 8 + icon.width() + 4 + textW + 10;
        const QRect box(x, y, width, 28);
        painter.setPen(Qt::NoPen);
        painter.setBrush(bg);
        painter.drawRoundedRect(box, 14, 14);
        painter.drawPixmap(box.left() + 8, box.center().y() - icon.height() / 2, icon);
        painter.setFont(font);
        painter.setPen(fg);
        painter.drawText(QRect(box.left() + 8 + icon.width() + 4, box.top(), textW + 4, box.height()), Qt::AlignVCenter | Qt::AlignLeft,
                         text);
    }

    QFont nameFont_;
    QFont subFont_;
    QFont subStrong_;
    QFont bodyFont_;
    QFont bodyStrong_;
    QFont moneyFont_;
    QFont smallMono_;
    QFont studentFont_;
    QFont cardFont_;
    QFont initialsFont_;
    QFont pillFont_;
    QFont ageFont_;
    QFont capsFont_;
    QFont countFont_;
    QPixmap userIcon_;
    QPixmap lockIcon_;
    QPixmap badgeIcon_;
    QPixmap banIcon_;
};

PlayersScreen::PlayersScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    setObjectName(QStringLiteral("players"));
    setAttribute(Qt::WA_StyledBackground);
    setFont(ui(18, QFont::Normal));
    setStyleSheet(QStringLiteral(
        "QWidget#players { background:%1; }"
        "QLabel { background: transparent; border: none; }"
        "QWidget#players QPushButton { min-height: 0px; min-width: 0px; padding: 0px; }"
        "#playerRow { background:transparent; border:none; border-bottom:1px solid %2; }"
        "#playerRow[on=\"true\"] { background:rgba(159,162,229,0.13); }")
                      .arg(kBg, kDivider));
    buildUi();
}

void PlayersScreen::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto* bar = new QFrame;
    bar->setObjectName(QStringLiteral("playersBar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(72);
    bar->setStyleSheet(QStringLiteral(
        "#playersBar { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* barRow = new QHBoxLayout(bar);
    barRow->setContentsMargins(10, 0, 10, 0);
    barRow->setSpacing(16);
    auto* back = iconButton(QStringLiteral("playersBack"), QStringLiteral("arrow-left"), 24, QColor(kIcon),
                            QStringLiteral("Back"), ui(20, QFont::DemiBold), 52,
                            QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 20px 0 14px;")
                                .arg(kRaised, kText, kBorderRaised));
    connect(back, &QPushButton::clicked, this, [this] { emit closed(changed_); });
    barRow->addWidget(back);
    auto* divider = new QFrame;
    divider->setObjectName(QStringLiteral("barDivider"));
    divider->setFixedSize(1, 40);
    divider->setStyleSheet(QStringLiteral("#barDivider { background:%1; border:none; }").arg(kBorderRaised));
    barRow->addWidget(divider);
    barRow->addWidget(txt(QStringLiteral("Players"), ui(30, QFont::Bold), kText));

    auto* view = new QFrame;
    view->setObjectName(QStringLiteral("viewSwitch"));
    view->setAttribute(Qt::WA_StyledBackground);
    view->setFixedHeight(56);
    view->setStyleSheet(QStringLiteral(
        "#viewSwitch { background:%1; border:1px solid %2; border-radius:15px; }").arg(kInset, kBorder));
    auto* viewRow = new QHBoxLayout(view);
    viewRow->setContentsMargins(4, 4, 4, 4);
    viewRow->setSpacing(4);
    const auto segment = [&](TapFrame*& frame, QLabel*& label, QLabel*& count, const QString& words) {
        frame = new TapFrame;
        frame->setObjectName(QStringLiteral("viewSeg"));
        frame->setFixedHeight(46);
        auto* row = new QHBoxLayout(frame);
        row->setContentsMargins(20, 0, 20, 0);
        row->setSpacing(10);
        label = txt(words, ui(20, QFont::DemiBold), kSoftText);
        count = txt(QStringLiteral("0"), mono(16, QFont::Medium), kFaint);
        row->addWidget(label);
        row->addWidget(count);
        passClicks(frame);
        viewRow->addWidget(frame);
    };
    segment(sessionSeg_, sessionLab_, sessionNum_, QStringLiteral("This session"));
    segment(allSeg_, allLab_, allNum_, QStringLiteral("All members"));
    connect(sessionSeg_, &TapFrame::clicked, this, [this] { setSessionView(true); });
    connect(allSeg_, &TapFrame::clicked, this, [this] { setSessionView(false); });
    barRow->addSpacing(8);
    barRow->addWidget(view);
    barRow->addStretch(1);
    auto* both = iconButton(QStringLiteral("exportBoth"), QStringLiteral("download"), 22, QColor(kIcon),
                            QStringLiteral("Export session & members"), ui(20, QFont::DemiBold), 52,
                            QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 20px 0 16px;")
                                .arg(kRaised, kText, kBorderRaised));
    connect(both, &QPushButton::clicked, this, [this] { exportBoth(); });
    barRow->addWidget(both);
    auto* clock = new QFrame;
    clock->setObjectName(QStringLiteral("playersClock"));
    clock->setAttribute(Qt::WA_StyledBackground);
    clock->setFixedHeight(52);
    clock->setStyleSheet(QStringLiteral(
        "#playersClock { background:%1; border:1px solid %2; border-radius:14px; }").arg(kRaised, kBorderRaised));
    auto* clockRow = new QHBoxLayout(clock);
    clockRow->setContentsMargins(18, 0, 18, 0);
    clockRow->setSpacing(6);
    clockTime_ = txt(QStringLiteral("12:00"), mono(30, QFont::DemiBold), kText);
    QFont clockFont = clockTime_->font();
    clockFont.setLetterSpacing(QFont::AbsoluteSpacing, -0.6);
    clockTime_->setFont(clockFont);
    clockPeriod_ = txt(QStringLiteral("AM"), ui(18, QFont::DemiBold), kMuted);
    clockRow->addWidget(clockTime_, 0, Qt::AlignBaseline);
    clockRow->addWidget(clockPeriod_, 0, Qt::AlignBaseline);
    barRow->addWidget(clock);
    root->addWidget(bar);

    auto* body = new QHBoxLayout;
    body->setSpacing(16);
    auto* listPanel = new QFrame;
    listPanel->setObjectName(QStringLiteral("listPanel"));
    listPanel->setAttribute(Qt::WA_StyledBackground);
    listPanel->setStyleSheet(QStringLiteral(
        "#listPanel { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* listColumn = new QVBoxLayout(listPanel);
    listColumn->setContentsMargins(0, 0, 0, 0);
    listColumn->setSpacing(0);

    auto* toolbar = new QFrame;
    toolbar->setObjectName(QStringLiteral("playerToolbar"));
    toolbar->setFixedHeight(88);
    toolbar->setStyleSheet(QStringLiteral(
        "#playerToolbar { background:transparent; border:none; border-bottom:1px solid %1; }").arg(kDivider));
    auto* tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(14, 0, 14, 0);
    tools->setSpacing(12);
    auto* searchBox = new QFrame;
    searchBox->setObjectName(QStringLiteral("searchBox"));
    searchBox->setAttribute(Qt::WA_StyledBackground);
    searchBox->setFixedHeight(60);
    searchBox->setStyleSheet(QStringLiteral(
        "#searchBox { background:%1; border:1px solid %2; border-radius:14px; }").arg(kInset, kBorderRaised));
    auto* searchRow = new QHBoxLayout(searchBox);
    searchRow->setContentsMargins(18, 0, 8, 0);
    searchRow->setSpacing(10);
    searchRow->addWidget(iconLabel(QStringLiteral("search"), 24, QColor(kMuted)));
    search_ = new QLineEdit;
    search_->setObjectName(QStringLiteral("searchEdit"));
    search_->setFont(ui(21, QFont::Normal));
    search_->setPlaceholderText(QStringLiteral("Search by name, card number, zID or email"));
    search_->setFrame(false);
    search_->setStyleSheet(QStringLiteral(
        "#searchEdit { background:%1; color:%2; border:none; padding:0; min-height:0px; }").arg(kInset, kText));
    darkLine(search_, kInset);
    searchRow->addWidget(search_, 1);
    scanHint_ = new QWidget;
    auto* hintRow = new QHBoxLayout(scanHint_);
    hintRow->setContentsMargins(0, 0, 10, 0);
    hintRow->setSpacing(8);
    hintRow->addWidget(iconLabel(QStringLiteral("scan-barcode"), 20, QColor(kFaint)));
    hintRow->addWidget(txt(QStringLiteral("or scan a card"), ui(17, QFont::Normal), kFaint));
    searchRow->addWidget(scanHint_);
    clearSearch_ = iconButton(QStringLiteral("clearSearch"), QStringLiteral("x"), 20, QColor(kIcon), QString(),
                              ui(16, QFont::Normal), 44,
                              QStringLiteral("background:%1; border:1px solid %2; border-radius:12px;").arg(kRaised, kBorderButton));
    clearSearch_->setFixedSize(44, 44);
    clearSearch_->hide();
    connect(clearSearch_, &QPushButton::clicked, this, [this] { search_->clear(); search_->setFocus(); });
    searchRow->addWidget(clearSearch_);
    tools->addWidget(searchBox, 1);
    connect(search_, &QLineEdit::textChanged, this, [this] { applyListFilter(); });

    const QString toolRules = QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 22px 0 18px;")
                                  .arg(kRaised, kText, kBorderButton);
    sessionTools_ = new QWidget;
    auto* sessionToolRow = new QHBoxLayout(sessionTools_);
    sessionToolRow->setContentsMargins(0, 0, 0, 0);
    sessionToolRow->setSpacing(0);
    auto* exportSession = iconButton(QStringLiteral("exportSession"), QStringLiteral("download"), 22, QColor(kIcon),
                                     QStringLiteral("Export session list"), ui(20, QFont::DemiBold), 60, toolRules);
    connect(exportSession, &QPushButton::clicked, this, [this] { exportSessionList(); });
    sessionToolRow->addWidget(exportSession);
    tools->addWidget(sessionTools_);
    memberTools_ = new QWidget;
    auto* memberToolRow = new QHBoxLayout(memberTools_);
    memberToolRow->setContentsMargins(0, 0, 0, 0);
    memberToolRow->setSpacing(12);
    auto* import = iconButton(QStringLiteral("importPlayers"), QStringLiteral("upload"), 22, QColor(kIcon),
                              QStringLiteral("Import"), ui(20, QFont::DemiBold), 60, toolRules);
    auto* exportAll = iconButton(QStringLiteral("exportAll"), QStringLiteral("download"), 22, QColor(kIcon),
                                 QStringLiteral("Export all"), ui(20, QFont::DemiBold), 60, toolRules);
    connect(import, &QPushButton::clicked, this, [this] { beginImport(); });
    connect(exportAll, &QPushButton::clicked, this, [this] { exportAllMembers(); });
    memberToolRow->addWidget(import);
    memberToolRow->addWidget(exportAll);
    memberTools_->hide();
    tools->addWidget(memberTools_);
    listColumn->addWidget(toolbar);

    filterRow_ = new QFrame;
    filterRow_->setObjectName(QStringLiteral("filterRow"));
    filterRow_->setFixedHeight(68);
    filterRow_->setStyleSheet(QStringLiteral(
        "#filterRow { background:transparent; border:none; border-bottom:1px solid %1; }").arg(kDivider));
    auto* filters = new QHBoxLayout(filterRow_);
    filters->setContentsMargins(24, 0, 14, 0);
    filters->setSpacing(8);
    const char* filterNames[] = {"All", "Playing", "Cashed out", "Under 18"};
    for (int i = 0; i < 4; ++i) {
        auto* chip = new TapFrame;
        chip->setObjectName(QStringLiteral("playerFilter"));
        chip->setFixedHeight(44);
        auto* row = new QHBoxLayout(chip);
        row->setContentsMargins(16, 0, 14, 0);
        row->setSpacing(8);
        filterLab_[i] = txt(QString::fromUtf8(filterNames[i]), ui(17, QFont::DemiBold), kSoftText);
        filterNum_[i] = txt(QStringLiteral("0"), mono(14, QFont::Medium), kFaint);
        row->addWidget(filterLab_[i]);
        row->addWidget(filterNum_[i]);
        passClicks(chip);
        connect(chip, &TapFrame::clicked, this, [this, i] { setFilter(i); });
        filters->addWidget(chip);
        filterChip_[i] = chip;
    }
    filters->addStretch(1);
    caption_ = txt(QString(), ui(17, QFont::Normal), kMuted);
    filters->addWidget(caption_);
    listColumn->addWidget(filterRow_);

    auto* header = new QFrame;
    header->setObjectName(QStringLiteral("columnHeader"));
    header->setFixedHeight(44);
    header->setStyleSheet(QStringLiteral(
        "#columnHeader { background:%1; border:none; border-bottom:1px solid %2; }").arg(kStrip, kDivider));
    headerRow_ = new QHBoxLayout(header);
    headerRow_->setContentsMargins(24, 0, 24, 0);
    headerRow_->setSpacing(16);
    listColumn->addWidget(header);

    listView_ = new QListView;
    listView_->setObjectName(QStringLiteral("playerList"));
    listView_->setFrameShape(QFrame::NoFrame);
    listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView_->setSelectionMode(QAbstractItemView::NoSelection);
    listView_->setFocusPolicy(Qt::NoFocus);
    listView_->setUniformItemSizes(false);
    listView_->setSpacing(0);
    listView_->setStyleSheet(QStringLiteral(
        "QListView { background:%1; border:none; outline:none; }"
        "QListView::item { background:transparent; border:none; }").arg(kPanel));
    QPalette listPal = listView_->palette();
    listPal.setColor(QPalette::Base, QColor(kPanel));
    listPal.setColor(QPalette::Window, QColor(kPanel));
    listView_->setPalette(listPal);
    listView_->viewport()->setPalette(listPal);
    listView_->viewport()->setAutoFillBackground(true);
    listModel_ = new PlayerListModel(listView_);
    listView_->setModel(listModel_);
    listView_->setItemDelegate(new PlayerDelegate(listView_));
    connect(listView_, &QListView::clicked, this, [this](const QModelIndex& index) {
        const std::string id = listModel_->idAt(index.row());
        if (!id.empty()) selectPlayer(id);
    });
    QScroller::grabGesture(listView_->viewport(), QScroller::LeftMouseButtonGesture);
    listView_->viewport()->installEventFilter(this);
    emptyList_ = new QWidget(listView_->viewport());
    emptyList_->setObjectName(QStringLiteral("emptyPlayers"));
    emptyList_->setStyleSheet(QStringLiteral("#emptyPlayers { background:transparent; }"));
    auto* listEmpty = new QVBoxLayout(emptyList_);
    listEmpty->setSpacing(8);
    listEmpty->addStretch(1);
    listEmpty->addWidget(txt(QStringLiteral("No players match"), ui(26, QFont::Bold), kText), 0, Qt::AlignHCenter);
    auto* hint = txt(QStringLiteral("Try a name, card number, zID or email, or switch the filter back to All."),
                     ui(19, QFont::Normal), kMuted, Qt::AlignHCenter);
    hint->setWordWrap(true);
    hint->setFixedWidth(520);
    listEmpty->addWidget(hint, 0, Qt::AlignHCenter);
    listEmpty->addStretch(1);
    emptyList_->hide();
    listColumn->addWidget(listView_, 1);

    toastWrap_ = new QWidget;
    auto* toastOuter = new QVBoxLayout(toastWrap_);
    toastOuter->setContentsMargins(12, 0, 12, 12);
    auto* toast = new QFrame;
    toast->setObjectName(QStringLiteral("playerToast"));
    toast->setAttribute(Qt::WA_StyledBackground);
    toast->setFixedHeight(68);
    toast->setStyleSheet(QStringLiteral(
        "#playerToast { background:#15241B; border:1px solid #24402D; border-radius:14px; }"));
    auto* toastRow = new QHBoxLayout(toast);
    toastRow->setContentsMargins(16, 0, 10, 0);
    toastRow->setSpacing(14);
    auto* badge = new QLabel;
    badge->setObjectName(QStringLiteral("toastBadge"));
    badge->setAttribute(Qt::WA_StyledBackground);
    badge->setFixedSize(34, 34);
    badge->setAlignment(Qt::AlignCenter);
    badge->setPixmap(iconPx(QStringLiteral("check"), 20, QColor("#052E16")));
    badge->setStyleSheet(QStringLiteral("#toastBadge { background:#4ADE80; border-radius:17px; }"));
    toastRow->addWidget(badge);
    toastTitle_ = txt(QString(), ui(19, QFont::Bold), kText);
    toastDetail_ = txt(QString(), ui(19, QFont::Normal), QStringLiteral("#B7C7BE"));
    toastRow->addWidget(toastTitle_);
    toastRow->addWidget(toastDetail_, 1);
    auto* dismiss = iconButton(QStringLiteral("toastDismiss"), QStringLiteral("x"), 22, QColor(kSoftText), QString(),
                               ui(16, QFont::Normal), 48, QStringLiteral("background:transparent; border:none;"));
    dismiss->setFixedSize(48, 48);
    connect(dismiss, &QPushButton::clicked, toastWrap_, &QWidget::hide);
    toastRow->addWidget(dismiss);
    toastOuter->addWidget(toast);
    toastWrap_->hide();
    listColumn->addWidget(toastWrap_);
    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    toastTimer_->setInterval(7000);
    connect(toastTimer_, &QTimer::timeout, toastWrap_, &QWidget::hide);
    body->addWidget(listPanel, 1);

    detailPanel_ = new QFrame;
    detailPanel_->setObjectName(QStringLiteral("detailPanel"));
    detailPanel_->setAttribute(Qt::WA_StyledBackground);
    detailPanel_->setFixedWidth(560);
    auto* detailColumn = new QVBoxLayout(detailPanel_);
    detailColumn->setContentsMargins(0, 0, 0, 0);
    detailStack_ = new QStackedWidget;
    auto* empty = new QWidget;
    auto* emptyColumn = new QVBoxLayout(empty);
    emptyColumn->setAlignment(Qt::AlignCenter);
    emptyColumn->setSpacing(14);
    auto* usersBox = new QLabel;
    usersBox->setObjectName(QStringLiteral("usersBox"));
    usersBox->setAttribute(Qt::WA_StyledBackground);
    usersBox->setFixedSize(88, 88);
    usersBox->setAlignment(Qt::AlignCenter);
    usersBox->setPixmap(iconPx(QStringLiteral("users"), 40, QColor(kIcon)));
    usersBox->setStyleSheet(QStringLiteral("#usersBox { background:%1; border:1px solid #2C3540; border-radius:22px; }").arg(kRaised));
    emptyColumn->addWidget(usersBox, 0, Qt::AlignHCenter);
    emptyColumn->addWidget(txt(QStringLiteral("Tap a player"), ui(34, QFont::Bold), kText), 0, Qt::AlignHCenter);
    auto* emptyHint = txt(QStringLiteral("Or scan their card. Their details and this session's buy-ins show here."),
                          ui(20, QFont::Normal), kMuted, Qt::AlignHCenter);
    emptyHint->setWordWrap(true);
    emptyHint->setFixedWidth(380);
    emptyHint->setAlignment(Qt::AlignHCenter);
    emptyColumn->addWidget(emptyHint, 0, Qt::AlignHCenter);
    detailStack_->addWidget(empty);
    auto* selected = new QWidget;
    detailLayout_ = new QVBoxLayout(selected);
    detailLayout_->setContentsMargins(0, 0, 0, 0);
    detailLayout_->setSpacing(0);
    detailStack_->addWidget(selected);
    detailColumn->addWidget(detailStack_);
    body->addWidget(detailPanel_);
    root->addLayout(body, 1);

    scanner_ = new QLineEdit(this);
    scanner_->setFixedSize(1, 1);
    scanner_->move(-40, -40);
    connect(scanner_, &QLineEdit::returnPressed, this, [this] {
        const QString id = scanner_->text().trimmed();
        scanner_->clear();
        if (!overlay_ && !id.isEmpty()) onScan(id);
    });
    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, [this] {
        const QTime now = QTime::currentTime();
        clockTime_->setText(now.toString(QStringLiteral("h:mm")));
        clockPeriod_->setText(now.toString(QStringLiteral("AP")));
    });
    clockTimer_->start(1000);
    clockTime_->setText(QTime::currentTime().toString(QStringLiteral("h:mm")));
    clockPeriod_->setText(QTime::currentTime().toString(QStringLiteral("AP")));
}

void PlayersScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
    focusScanner();
}

void PlayersScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    closeOverlay();
}

void PlayersScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (emptyList_ && listView_) emptyList_->setGeometry(listView_->viewport()->rect());
    if (!dim_ || !dim_->parentWidget()) return;
    dim_->setGeometry(dim_->parentWidget()->rect());
    if (dialog_) {
        dialog_->move((dim_->width() - dialog_->width()) / 2, (dim_->height() - dialog_->height()) / 2);
    }
}

bool PlayersScreen::eventFilter(QObject* watched, QEvent* event) {
    if (listView_ && watched == listView_->viewport() && event->type() == QEvent::Resize && emptyList_) {
        emptyList_->setGeometry(listView_->viewport()->rect());
    }
    return Screen::eventFilter(watched, event);
}

void PlayersScreen::focusScanner() {
    if (overlay_ || !scanner_) return;
    if (QWidget* focused = QApplication::focusWidget()) {
        if (qobject_cast<QLineEdit*>(focused) && focused != scanner_) return;
    }
    scanner_->setFocus(Qt::OtherFocusReason);
}

void PlayersScreen::refresh() {
    nightsReady_ = false;
    updateChrome();
    rebuildHeader();
    rebuildList();
    rebuildDetails();
}

void PlayersScreen::updateChrome() {
    primeNights();
    const bool sessionOn = sessionView_;
    const auto paintSeg = [](TapFrame* frame, QLabel* label, QLabel* count, bool on) {
        frame->setStyleSheet(on ? QStringLiteral("#viewSeg { background:%1; border:none; border-radius:11px; }").arg(kText)
                                : QStringLiteral("#viewSeg { background:transparent; border:none; border-radius:11px; }"));
        label->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(on ? kBg : kSoftText));
        count->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(on ? "#4B5560" : kFaint));
    };
    paintSeg(sessionSeg_, sessionLab_, sessionNum_, sessionOn);
    paintSeg(allSeg_, allLab_, allNum_, !sessionOn);
    sessionTools_->setVisible(sessionOn);
    memberTools_->setVisible(!sessionOn);
    filterRow_->setVisible(sessionOn);

    int sessionCount = 0;
    int playing = 0;
    int cashed = 0;
    int minors = 0;
    std::set<std::string> sessionIds;
    for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
    for (const std::string& id : sessionIds) {
        const Player* player = ctx_.players().find(id);
        if (!player) continue;
        ++sessionCount;
        const Night& night = nightOf(id);
        if (player->isUnderage()) ++minors;
        else if (night.status == NightStatus::Cashed) ++cashed;
        else ++playing;
    }
    const int counts[] = {sessionCount, playing, cashed, minors};
    sessionNum_->setText(QString::number(sessionCount));
    allNum_->setText(QString::number(static_cast<int>(ctx_.players().count())));
    for (int i = 0; i < 4; ++i) {
        const bool on = sessionOn && i == filter_;
        filterChip_[i]->setStyleSheet(on
            ? QStringLiteral("#playerFilter { background:%1; border:1px solid %1; border-radius:12px; }").arg(kText)
            : QStringLiteral("#playerFilter { background:%1; border:1px solid %2; border-radius:12px; }").arg(kRaised, kBorderRaised));
        filterLab_[i]->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(on ? kBg : kSoftText));
        filterNum_[i]->setText(QString::number(counts[i]));
        filterNum_[i]->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(on ? "#4B5560" : kFaint));
    }
}

void PlayersScreen::setSessionView(bool session) {
    if (sessionView_ == session && filter_ == 0) return;
    sessionView_ = session;
    filter_ = 0;
    updateChrome();
    rebuildHeader();
    rebuildList();
}

void PlayersScreen::setFilter(int filter) {
    filter_ = filter;
    updateChrome();
    applyListFilter();
}

void PlayersScreen::rebuildHeader() {
    wipe(headerRow_);
    auto* playerHead = new QWidget;
    auto* playerRow = new QHBoxLayout(playerHead);
    playerRow->setContentsMargins(54, 0, 0, 0);
    playerRow->setSpacing(6);
    playerRow->addWidget(caps(QStringLiteral("PLAYER"), 12));
    playerRow->addWidget(iconLabel(QStringLiteral("arrow-down"), 14, QColor(kFaint)));
    playerRow->addWidget(txt(QStringLiteral("A–Z"), ui(12, QFont::Bold), kFaint));
    playerRow->addStretch(1);
    stretchColumn(playerHead);
    headerRow_->addWidget(playerHead, sessionView_ ? 1 : 10);
    const auto fixed = [&](const QString& label, int width, Qt::Alignment align) {
        auto* cell = caps(label, 12);
        cell->setAlignment(align);
        pinColumn(cell, width);
        headerRow_->addWidget(cell);
    };
    if (sessionView_) {
        fixed(QStringLiteral("STATUS"), 150, Qt::AlignLeft | Qt::AlignVCenter);
        fixed(QStringLiteral("BOUGHT IN"), 120, Qt::AlignRight | Qt::AlignVCenter);
        fixed(QStringLiteral("CASHED OUT"), 120, Qt::AlignRight | Qt::AlignVCenter);
    } else {
        fixed(QStringLiteral("STUDENT #"), 130, Qt::AlignLeft | Qt::AlignVCenter);
        auto* degree = caps(QStringLiteral("DEGREE"), 12);
        stretchColumn(degree);
        headerRow_->addWidget(degree, 9);
        fixed(QStringLiteral("YEAR"), 120, Qt::AlignLeft | Qt::AlignVCenter);
    }
    fixed(QStringLiteral("MEMBERSHIP"), 140, Qt::AlignLeft | Qt::AlignVCenter);
    fixed(QStringLiteral("ARC"), 96, Qt::AlignLeft | Qt::AlignVCenter);
    fixed(QStringLiteral("AGE"), 120, Qt::AlignLeft | Qt::AlignVCenter);
}

void PlayersScreen::primeNights() {
    if (nightsReady_) return;
    std::unordered_map<std::string, NightWork> work;
    std::unordered_map<std::string, bool> under;
    for (const auto& scan : ctx_.players().attendance().firstScans()) {
        const Player* player = ctx_.players().find(scan.playerId);
        if (!player || work.count(player->id())) continue;
        work.emplace(player->id(), NightWork{});
        under.emplace(player->id(), player->isUnderage());
    }
    if (!work.empty()) {
        for (const auto& transaction : ctx_.log().entries()) {
            auto it = work.find(transaction->playerId());
            if (it == work.end()) continue;
            absorb(it->second, *transaction);
        }
    }
    gNights.clear();
    for (auto& entry : work) gNights.emplace(entry.first, finishNight(std::move(entry.second), under[entry.first]));
    nightsReady_ = true;
}

void PlayersScreen::rebuildList() {
    primeNights();
    std::set<std::string> sessionIds;
    for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
    std::vector<const Player*> players;
    if (sessionView_) {
        players.reserve(sessionIds.size());
        for (const std::string& id : sessionIds) {
            if (const Player* player = ctx_.players().find(id)) players.push_back(player);
        }
    } else {
        players = ctx_.players().all();
    }
    const auto byName = [](const Player* a, const Player* b) {
        const bool namedA = realName(*a);
        const bool namedB = realName(*b);
        if (namedA != namedB) return namedA;
        if (namedA) {
            const int name = QString::fromStdString(a->name()).compare(QString::fromStdString(b->name()), Qt::CaseInsensitive);
            if (name != 0) return name < 0;
        }
        return a->id() < b->id();
    };
    std::sort(players.begin(), players.end(), byName);

    std::vector<const Player*> loaded;
    std::set<std::string> loadedIds;
    if (sessionView_) {
        for (const std::string& id : ctx_.loadedPlayers()) {
            const auto it = std::find_if(players.begin(), players.end(),
                                         [&](const Player* player) { return player->id() == id; });
            if (it == players.end() || loadedIds.count(id)) continue;
            loaded.push_back(*it);
            loadedIds.insert(id);
        }
    }

    std::vector<MemberRow> rows;
    rows.reserve(players.size());
    const auto makeRow = [&](const Player* player, bool isLoaded) {
        MemberRow row;
        row.id = player->id();
        row.named = realName(*player);
        row.name = toQ(player->name());
        row.sub = toQ(player->id());
        row.loaded = isLoaded;
        row.minor = player->isUnderage();
        row.inSession = sessionIds.count(player->id()) > 0;
        row.student = toQ(player->studentNumber());
        row.degree = toQ(player->degree());
        row.year = toQ(player->studyYear());
        row.studentMissing = !hasDetail(row.student);
        row.degreeMissing = !hasDetail(row.degree);
        row.yearMissing = !hasDetail(row.year);
        if (player->membership() != MembershipType::None) {
            row.membership = QString::fromLatin1(membershipTypeLabel(player->membership()));
            row.tier = QColor(tierColor(player->membership()));
        }
        if (player->arcStatus() == ArcStatus::Arc) row.arc = 1;
        else if (player->arcStatus() == ArcStatus::NonArc) row.arc = 2;
        if (sessionView_) {
            const Night& night = nightOf(player->id());
            row.status = night.status;
            row.bought = moneyOrDash(night.bought);
            row.cashed = moneyOrDash(night.cashed);
            row.boughtColor = QColor(night.bought.cents() == 0 ? kFaint : kCashIn);
            row.cashedColor = QColor(night.cashed.cents() == 0 ? kFaint : kCashOut);
            row.cashedWhen = night.hasOut ? QStringLiteral("at %1").arg(clock12(night.lastOut)) : QString();
        }
        row.hay = (row.name + QLatin1Char('\n') + row.sub + QLatin1Char('\n') + row.student + QLatin1Char('\n') + toQ(player->email()))
                      .toCaseFolded();
        rows.push_back(std::move(row));
    };
    for (const Player* player : loaded) makeRow(player, true);
    for (const Player* player : players) {
        if (!loadedIds.count(player->id())) makeRow(player, false);
    }
    listModel_->setRows(std::move(rows), sessionView_);
    applyListFilter();
}

void PlayersScreen::applyListFilter() {
    const QString query = search_ ? search_->text().trimmed() : QString();
    if (scanHint_) scanHint_->setVisible(query.isEmpty());
    if (clearSearch_) clearSearch_->setVisible(!query.isEmpty());
    if (listModel_) listModel_->refilter(query, filter_);
    const int shown = listModel_ ? listModel_->matchCount() : 0;
    if (emptyList_) {
        emptyList_->setVisible(shown == 0);
        if (listView_) emptyList_->setGeometry(listView_->viewport()->rect());
        if (shown == 0) emptyList_->raise();
    }
    if (sessionView_ && caption_) {
        if (!query.isEmpty()) {
            caption_->setText(shown == 1 ? QStringLiteral("1 player matches")
                                         : QStringLiteral("%1 players match").arg(shown));
        } else {
            caption_->setText(QStringLiteral("%1 players this session").arg(shown));
        }
    }
}

void PlayersScreen::selectPlayer(const std::string& id) {
    selected_ = id;
    if (listModel_) listModel_->setSelected(id);
    rebuildDetails();
}

void PlayersScreen::clearSelection() {
    selected_.clear();
    selectPlayer({});
}

void PlayersScreen::rebuildDetails() {
    const Player* player = selected_.empty() ? nullptr : ctx_.players().find(selected_);
    if (!player) selected_.clear();
    const bool minor = player && player->isUnderage();
    detailPanel_->setStyleSheet(QStringLiteral(
        "#detailPanel { background:%1; border:%2px solid %3; border-radius:18px; }")
                                    .arg(kPanel)
                                    .arg(player ? 2 : 1)
                                    .arg(!player ? kBorder : (minor ? "#F28B82" : kCashIn)));
    detailStack_->setCurrentIndex(player ? 1 : 0);
    wipe(detailLayout_);
    if (!player) return;

    const bool named = realName(*player);
    std::set<std::string> sessionIds;
    for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
    const bool inSession = sessionIds.count(player->id()) > 0;
    const Night night = nightFor(ctx_, *player);

    auto* header = new QWidget;
    auto* headerColumn = new QVBoxLayout(header);
    headerColumn->setContentsMargins(28, 24, 24, 22);
    headerColumn->setSpacing(18);
    auto* top = new QHBoxLayout;
    top->setSpacing(18);
    auto* avatar = new QLabel;
    avatar->setObjectName(QStringLiteral("detailAvatar"));
    avatar->setAttribute(Qt::WA_StyledBackground);
    avatar->setFixedSize(84, 84);
    avatar->setAlignment(Qt::AlignCenter);
    if (named) {
        avatar->setText(initialsOf(toQ(player->name())));
        avatar->setFont(ui(32, QFont::Bold));
        avatar->setStyleSheet(QStringLiteral("#detailAvatar { background:#262A40; color:#D9DBF7; border-radius:42px; }"));
    } else {
        avatar->setPixmap(iconPx(QStringLiteral("user"), 36, QColor(kFaint)));
        avatar->setStyleSheet(QStringLiteral(
            "#detailAvatar { background:transparent; border:1px dashed #3A424C; border-radius:42px; }"));
    }
    top->addWidget(avatar, 0, Qt::AlignTop);
    auto* title = new QVBoxLayout;
    title->setSpacing(4);
    title->addWidget(new ElideLabel(named ? toQ(player->name()) : QStringLiteral("New player"), ui(36, QFont::Bold), kText));
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(8);
    cardRow->setContentsMargins(0, 0, 0, 0);
    cardRow->addWidget(iconLabel(QStringLiteral("scan-barcode"), 18, QColor(kMuted)));
    cardRow->addWidget(txt(toQ(player->id()), mono(17, QFont::Normal), kMuted));
    if (inSession) {
        cardRow->addWidget(dot(8, QStringLiteral("#4ADE80")));
        cardRow->addWidget(txt(QStringLiteral("This session"), ui(16, QFont::DemiBold), QStringLiteral("#86EFAC")));
    }
    cardRow->addStretch(1);
    title->addLayout(cardRow);
    top->addLayout(title, 1);
    auto* close = iconButton(QStringLiteral("detailClose"), QStringLiteral("x"), 22, QColor(kIcon), QString(),
                             ui(16, QFont::Normal), 52,
                             QStringLiteral("background:%1; border:1px solid %2; border-radius:14px;").arg(kRaised, kBorderButton));
    close->setFixedSize(52, 52);
    connect(close, &QPushButton::clicked, this, [this] { clearSelection(); });
    top->addWidget(close, 0, Qt::AlignTop);
    headerColumn->addLayout(top);

    auto* badges = new QHBoxLayout;
    badges->setSpacing(10);
    auto* membership = new TapFrame;
    membership->setObjectName(QStringLiteral("detailMembership"));
    membership->setFixedHeight(44);
    const bool typed = player->membership() != MembershipType::None;
    membership->setStyleSheet(typed
        ? QStringLiteral("#detailMembership { background:#2A3038; border:1px solid #3D4651; border-radius:22px; }"
                         "#detailMembership[pressed=\"true\"] { background:#343B44; }")
        : QStringLiteral("#detailMembership { background:transparent; border:1px dashed #4A535E; border-radius:22px; }"
                         "#detailMembership[pressed=\"true\"] { background:#1D232A; }"));
    auto* memRow = new QHBoxLayout(membership);
    memRow->setContentsMargins(10, 0, 12, 0);
    memRow->setSpacing(8);
    memRow->addWidget(iconLabel(QStringLiteral("award"), 18, QColor(typed ? "#DCE3EA" : kMuted)));
    memRow->addWidget(txt(typed ? QStringLiteral("%1 member").arg(QString::fromLatin1(membershipTypeLabel(player->membership())))
                                : QStringLiteral("Set membership"),
                         ui(17, QFont::DemiBold), typed ? QStringLiteral("#DCE3EA") : kMuted));
    memRow->addWidget(iconLabel(QStringLiteral("pencil"), 16, QColor(kMuted)));
    passClicks(membership);
    connect(membership, &TapFrame::clicked, this, [this] { chooseMembership(); });
    badges->addWidget(membership);
    if (player->arcStatus() == ArcStatus::Arc) {
        auto* pill = new QFrame;
        pill->setObjectName(QStringLiteral("detailArc"));
        pill->setAttribute(Qt::WA_StyledBackground);
        pill->setFixedHeight(32);
        pill->setStyleSheet(QStringLiteral("#detailArc { background:rgba(74,222,128,0.14); border-radius:16px; }"));
        auto* row = new QHBoxLayout(pill);
        row->setContentsMargins(10, 0, 12, 0);
        row->setSpacing(6);
        row->addWidget(iconLabel(QStringLiteral("badge-check"), 17, QColor("#86EFAC")));
        row->addWidget(txt(QStringLiteral("ARC member"), ui(16, QFont::DemiBold), QStringLiteral("#86EFAC")));
        badges->addWidget(pill);
    } else if (player->arcStatus() == ArcStatus::NonArc) {
        auto* pill = new QFrame;
        pill->setObjectName(QStringLiteral("detailArc"));
        pill->setAttribute(Qt::WA_StyledBackground);
        pill->setFixedHeight(32);
        pill->setStyleSheet(QStringLiteral("#detailArc { background:transparent; border:1px solid #3A424C; border-radius:16px; }"));
        auto* row = new QHBoxLayout(pill);
        row->setContentsMargins(12, 0, 12, 0);
        row->addWidget(txt(QStringLiteral("Not ARC"), ui(16, QFont::DemiBold), kMuted));
        badges->addWidget(pill);
    }
    auto* age = new QFrame;
    age->setObjectName(QStringLiteral("detailAge"));
    age->setAttribute(Qt::WA_StyledBackground);
    age->setFixedHeight(32);
    age->setStyleSheet(minor
        ? QStringLiteral("#detailAge { background:rgba(242,139,130,0.14); border-radius:16px; }")
        : QStringLiteral("#detailAge { background:%1; border-radius:16px; }").arg(kBorder));
    auto* ageRow = new QHBoxLayout(age);
    ageRow->setContentsMargins(10, 0, 12, 0);
    ageRow->setSpacing(6);
    ageRow->addWidget(iconLabel(minor ? QStringLiteral("ban") : QStringLiteral("id-card"), 17,
                                QColor(minor ? "#F28B82" : kSoftText)));
    ageRow->addWidget(txt(minor ? QStringLiteral("Under 18") : QStringLiteral("18+"),
                          ui(16, minor ? QFont::Bold : QFont::DemiBold), minor ? QStringLiteral("#F28B82") : kSoftText));
    badges->addWidget(age);
    badges->addStretch(1);
    headerColumn->addLayout(badges);
    detailLayout_->addWidget(header);

    if (inSession && !minor) {
        auto* strip = new QFrame;
        strip->setObjectName(QStringLiteral("sessionStrip"));
        strip->setAttribute(Qt::WA_StyledBackground);
        strip->setFixedHeight(120);
        strip->setStyleSheet(QStringLiteral(
            "#sessionStrip { background:%1; border:none; border-top:1px solid %2; border-bottom:1px solid %2; }")
                                 .arg(kStrip, kBorder));
        auto* stripRow = new QHBoxLayout(strip);
        stripRow->setContentsMargins(0, 0, 0, 0);
        stripRow->setSpacing(0);
        const auto stat = [&](const QString& label, QWidget* value, const QString& sub) {
            auto* column = new QVBoxLayout;
            column->setSpacing(5);
            column->setContentsMargins(16, 0, 16, 0);
            column->addStretch(1);
            column->addWidget(caps(label, 12), 0, Qt::AlignHCenter);
            column->addWidget(value, 0, Qt::AlignHCenter);
            column->addWidget(txt(sub, ui(16, QFont::Normal), kMuted), 0, Qt::AlignHCenter);
            column->addStretch(1);
            stripRow->addLayout(column, 1);
        };
        const auto rule = [&] {
            auto* line = new QFrame;
            line->setObjectName(QStringLiteral("stripRule"));
            line->setFixedWidth(1);
            line->setStyleSheet(QStringLiteral("#stripRule { background:%1; border:none; }").arg(kBorder));
            stripRow->addWidget(line);
        };
        stat(QStringLiteral("BOUGHT IN"),
             txt(money(night.bought), mono(28, QFont::DemiBold), kCashIn),
             QStringLiteral("%1 %2, %3 %4")
                 .arg(night.buyIns)
                 .arg(night.buyIns == 1 ? QStringLiteral("buy-in") : QStringLiteral("buy-ins"))
                 .arg(night.rebuys)
                 .arg(night.rebuys == 1 ? QStringLiteral("rebuy") : QStringLiteral("rebuys")));
        rule();
        stat(QStringLiteral("CASHED OUT"),
             txt(night.hasOut ? money(night.cashed) : QStringLiteral("—"), mono(28, QFont::DemiBold),
                 night.hasOut ? kCashOut : kFaint),
             !night.cashedAfter ? QStringLiteral("Still playing")
                                : QStringLiteral("at %1").arg(clock12(night.lastOut)));
        rule();
        auto* first = new QWidget;
        auto* firstRow = new QHBoxLayout(first);
        firstRow->setContentsMargins(0, 0, 0, 0);
        firstRow->setSpacing(6);
        firstRow->addWidget(txt(night.hasBuy ? night.firstBuy.toString(QStringLiteral("h:mm")) : QStringLiteral("—"),
                                mono(28, QFont::DemiBold), night.hasBuy ? kText : kFaint));
        if (night.hasBuy) firstRow->addWidget(txt(night.firstBuy.toString(QStringLiteral("AP")), ui(16, QFont::DemiBold), kMuted));
        stat(QStringLiteral("FIRST BUY-IN"), first,
             night.hasRebuy ? QStringLiteral("Last rebuy %1").arg(clock12(night.lastRebuy)) : QStringLiteral("No rebuys"));
        detailLayout_->addWidget(strip);
    } else if (!inSession) {
        auto* strip = new QFrame;
        strip->setObjectName(QStringLiteral("absentStrip"));
        strip->setAttribute(Qt::WA_StyledBackground);
        strip->setFixedHeight(56);
        strip->setStyleSheet(QStringLiteral(
            "#absentStrip { background:%1; border:none; border-top:1px solid %2; border-bottom:1px solid %2; }")
                                 .arg(kStrip, kBorder));
        auto* row = new QHBoxLayout(strip);
        row->setContentsMargins(28, 0, 28, 0);
        row->addWidget(txt(QStringLiteral("Not in this session"), ui(18, QFont::Normal), kMuted));
        detailLayout_->addWidget(strip);
    }

    if (minor) {
        auto* wrap = new QWidget;
        auto* wrapLay = new QVBoxLayout(wrap);
        wrapLay->setContentsMargins(28, 20, 24, 0);
        auto* banner = new QFrame;
        banner->setObjectName(QStringLiteral("minorBanner"));
        banner->setAttribute(Qt::WA_StyledBackground);
        banner->setStyleSheet(QStringLiteral(
            "#minorBanner { background:rgba(242,139,130,0.08); border:1px solid rgba(242,139,130,0.35); border-radius:14px; }"));
        auto* column = new QVBoxLayout(banner);
        column->setContentsMargins(20, 14, 14, 14);
        column->setSpacing(12);
        auto* notice = new QLabel;
        notice->setTextFormat(Qt::RichText);
        notice->setWordWrap(true);
        notice->setFont(ui(19, QFont::Normal));
        notice->setStyleSheet(QStringLiteral("color:%1; background:transparent; border:none;").arg(kSoftText));
        notice->setText(QStringLiteral("<span style=\"color:#F28B82; font-weight:700;\">Under 18.</span> Buy-in and cash-out are locked."));
        column->addWidget(notice);
        auto* again = iconButton(QStringLiteral("checkAgain"), QStringLiteral("id-card"), 20, QColor(kIcon),
                                 QStringLiteral("Check ID again"), ui(18, QFont::DemiBold), 52,
                                 QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 18px;")
                                     .arg(kRaised, kText, kBorderButton));
        connect(again, &QPushButton::clicked, this, [this] { checkIdAgain(); });
        column->addWidget(again, 0, Qt::AlignLeft);
        wrapLay->addWidget(banner);
        detailLayout_->addWidget(wrap);
    }

    auto* detailScroll = new QScrollArea;
    detailScroll->setObjectName(QStringLiteral("detailScroll"));
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailScroll->setStyleSheet(QStringLiteral(
        "#detailScroll, #detailScroll > QWidget, #detailViewport { background:%1; border:none; }").arg(kPanel));
    auto* detailBody = new QWidget;
    detailBody->setObjectName(QStringLiteral("detailBody"));
    detailBody->setAttribute(Qt::WA_StyledBackground);
    auto* fields = new QVBoxLayout(detailBody);
    fields->setContentsMargins(28, 22, 24, 16);
    fields->setSpacing(12);
    fields->addWidget(caps(QStringLiteral("DETAILS"), 13));
    const QString student = toQ(player->studentNumber());
    const QString email = toQ(player->email());
    const QString degree = toQ(player->degree());
    const QString year = toQ(player->studyYear());
    const bool any = hasDetail(student) || hasDetail(email) || hasDetail(degree) || hasDetail(year);
    if (!named) {
        auto* box = new QFrame;
        box->setObjectName(QStringLiteral("emptyFile"));
        box->setAttribute(Qt::WA_StyledBackground);
        box->setStyleSheet(QStringLiteral(
            "#emptyFile { background:transparent; border:1px dashed %1; border-radius:14px; }").arg(kBorderButton));
        auto* column = new QVBoxLayout(box);
        column->setContentsMargins(24, 22, 24, 22);
        column->setSpacing(16);
        auto* copy = txt(QStringLiteral("Nothing on file but the card number. They were added as a new player the first time they scanned in."),
                         ui(20, QFont::Normal), kSoftText);
        copy->setWordWrap(true);
        column->addWidget(copy);
        auto* add = iconButton(QStringLiteral("addDetails"), QStringLiteral("pencil"), 22, QColor(kIcon),
                               QStringLiteral("Add details"), ui(20, QFont::DemiBold), 60,
                               QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 22px 0 18px;")
                                   .arg(kRaised, kText, kBorderButton));
        connect(add, &QPushButton::clicked, this, [this] { editPlayer(); });
        column->addWidget(add, 0, Qt::AlignLeft);
        fields->addWidget(box);
    } else if (!any) {
        fields->addWidget(txt(QStringLiteral("No other details on file"), ui(20, QFont::Normal), kMuted));
    } else {
        const auto fieldRow = [&](const QString& label, const QString& value, bool number) {
            if (!hasDetail(value)) return;
            auto* row = new QFrame;
            row->setObjectName(QStringLiteral("detailField"));
            row->setAttribute(Qt::WA_StyledBackground);
            row->setAutoFillBackground(false);
            row->setFixedHeight(58);
            row->setStyleSheet(QStringLiteral("#detailField { background:transparent; border:none; border-bottom:1px solid %1; }").arg(kDivider));
            auto* line = new QHBoxLayout(row);
            line->setContentsMargins(0, 0, 0, 0);
            line->setSpacing(12);
            auto* name = caps(label, 13);
            name->setFixedWidth(116);
            line->addWidget(name);
            auto* valueLabel = new ElideLabel(value, number ? mono(19, QFont::Normal) : ui(20, QFont::Normal), kText);
            valueLabel->setMinimumWidth(48);
            line->addWidget(valueLabel, 1);
            fields->addWidget(row);
        };
        fieldRow(QStringLiteral("STUDENT #"), student, true);
        fieldRow(QStringLiteral("EMAIL"), email, false);
        fieldRow(QStringLiteral("DEGREE"), degree, false);
        fieldRow(QStringLiteral("YEAR"), year, false);
    }
    fields->addStretch(1);
    detailScroll->setWidget(detailBody);
    detailBody->setStyleSheet(QStringLiteral("#detailBody { background:%1; border:none; }").arg(kPanel));
    detailScroll->viewport()->setObjectName(QStringLiteral("detailViewport"));
    detailScroll->viewport()->setAttribute(Qt::WA_StyledBackground);
    detailScroll->viewport()->setAutoFillBackground(true);
    QPalette detailPal = detailBody->palette();
    detailPal.setColor(QPalette::Window, QColor(kPanel));
    detailPal.setColor(QPalette::Base, QColor(kPanel));
    detailBody->setPalette(detailPal);
    detailBody->setAutoFillBackground(true);
    detailScroll->viewport()->setPalette(detailPal);
    QScroller::grabGesture(detailScroll->viewport(), QScroller::LeftMouseButtonGesture);
    detailLayout_->addWidget(detailScroll, 1);

    auto* footer = new QFrame;
    footer->setObjectName(QStringLiteral("detailFooter"));
    footer->setStyleSheet(QStringLiteral("#detailFooter { background:transparent; border:none; border-top:1px solid %1; }").arg(kBorder));
    auto* footerRow = new QHBoxLayout(footer);
    footerRow->setContentsMargins(24, 18, 24, 24);
    footerRow->setSpacing(12);
    if (named) {
        auto* edit = iconButton(QStringLiteral("editDetails"), QStringLiteral("pencil"), 22, QColor(kIcon),
                                QStringLiteral("Edit details"), ui(20, QFont::Bold), 64,
                                QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:16px;")
                                    .arg(kRaised, kText, kBorderButton));
        connect(edit, &QPushButton::clicked, this, [this] { editPlayer(); });
        footerRow->addWidget(edit, 10);
    }
    auto* load = iconButton(QStringLiteral("loadMain"), QStringLiteral("arrow-right"), 22, QColor(kBg),
                            QStringLiteral("Load on main screen"), ui(20, QFont::Bold), 64,
                            QStringLiteral("background:%1; color:%2; border:none; border-radius:16px;").arg(kText, kBg));
    load->setLayoutDirection(Qt::RightToLeft);
    connect(load, &QPushButton::clicked, this, [this] { loadSelected(); });
    footerRow->addWidget(load, 14);
    detailLayout_->addWidget(footer);
}

void PlayersScreen::onScan(const QString& raw) {
    const std::string id = raw.trimmed().toStdString();
    if (id.empty()) return;
    std::set<std::string> sessionIds;
    for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
    const bool inSession = sessionIds.count(id) > 0;
    const bool inDb = ctx_.players().contains(id);
    if (sessionView_ && inSession) {
        // Stay on this view.
    } else if (!sessionView_ && inDb) {
        // Stay on all members.
    } else if (inDb) {
        sessionView_ = false;
        filter_ = 0;
    } else {
        showToast(QStringLiteral("No player with card %1").arg(raw.trimmed()), QString());
        return;
    }
    search_->clear();
    filter_ = 0;
    updateChrome();
    rebuildHeader();
    rebuildList();
    selectPlayer(id);
    if (listModel_ && listView_) {
        const int row = listModel_->rowOf(id);
        if (row >= 0) listView_->scrollTo(listModel_->index(row, 0), QAbstractItemView::PositionAtCenter);
    }
}

void PlayersScreen::loadSelected() {
    if (selected_.empty()) return;
    ctx_.noteLoadedPlayer(selected_);
    emit loadOnMain(QString::fromStdString(selected_));
    emit closed(changed_);
}

void PlayersScreen::showToast(const QString& title, const QString& detail) {
    toastTitle_->setText(title);
    toastDetail_->setText(detail);
    toastDetail_->setVisible(!detail.isEmpty());
    toastWrap_->show();
    toastTimer_->start();
}

QDate PlayersScreen::sessionDate() const {
    QDate best;
    const auto consider = [&](const std::string& utc) {
        const QDateTime local = localFromUtc(utc);
        if (!local.isValid()) return;
        if (!best.isValid() || local.date() < best) best = local.date();
    };
    for (const auto& scan : ctx_.players().attendance().records()) consider(scan.time);
    for (const auto& transaction : ctx_.log().entries()) consider(transaction->time());
    return best.isValid() ? best : QDate::currentDate();
}

void PlayersScreen::exportSessionList() {
    const QString suggested = QStringLiteral("pokersoc-session-%1.csv").arg(sessionDate().toString(QStringLiteral("yyyy-MM-dd")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export session list"), suggested,
                                                      QStringLiteral("CSV files (*.csv);;All files (*.*)"));
    if (path.isEmpty()) return;
    std::set<std::string> sessionIds;
    for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
    std::vector<const Player*> players;
    for (const std::string& id : sessionIds) {
        if (const Player* player = ctx_.players().find(id)) players.push_back(player);
    }
    std::sort(players.begin(), players.end(), [](const Player* a, const Player* b) {
        const bool namedA = realName(*a);
        const bool namedB = realName(*b);
        if (namedA != namedB) return namedA;
        if (namedA) {
            const int name = QString::fromStdString(a->name()).compare(QString::fromStdString(b->name()), Qt::CaseInsensitive);
            if (name != 0) return name < 0;
        }
        return a->id() < b->id();
    });
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Cannot write '%1'.").arg(path));
        return;
    }
    QByteArray out;
    out += "player_id,display_name,email,student_number,degree,study_year,arc_member,"
           "is_underage,membership_type,created_at,Bought in,Cashed out,Status\r\n";
    for (const Player* player : players) {
        const Night night = nightFor(ctx_, *player);
        const char* status = night.status == NightStatus::Locked ? "Locked"
                             : night.status == NightStatus::Cashed ? "Cashed out" : "Playing";
        out += util::csvEscape(player->id()) + ',' +
               util::csvEscape(player->name().empty() ? "New Player" : player->name()) + ',' +
               util::csvEscape(orNone(player->email())) + ',' +
               util::csvEscape(orNone(player->studentNumber())) + ',' +
               util::csvEscape(orNone(player->degree())) + ',' +
               util::csvEscape(orNone(player->studyYear())) + ',' +
               arcText(player->arcStatus()) + ',' +
               (player->isUnderage() ? "1" : "0") + ',' +
               domain::membershipTypeLabel(player->membership()) + ',' +
               util::csvEscape(player->createdAt()) + ',' +
               util::csvEscape(money(night.bought).toStdString()) + ',' +
               util::csvEscape(money(night.cashed).toStdString()) + ',' +
               status + "\r\n";
    }
    file.write(out);
    showToast(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()),
              QStringLiteral("%1 players").arg(players.size()));
}

void PlayersScreen::exportAllMembers() {
    const QString suggested = QStringLiteral("pokersoc-players-%1.csv").arg(sessionDate().toString(QStringLiteral("yyyy-MM-dd")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Players"), suggested,
                                                      QStringLiteral("CSV files (*.csv);;All files (*.*)"));
    if (path.isEmpty()) return;
    try {
        const std::size_t count = ctx_.files().exportPlayers(toStd(path));
        showToast(QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()),
                  QStringLiteral("%1 players").arg(static_cast<int>(count)));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("Error exporting players: %1").arg(error.what()));
    }
}

void PlayersScreen::exportBoth() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Save session and members"));
    if (dir.isEmpty()) return;
    const QString date = sessionDate().toString(QStringLiteral("yyyy-MM-dd"));
    const QString sessionPath = dir + QStringLiteral("/pokersoc-session-%1.csv").arg(date);
    const QString playersPath = dir + QStringLiteral("/pokersoc-players-%1.csv").arg(date);
    const QString previous = search_->text();
    search_->clear();
    try {
        const std::size_t members = ctx_.files().exportPlayers(toStd(playersPath));
        QFile sessionFile(sessionPath);
        if (!sessionFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("Cannot write the session file.");
        }
        std::set<std::string> sessionIds;
        for (const auto& scan : ctx_.players().attendance().firstScans()) sessionIds.insert(scan.playerId);
        QByteArray out;
        out += "player_id,display_name,email,student_number,degree,study_year,arc_member,"
               "is_underage,membership_type,created_at,Bought in,Cashed out,Status\r\n";
        int sessionCount = 0;
        for (const std::string& id : sessionIds) {
            const Player* player = ctx_.players().find(id);
            if (!player) continue;
            const Night night = nightFor(ctx_, *player);
            const char* status = night.status == NightStatus::Locked ? "Locked"
                                 : night.status == NightStatus::Cashed ? "Cashed out" : "Playing";
            out += util::csvEscape(player->id()) + ',' +
                   util::csvEscape(player->name().empty() ? "New Player" : player->name()) + ',' +
                   util::csvEscape(orNone(player->email())) + ',' +
                   util::csvEscape(orNone(player->studentNumber())) + ',' +
                   util::csvEscape(orNone(player->degree())) + ',' +
                   util::csvEscape(orNone(player->studyYear())) + ',' +
                   arcText(player->arcStatus()) + ',' +
                   (player->isUnderage() ? "1" : "0") + ',' +
                   domain::membershipTypeLabel(player->membership()) + ',' +
                   util::csvEscape(player->createdAt()) + ',' +
                   util::csvEscape(money(night.bought).toStdString()) + ',' +
                   util::csvEscape(money(night.cashed).toStdString()) + ',' +
                   status + "\r\n";
            ++sessionCount;
        }
        sessionFile.write(out);
        showToast(QStringLiteral("Saved 2 files"),
                  QStringLiteral("%1 session players, %2 members").arg(sessionCount).arg(static_cast<int>(members)));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Error exporting: %1").arg(error.what()));
    }
    search_->setText(previous);
}

void PlayersScreen::beginImport() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Player Database"), QString(),
                                                      QStringLiteral("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty()) showImportReview(path);
}

void PlayersScreen::showImportReview(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Cannot read '%1'.").arg(path));
        return;
    }
    QString text = QString::fromUtf8(file.readAll());
    if (text.startsWith(QChar(0xFEFF))) text.remove(0, 1);
    importPath_ = path;
    importRows_.clear();
    fillBlanks_ = true;
    bool header = true;
    int lineNo = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("\\r\\n|\\n|\\r")));
    for (const QString& line : lines) {
        ++lineNo;
        if (header) {
            header = false;
            continue;
        }
        if (line.trimmed().isEmpty()) continue;
        const std::vector<std::string> parts = util::csvParseLine(line.toStdString());
        auto field = [&](std::size_t index) {
            return index < parts.size() ? QString::fromStdString(util::trim(parts[index])) : QString();
        };
        importRows_.push_back({QString::number(lineNo), field(0), field(1), field(2), field(3), field(4),
                               field(5), field(6), field(7), field(8)});
    }
    closeOverlay();
    overlay_ = true;
    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    dim_ = new Scrim(host);
    dim_->setObjectName(QStringLiteral("importScrim"));
    dim_->setStyleSheet(QStringLiteral("#importScrim { background:rgba(2,5,9,0.78); }"));
    dim_->setGeometry(host->rect());
    dialog_ = new QFrame(dim_);
    dialog_->setObjectName(QStringLiteral("importDialog"));
    dialog_->setAttribute(Qt::WA_StyledBackground);
    dialog_->setFixedWidth(1040);
    dialog_->setStyleSheet(QStringLiteral(
        "#importDialog { background:%1; border:1px solid %2; border-radius:24px; }").arg(kPanel, kBorderRaised));
    auto* shadow = new QGraphicsDropShadowEffect(dialog_);
    shadow->setBlurRadius(120);
    shadow->setOffset(0, 40);
    shadow->setColor(QColor(0, 0, 0, 153));
    dialog_->setGraphicsEffect(shadow);
    auto* column = new QVBoxLayout(dialog_);
    column->setContentsMargins(40, 40, 40, 40);
    column->setSpacing(24);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(22);
    auto* iconBox = new QLabel;
    iconBox->setObjectName(QStringLiteral("importIcon"));
    iconBox->setAttribute(Qt::WA_StyledBackground);
    iconBox->setFixedSize(76, 76);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setPixmap(iconPx(QStringLiteral("upload"), 38, QColor(kCashIn)));
    iconBox->setStyleSheet(QStringLiteral("#importIcon { background:rgba(159,162,229,0.16); border-radius:20px; }"));
    headerRow->addWidget(iconBox, 0, Qt::AlignTop);
    auto* heading = new QVBoxLayout;
    heading->setSpacing(6);
    heading->addWidget(caps(QStringLiteral("IMPORT PLAYERS"), 14, kCashIn));
    heading->addWidget(txt(QStringLiteral("Review before importing"), ui(48, QFont::Bold), kText));
    auto* fileRow = new QHBoxLayout;
    fileRow->setSpacing(8);
    fileRow->addWidget(iconLabel(QStringLiteral("file-spreadsheet"), 20, QColor(kSoftText)));
    fileRow->addWidget(txt(QFileInfo(path).fileName(), mono(18, QFont::Normal), kSoftText));
    fileRow->addWidget(txt(QStringLiteral("· %1 rows").arg(importRows_.size()), ui(20, QFont::Normal), kMuted));
    fileRow->addStretch(1);
    heading->addLayout(fileRow);
    headerRow->addLayout(heading, 1);
    auto* change = iconButton(QStringLiteral("changeFile"), QString(), 0, QColor(), QStringLiteral("Change file"),
                              ui(18, QFont::DemiBold), 52,
                              QStringLiteral("background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 18px;")
                                  .arg(kRaised, kText, kBorderButton));
    connect(change, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Player Database"), QString(),
                                                          QStringLiteral("CSV files (*.csv);;All files (*.*)"));
        if (!path.isEmpty()) showImportReview(path);
    });
    headerRow->addWidget(change, 0, Qt::AlignTop);
    column->addLayout(headerRow);

    auto* tiles = new QHBoxLayout;
    tiles->setSpacing(12);
    const auto tile = [&](QLabel*& value, const QString& icon, const QString& label, const QString& tint) {
        auto* card = new QFrame;
        card->setObjectName(QStringLiteral("importTile"));
        card->setAttribute(Qt::WA_StyledBackground);
        card->setStyleSheet(QStringLiteral(
            "#importTile { background:%1; border:1px solid %2; border-radius:18px; }").arg(kStrip, kBorderRaised));
        auto* tileColumn = new QVBoxLayout(card);
        tileColumn->setContentsMargins(22, 20, 22, 20);
        tileColumn->setSpacing(8);
        auto* labelRow = new QHBoxLayout;
        labelRow->setSpacing(8);
        labelRow->addWidget(iconLabel(icon, 18, QColor(tint)));
        labelRow->addWidget(caps(label, 13, tint));
        labelRow->addStretch(1);
        tileColumn->addLayout(labelRow);
        value = txt(QStringLiteral("0"), mono(44, QFont::DemiBold), tint);
        tileColumn->addWidget(value);
        tiles->addWidget(card, 1);
    };
    tile(importNew_, QStringLiteral("user-plus"), QStringLiteral("NEW"), kCashIn);
    tile(importUpdated_, QStringLiteral("pencil"), QStringLiteral("UPDATED"), kSoftText);
    tile(importSame_, QStringLiteral("check"), QStringLiteral("UNCHANGED"), kSoftText);
    tile(importSkipped_, QStringLiteral("triangle-alert"), QStringLiteral("SKIPPED"), QStringLiteral("#F28B82"));
    column->addLayout(tiles);
    auto* subs = new QHBoxLayout;
    subs->setSpacing(12);
    const char* subText[] = {"Added to the database", "Blank details filled in", "Already up to date", "Rows with no player ID"};
    for (int i = 0; i < 4; ++i) {
        auto* sub = txt(QString::fromUtf8(subText[i]), ui(17, QFont::Normal), kMuted);
        if (i == 1) sub->setObjectName(QStringLiteral("updatedSub"));
        subs->addWidget(sub, 1);
    }
    column->addLayout(subs);

    auto* policy = new QFrame;
    policy->setObjectName(QStringLiteral("importPolicy"));
    policy->setAttribute(Qt::WA_StyledBackground);
    policy->setStyleSheet(QStringLiteral(
        "#importPolicy { background:%1; border:1px solid %2; border-radius:16px; }").arg(kRaised, kBorderButton));
    auto* policyRow = new QHBoxLayout(policy);
    policyRow->setContentsMargins(20, 14, 14, 14);
    policyRow->setSpacing(18);
    auto* shield = new QLabel;
    shield->setObjectName(QStringLiteral("shieldBox"));
    shield->setAttribute(Qt::WA_StyledBackground);
    shield->setFixedSize(48, 48);
    shield->setAlignment(Qt::AlignCenter);
    shield->setPixmap(iconPx(QStringLiteral("shield-check"), 26, QColor(kIcon)));
    shield->setStyleSheet(QStringLiteral("#shieldBox { background:#262D35; border-radius:12px; }"));
    policyRow->addWidget(shield);
    auto* policyText = new QVBoxLayout;
    policyText->setSpacing(2);
    policyText->addWidget(txt(QStringLiteral("Player already in the database?"), ui(21, QFont::DemiBold), kText));
    importPolicyNote_ = txt(QStringLiteral("Age status is never changed by an import."), ui(17, QFont::Normal), kMuted);
    policyText->addWidget(importPolicyNote_);
    policyRow->addLayout(policyText, 1);
    auto* policySwitch = new QFrame;
    policySwitch->setObjectName(QStringLiteral("policySwitch"));
    policySwitch->setAttribute(Qt::WA_StyledBackground);
    policySwitch->setFixedHeight(58);
    policySwitch->setStyleSheet(QStringLiteral(
        "#policySwitch { background:%1; border:1px solid %2; border-radius:15px; }").arg(kInset, kBorder));
    auto* switchRow = new QHBoxLayout(policySwitch);
    switchRow->setContentsMargins(4, 4, 4, 4);
    switchRow->setSpacing(4);
    auto* blanks = new QPushButton(QStringLiteral("Only fill in blanks"));
    auto* replace = new QPushButton(QStringLiteral("Use the file's details"));
    blanks->setObjectName(QStringLiteral("policyBlanks"));
    replace->setObjectName(QStringLiteral("policyReplace"));
    blanks->setFixedHeight(50);
    replace->setFixedHeight(50);
    blanks->setFont(ui(18, QFont::DemiBold));
    replace->setFont(ui(18, QFont::DemiBold));
    blanks->setFocusPolicy(Qt::NoFocus);
    replace->setFocusPolicy(Qt::NoFocus);
    blanks->setCursor(Qt::PointingHandCursor);
    replace->setCursor(Qt::PointingHandCursor);
    switchRow->addWidget(blanks);
    switchRow->addWidget(replace);
    const auto paintPolicy = [=] {
        const QString on = QStringLiteral("background:%1; color:%2; border:none; border-radius:11px; padding:0 16px;").arg(kText, kBg);
        const QString off = QStringLiteral("background:transparent; color:%1; border:none; border-radius:11px; padding:0 16px;").arg(kSoftText);
        blanks->setStyleSheet(QStringLiteral("#policyBlanks { %1 }").arg(fillBlanks_ ? on : off));
        replace->setStyleSheet(QStringLiteral("#policyReplace { %1 }").arg(fillBlanks_ ? off : on));
    };
    paintPolicy();
    connect(blanks, &QPushButton::clicked, this, [=] {
        fillBlanks_ = true;
        paintPolicy();
        paintImportReview();
    });
    connect(replace, &QPushButton::clicked, this, [=] {
        fillBlanks_ = false;
        paintPolicy();
        paintImportReview();
    });
    policyRow->addWidget(policySwitch);
    column->addWidget(policy);

    auto* changes = new QFrame;
    changes->setObjectName(QStringLiteral("changeList"));
    changes->setAttribute(Qt::WA_StyledBackground);
    changes->setStyleSheet(QStringLiteral("#changeList { background:transparent; border:1px solid %1; border-radius:16px; }").arg(kBorder));
    auto* changeColumn = new QVBoxLayout(changes);
    changeColumn->setContentsMargins(0, 0, 0, 0);
    changeColumn->setSpacing(0);
    auto* changeHead = new QFrame;
    changeHead->setFixedHeight(44);
    changeHead->setObjectName(QStringLiteral("changeHead"));
    changeHead->setStyleSheet(QStringLiteral("#changeHead { background:%1; border:none; border-bottom:1px solid %2; }").arg(kStrip, kDivider));
    auto* changeHeadRow = new QHBoxLayout(changeHead);
    changeHeadRow->setContentsMargins(20, 0, 20, 0);
    changeHeadRow->addWidget(caps(QStringLiteral("CHANGES"), 13));
    changeHeadRow->addStretch(1);
    importChangeCaption_ = txt(QString(), ui(16, QFont::Normal), kMuted);
    changeHeadRow->addWidget(importChangeCaption_);
    changeColumn->addWidget(changeHead);
    auto* changeScroll = new QScrollArea;
    changeScroll->setWidgetResizable(true);
    changeScroll->setFixedHeight(232);
    changeScroll->setFrameShape(QFrame::NoFrame);
    changeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    changeScroll->setStyleSheet(QStringLiteral("QScrollArea { background:transparent; border:none; }"));
    auto* changeBody = new QWidget;
    importChanges_ = new QVBoxLayout(changeBody);
    importChanges_->setContentsMargins(0, 0, 0, 0);
    importChanges_->setSpacing(0);
    importChanges_->addStretch(1);
    changeScroll->setWidget(changeBody);
    QScroller::grabGesture(changeScroll->viewport(), QScroller::LeftMouseButtonGesture);
    changeColumn->addWidget(changeScroll);
    column->addWidget(changes);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(16);
    auto* cancel = iconButton(QStringLiteral("importCancel"), QString(), 0, QColor(), QStringLiteral("Cancel"),
                              ui(22, QFont::DemiBold), 88,
                              QStringLiteral("background:transparent; color:%1; border:1px solid %2; border-radius:18px;")
                                  .arg(kSoftText, kBorderButton));
    cancel->setFixedSize(168, 88);
    connect(cancel, &QPushButton::clicked, this, [this] { closeOverlay(); focusScanner(); });
    importApply_ = iconButton(QStringLiteral("importApply"), QStringLiteral("upload"), 28, QColor("#14152B"),
                              QStringLiteral("Import 0 changes"), ui(26, QFont::Bold), 88,
                              QStringLiteral("background:%1; color:#14152B; border:2px solid #B4B6EE; border-radius:18px;").arg(kCashIn));
    connect(importApply_, &QPushButton::clicked, this, [this] { applyImport(); });
    buttons->addWidget(cancel);
    buttons->addWidget(importApply_, 1);
    column->addLayout(buttons);

    paintImportReview();
    dialog_->adjustSize();
    dialog_->move((dim_->width() - dialog_->width()) / 2, (dim_->height() - dialog_->height()) / 2);
    dim_->show();
    dim_->raise();
}

void PlayersScreen::paintImportReview() {
    if (!importNew_) return;
    const Plan plan = planImport(ctx_.players(), importRows_, fillBlanks_);
    importNew_->setText(QString::number(plan.added));
    importUpdated_->setText(QString::number(plan.updated));
    importSame_->setText(QString::number(plan.same));
    importSkipped_->setText(QString::number(plan.skipped));
    const int changes = plan.added + plan.updated;
    importChangeCaption_->setText(QStringLiteral("%1 changes · %2 skipped").arg(changes).arg(plan.skipped));
    importApply_->setText(QStringLiteral("Import %1 changes").arg(changes));
    if (auto* updatedSub = dialog_ ? dialog_->findChild<QLabel*>(QStringLiteral("updatedSub")) : nullptr) {
        updatedSub->setText(fillBlanks_ ? QStringLiteral("Blank details filled in")
                                        : QStringLiteral("Details replaced from file"));
    }
    wipe(importChanges_);
    for (const PlanChange& change : plan.rows) {
        auto* row = new QFrame;
        row->setObjectName(QStringLiteral("changeRow"));
        row->setFixedHeight(58);
        row->setStyleSheet(QStringLiteral("#changeRow { background:transparent; border:none; border-bottom:1px solid %1; }").arg(kDivider));
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(20, 0, 20, 0);
        line->setSpacing(16);
        auto* badge = new QLabel(change.kind);
        badge->setObjectName(QStringLiteral("changeBadge"));
        badge->setAttribute(Qt::WA_StyledBackground);
        badge->setFixedHeight(28);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFont(ui(14, QFont::Bold));
        QFont badgeFont = badge->font();
        badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        badge->setFont(badgeFont);
        QString bg = QStringLiteral("#1e262f");
        QString fg = kSoftText;
        if (change.kind == QLatin1String("NEW")) {
            bg = QStringLiteral("rgba(159,162,229,0.16)");
            fg = kCashIn;
        } else if (change.kind == QLatin1String("SKIPPED")) {
            bg = QStringLiteral("rgba(242,139,130,0.14)");
            fg = QStringLiteral("#F28B82");
        }
        badge->setStyleSheet(QStringLiteral("#changeBadge { background:%1; color:%2; border-radius:8px; padding:0 8px; }").arg(bg, fg));
        badge->setFixedWidth(112);
        line->addWidget(badge);
        auto* who = new QVBoxLayout;
        who->setSpacing(0);
        who->addWidget(new ElideLabel(change.title, ui(20, QFont::Medium),
                                      change.kind == QLatin1String("SKIPPED") ? kMuted : kText));
        if (!change.sub.isEmpty()) who->addWidget(txt(change.sub, mono(14, QFont::Normal), kFaint));
        line->addLayout(who, 1);
        line->addWidget(new ElideLabel(change.what, ui(18, QFont::Normal), kMuted), 12);
        importChanges_->addWidget(row);
    }
    importChanges_->addStretch(1);
}

void PlayersScreen::applyImport() {
    int added = 0;
    int updated = 0;
    try {
        for (const QStringList& row : importRows_) {
            const bool exists = !row.value(1).trimmed().isEmpty() && ctx_.players().contains(row.value(1).trimmed().toStdString());
            const Plan one = planImport(ctx_.players(), {row}, fillBlanks_);
            if (one.added == 0 && one.updated == 0) continue;
            applyRow(ctx_.players(), row, fillBlanks_);
            if (exists) ++updated;
            else ++added;
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Error importing players: %1").arg(error.what()));
        return;
    }
    changed_ = true;
    closeOverlay();
    refresh();
    showToast(QStringLiteral("Imported %1 changes").arg(added + updated),
              QStringLiteral("%1 new players added to the database").arg(added));
    focusScanner();
}

void PlayersScreen::chooseMembership() {
    if (selected_.empty()) return;
    closeOverlay();
    overlay_ = true;
    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    dim_ = new Scrim(host);
    dim_->setObjectName(QStringLiteral("memberScrim"));
    dim_->setStyleSheet(QStringLiteral("#memberScrim { background:rgba(2,5,9,0.78); }"));
    dim_->setGeometry(host->rect());
    dialog_ = new QFrame(dim_);
    dialog_->setObjectName(QStringLiteral("memberDialog"));
    dialog_->setAttribute(Qt::WA_StyledBackground);
    dialog_->setFixedWidth(680);
    dialog_->setStyleSheet(QStringLiteral(
        "#memberDialog { background:%1; border:1px solid %2; border-radius:18px; }").arg(kPanel, kBorder));
    auto* column = new QVBoxLayout(dialog_);
    column->setContentsMargins(28, 28, 28, 28);
    column->setSpacing(16);
    column->addWidget(txt(QStringLiteral("Membership"), ui(32, QFont::Bold), kText));
    const MembershipType types[] = {MembershipType::Bronze, MembershipType::Silver, MembershipType::Gold,
                                    MembershipType::Platinum, MembershipType::Diamond};
    for (MembershipType type : types) {
        auto* button = new QPushButton(QString::fromLatin1(membershipTypeLabel(type)));
        button->setObjectName(QStringLiteral("memberChoice"));
        button->setFont(ui(28, QFont::Bold));
        button->setFixedHeight(96);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "#memberChoice { background:%1; color:%2; border:1px solid %3; border-radius:18px; }"
            "#memberChoice:hover, #memberChoice:pressed { background:%1; color:%2; }")
                                  .arg(kRaised, kText, kBorderButton));
        connect(button, &QPushButton::clicked, this, [this, type] {
            ctx_.players().setMembership(selected_, type);
            changed_ = true;
            closeOverlay();
            refresh();
            focusScanner();
        });
        column->addWidget(button);
    }
    auto* cancel = new QPushButton(QStringLiteral("Cancel"));
    cancel->setObjectName(QStringLiteral("memberCancel"));
    cancel->setFont(ui(22, QFont::DemiBold));
    cancel->setFixedHeight(80);
    cancel->setFocusPolicy(Qt::NoFocus);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QStringLiteral(
        "#memberCancel { color:%1; background:transparent; border:1px solid %2; border-radius:18px; }"
        "#memberCancel:hover, #memberCancel:pressed { color:%1; background:transparent; }")
                              .arg(kMuted, kBorderButton));
    connect(cancel, &QPushButton::clicked, this, [this] { closeOverlay(); focusScanner(); });
    column->addWidget(cancel);
    dialog_->adjustSize();
    dialog_->move((dim_->width() - dialog_->width()) / 2, (dim_->height() - dialog_->height()) / 2);
    dim_->show();
    dim_->raise();
}

void PlayersScreen::editPlayer() {
    const Player* player = selected_.empty() ? nullptr : ctx_.players().find(selected_);
    if (!player) return;
    closeOverlay();
    overlay_ = true;
    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    dim_ = new Scrim(host);
    dim_->setObjectName(QStringLiteral("editScrim"));
    dim_->setStyleSheet(QStringLiteral("#editScrim { background:rgba(2,5,9,0.78); }"));
    dim_->setGeometry(host->rect());
    dialog_ = new QFrame(dim_);
    dialog_->setObjectName(QStringLiteral("editDialog"));
    dialog_->setAttribute(Qt::WA_StyledBackground);
    dialog_->setFixedWidth(720);
    dialog_->setStyleSheet(QStringLiteral(
        "#editDialog { background:%1; border:1px solid %2; border-radius:24px; }").arg(kPanel, kBorderRaised));
    auto* column = new QVBoxLayout(dialog_);
    column->setContentsMargins(36, 32, 36, 32);
    column->setSpacing(16);
    column->addWidget(txt(realName(*player) ? QStringLiteral("Edit details") : QStringLiteral("Add details"),
                          ui(36, QFont::Bold), kText));
    const auto field = [&](const QString& label, const QString& value) {
        column->addWidget(caps(label, 13));
        auto* edit = new QLineEdit(blankText(value, label == QLatin1String("NAME")) ? QString() : value);
        edit->setObjectName(QStringLiteral("editField"));
        edit->setFont(ui(20, QFont::Normal));
        edit->setFixedHeight(56);
        darkLine(edit, kRaised);
        edit->setStyleSheet(QStringLiteral(
            "#editField { background:%1; color:%2; border:1px solid %3; border-radius:14px; padding:0 16px; min-height:0px; }")
                                .arg(kRaised, kText, kBorderButton));
        column->addWidget(edit);
        return edit;
    };
    QLineEdit* name = field(QStringLiteral("NAME"), toQ(player->name()));
    QLineEdit* email = field(QStringLiteral("EMAIL"), toQ(player->email()));
    QLineEdit* student = field(QStringLiteral("STUDENT #"), toQ(player->studentNumber()));
    QLineEdit* degree = field(QStringLiteral("DEGREE"), toQ(player->degree()));
    QLineEdit* year = field(QStringLiteral("YEAR"), toQ(player->studyYear()));
    column->addWidget(caps(QStringLiteral("ARC"), 13));
    auto* arcRow = new QHBoxLayout;
    arcRow->setSpacing(8);
    dialog_->setProperty("arc", static_cast<int>(player->arcStatus()));
    const char* arcNames[] = {"Unknown", "Yes", "No"};
    const ArcStatus arcValues[] = {ArcStatus::Unknown, ArcStatus::Arc, ArcStatus::NonArc};
    const auto paintArc = [dialog = dialog_] {
        const int chosen = dialog->property("arc").toInt();
        const auto buttons = dialog->findChildren<QPushButton*>(QStringLiteral("arcChoice"));
        for (QPushButton* button : buttons) {
            const bool on = button->property("value").toInt() == chosen;
            button->setStyleSheet(QStringLiteral(
                "#arcChoice { background:%1; color:%2; border:1px solid %3; border-radius:14px; }")
                                      .arg(on ? kText : kRaised, on ? kBg : kSoftText, on ? kText : kBorderButton));
        }
    };
    for (int i = 0; i < 3; ++i) {
        auto* button = new QPushButton(QString::fromUtf8(arcNames[i]));
        button->setObjectName(QStringLiteral("arcChoice"));
        button->setProperty("value", static_cast<int>(arcValues[i]));
        button->setFont(ui(18, QFont::DemiBold));
        button->setFixedHeight(52);
        button->setFocusPolicy(Qt::NoFocus);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, dialog_, [dialog = dialog_, value = static_cast<int>(arcValues[i]), paintArc] {
            dialog->setProperty("arc", value);
            paintArc();
        });
        arcRow->addWidget(button, 1);
    }
    column->addLayout(arcRow);
    paintArc();
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"));
    cancel->setObjectName(QStringLiteral("editCancel"));
    cancel->setFont(ui(20, QFont::DemiBold));
    cancel->setFixedHeight(64);
    cancel->setFocusPolicy(Qt::NoFocus);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QStringLiteral(
        "#editCancel { background:transparent; color:%1; border:1px solid %2; border-radius:16px; }").arg(kSoftText, kBorderButton));
    connect(cancel, &QPushButton::clicked, this, [this] { closeOverlay(); focusScanner(); });
    auto* save = new QPushButton(QStringLiteral("Save"));
    save->setObjectName(QStringLiteral("editSave"));
    save->setFont(ui(20, QFont::Bold));
    save->setFixedHeight(64);
    save->setFocusPolicy(Qt::NoFocus);
    save->setCursor(Qt::PointingHandCursor);
    save->setStyleSheet(QStringLiteral("#editSave { background:%1; color:%2; border:none; border-radius:16px; }").arg(kText, kBg));
    connect(save, &QPushButton::clicked, this, [=] {
        Player updated = *ctx_.players().find(selected_);
        const QString nextName = name->text().trimmed();
        updated.setName(nextName.isEmpty() ? "New Player" : nextName.toStdString());
        updated.setEmail(email->text().trimmed().toStdString());
        updated.setStudentNumber(student->text().trimmed().toStdString());
        updated.setDegree(degree->text().trimmed().toStdString());
        updated.setStudyYear(year->text().trimmed().toStdString());
        updated.setArcStatus(static_cast<ArcStatus>(dialog_->property("arc").toInt()));
        ctx_.players().save(std::move(updated));
        changed_ = true;
        closeOverlay();
        refresh();
        focusScanner();
    });
    buttons->addWidget(cancel, 1);
    buttons->addWidget(save, 1);
    column->addSpacing(8);
    column->addLayout(buttons);
    dialog_->adjustSize();
    dialog_->move((dim_->width() - dialog_->width()) / 2, qMax(20, (dim_->height() - dialog_->height()) / 2));
    dim_->show();
    dim_->raise();
    name->setFocus();
}

void PlayersScreen::checkIdAgain() {
    const Player* player = selected_.empty() ? nullptr : ctx_.players().find(selected_);
    if (!player) return;
    const std::string id = player->id();
    closeOverlay();
    overlay_ = true;
    QWidget* host = this;
    if (auto* main = qobject_cast<QMainWindow*>(window())) {
        if (main->centralWidget()) host = main->centralWidget();
    }
    dim_ = new Scrim(host);
    dim_->setObjectName(QStringLiteral("idScrim"));
    dim_->setStyleSheet(QStringLiteral("#idScrim { background:rgba(2,5,9,0.78); }"));
    dim_->setGeometry(host->rect());
    dialog_ = new QFrame(dim_);
    dialog_->setObjectName(QStringLiteral("idDialog"));
    dialog_->setAttribute(Qt::WA_StyledBackground);
    dialog_->setFixedWidth(880);
    dialog_->setStyleSheet(QStringLiteral(
        "#idDialog { background:%1; border:1px solid %2; border-radius:24px; }").arg(kPanel, kBorderRaised));
    auto* column = new QVBoxLayout(dialog_);
    column->setContentsMargins(40, 40, 40, 40);
    column->setSpacing(28);
    auto* header = new QHBoxLayout;
    header->setSpacing(22);
    auto* iconBox = new QLabel;
    iconBox->setObjectName(QStringLiteral("idIcon"));
    iconBox->setAttribute(Qt::WA_StyledBackground);
    iconBox->setFixedSize(76, 76);
    iconBox->setAlignment(Qt::AlignCenter);
    iconBox->setPixmap(iconPx(QStringLiteral("id-card"), 38, QColor(kCashIn)));
    iconBox->setStyleSheet(QStringLiteral("#idIcon { background:rgba(159,162,229,0.16); border-radius:20px; }"));
    header->addWidget(iconBox, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(6);
    text->addWidget(caps(QStringLiteral("CHECK ID"), 14, kCashIn));
    text->addWidget(txt(QStringLiteral("Check their ID"), ui(48, QFont::Bold), kText));
    text->addWidget(txt(QStringLiteral("Card %1").arg(toQ(id)), ui(20, QFont::Normal), kMuted));
    header->addLayout(text, 1);
    column->addLayout(header);
    auto* cutoff = new QFrame;
    cutoff->setObjectName(QStringLiteral("cutoffBlock"));
    cutoff->setAttribute(Qt::WA_StyledBackground);
    cutoff->setStyleSheet(QStringLiteral("#cutoffBlock { background:%1; border:1px solid %2; border-radius:18px; }").arg(kStrip, kBorderRaised));
    auto* cutoffRow = new QHBoxLayout(cutoff);
    cutoffRow->setContentsMargins(30, 26, 30, 26);
    auto* cutoffText = new QVBoxLayout;
    cutoffText->setSpacing(6);
    cutoffText->addWidget(caps(QStringLiteral("18+ IF BORN ON OR BEFORE"), 14));
    cutoffText->addWidget(txt(QDate::currentDate().addYears(-18).toString(QStringLiteral("d MMM yyyy")), ui(64, QFont::Bold), kText));
    cutoffRow->addLayout(cutoffText, 1);
    auto* mark = txt(QStringLiteral("18+"), ui(34, QFont::Bold), kText, Qt::AlignCenter);
    mark->setObjectName(QStringLiteral("ageMark"));
    mark->setAttribute(Qt::WA_StyledBackground);
    mark->setFixedSize(88, 88);
    mark->setStyleSheet(QStringLiteral("#ageMark { background:transparent; color:%1; border:2px solid %2; border-radius:44px; }").arg(kText, kBorderButton));
    cutoffRow->addWidget(mark);
    column->addWidget(cutoff);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(16);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"));
    cancel->setObjectName(QStringLiteral("idCancel"));
    cancel->setFont(ui(22, QFont::DemiBold));
    cancel->setFixedSize(168, 88);
    cancel->setFocusPolicy(Qt::NoFocus);
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setStyleSheet(QStringLiteral(
        "#idCancel { background:transparent; color:%1; border:1px solid %2; border-radius:18px; }").arg(kSoftText, kBorderButton));
    connect(cancel, &QPushButton::clicked, this, [this] { closeOverlay(); focusScanner(); });
    auto* under = new QPushButton(QStringLiteral("Under 18"));
    under->setObjectName(QStringLiteral("idUnder"));
    under->setFont(ui(26, QFont::Bold));
    under->setIcon(QIcon(iconPx(QStringLiteral("ban"), 26, QColor("#F28B82"))));
    under->setIconSize(QSize(26, 26));
    under->setFixedHeight(88);
    under->setFocusPolicy(Qt::NoFocus);
    under->setCursor(Qt::PointingHandCursor);
    under->setStyleSheet(QStringLiteral(
        "#idUnder { background:rgba(242,139,130,0.10); color:#F28B82; border:2px solid #845250; border-radius:18px; }"));
    auto* adult = new QPushButton(QStringLiteral("ID checked · 18+"));
    adult->setObjectName(QStringLiteral("idAdult"));
    adult->setFont(ui(26, QFont::Bold));
    adult->setIcon(QIcon(iconPx(QStringLiteral("check"), 30, QColor("#052E16"))));
    adult->setIconSize(QSize(30, 30));
    adult->setFixedHeight(88);
    adult->setFocusPolicy(Qt::NoFocus);
    adult->setCursor(Qt::PointingHandCursor);
    adult->setStyleSheet(QStringLiteral(
        "#idAdult { background:#4ADE80; color:#052E16; border:2px solid #6EE7A0; border-radius:18px; }"));
    const auto resolve = [this, id](bool adultOk) {
        ctx_.players().setUnderage(id, !adultOk);
        changed_ = true;
        closeOverlay();
        refresh();
        focusScanner();
    };
    connect(under, &QPushButton::clicked, this, [=] { resolve(false); });
    connect(adult, &QPushButton::clicked, this, [=] { resolve(true); });
    buttons->addWidget(cancel);
    buttons->addWidget(under, 10);
    buttons->addWidget(adult, 14);
    column->addLayout(buttons);
    dialog_->adjustSize();
    dialog_->move((dim_->width() - dialog_->width()) / 2, (dim_->height() - dialog_->height()) / 2);
    dim_->show();
    dim_->raise();
}

void PlayersScreen::closeOverlay() {
    overlay_ = false;
    if (dim_) {
        dim_->hide();
        dim_->deleteLater();
        dim_ = nullptr;
    }
    dialog_ = nullptr;
    importNew_ = importUpdated_ = importSame_ = importSkipped_ = nullptr;
    importChangeCaption_ = nullptr;
    importChanges_ = nullptr;
    importApply_ = nullptr;
    importPolicyNote_ = nullptr;
}

} // namespace pokersoc::ui
