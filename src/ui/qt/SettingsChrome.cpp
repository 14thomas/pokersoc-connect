#include "ui/qt/SettingsChrome.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyle>
#include <QVariantAnimation>
#include <QtGlobal>

namespace pokersoc::ui::qt::settings {
namespace {

struct Token {
    const char* name;
    const char* value;
};

constexpr Token kTokens[] = {
    {"bg", "#F6F7F9"},          {"surface", "#FFFFFF"},      {"tile", "#F9FAFB"},
    {"border", "#E5E7EB"},      {"divider", "#EEF0F3"},      {"hover", "#F3F4F6"},
    {"pressed", "#E5E7EB"},     {"text", "#111827"},         {"secondary", "#4B5563"},
    {"muted", "#6B7280"},       {"subtle", "#9CA3AF"},       {"track", "#D1D5DB"},
    {"trackHover", "#C2C7CF"},  {"accent", "#2563EB"},       {"accentHover", "#1D4ED8"},
    {"accentPressed", "#1E40AF"}, {"accentSoft", "#EFF6FF"}, {"accentTint", "#DBEAFE"},
    {"accentRing", "#93C5FD"},  {"danger", "#DC2626"},       {"warning", "#D97706"},
    {"success", "#16A34A"},
};

constexpr char kPageStyle[] = R"(
#tsPage { background: @bg; }

#headerBar { background: @bg; border: none; border-bottom: 1px solid transparent; }
#headerBar[scrolled="true"] { border-bottom-color: @border; }
#pageTitle { color: @text; font-size: 22px; font-weight: 600; }
#pageSubtitle { color: @muted; font-size: 13px; }

#card { background: @surface; border: 1px solid @border; border-radius: 12px; }
#cardTitle { color: @text; font-size: 15px; font-weight: 600; }
#cardDescription { color: @muted; font-size: 13px; }

#rowTitle { color: @text; font-size: 14px; font-weight: 500; }
#rowDescription { color: @muted; font-size: 13px; }
#rowTitle:disabled, #rowDescription:disabled { color: @subtle; }
#divider { background: @divider; border: none; }

#chip { background: @hover; color: @secondary; border-radius: 11px; padding: 0 10px;
        font-size: 12px; font-weight: 500; }
#summaryTile { background: @tile; border: 1px solid @divider; border-radius: 10px; }
#overline { color: @subtle; font-size: 11px; font-weight: 600; }
#summaryText { color: @text; font-size: 14px; }
#emptyState { color: @subtle; font-size: 13px; }

#clockTime { color: @text; font-size: 56px; font-weight: 700; }
#clockTime[tone="live"] { color: @success; }
#clockTime[tone="paused"] { color: @warning; }
#clockTime[tone="ended"] { color: @danger; }
#clockMeta { color: @text; font-size: 15px; font-weight: 600; }
#clockStatus { font-size: 13px; font-weight: 700; }
#clockStatus[tone="live"] { color: @success; }
#clockStatus[tone="paused"] { color: @warning; }
#clockStatus[tone="ended"] { color: @danger; }
#clockStatus[tone="break"] { color: @accent; }
#clockBlinds { color: @text; font-size: 14px; font-weight: 600; }
#clockNext { color: @muted; font-size: 13px; }
#clockPot { color: @secondary; font-size: 13px; font-weight: 500; }

#levelJump { background: @surface; border: 1px solid @border; border-radius: 8px;
             padding: 8px; color: @text; font-size: 13px; font-weight: 500; }
#levelJump:hover { background: @hover; }
#levelJump[current="true"] { background: @accentSoft; border-color: @accent; color: @accentHover; }

QPushButton { border-radius: 8px; padding: 0 16px; font-size: 14px; font-weight: 500; }
QPushButton:disabled { color: @subtle; }
QPushButton[variant="primary"] { background: @accent; border: 1px solid @accent; color: #FFFFFF; }
QPushButton[variant="primary"]:hover { background: @accentHover; border-color: @accentHover; }
QPushButton[variant="primary"]:pressed { background: @accentPressed; border-color: @accentPressed; }
QPushButton[variant="secondary"] { background: @surface; border: 1px solid @border; color: @text; }
QPushButton[variant="secondary"]:hover { background: @hover; }
QPushButton[variant="secondary"]:pressed { background: @pressed; }
QPushButton[variant="ghost"] { background: transparent; border: 1px solid transparent; color: @muted; }
QPushButton[variant="ghost"]:hover { background: @hover; color: @text; }
QPushButton[variant="ghost"]:pressed { background: @pressed; }
QPushButton[variant="ghost"]:disabled { background: transparent; color: @track; }
QPushButton[variant="danger"] { background: @surface; border: 1px solid @border; color: @danger; }
QPushButton[variant="danger"]:hover { background: #FEF2F2; border-color: #FECACA; }
QPushButton[variant="primary"][focusVisible="true"] { border: 2px solid @accentRing; padding: 0 15px; }
QPushButton[variant="secondary"][focusVisible="true"],
QPushButton[variant="ghost"][focusVisible="true"],
QPushButton[variant="danger"][focusVisible="true"] { border: 2px solid @accent; padding: 0 15px; }

QToolButton#iconButton { background: transparent; border: none; border-radius: 8px; }
QToolButton#iconButton:hover { background: @pressed; }
QToolButton#iconButton[focusVisible="true"] { border: 2px solid @accent; }

QScrollArea#scroll, #scrollContent { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 4px 3px 4px 3px; border: none; }
QScrollBar::handle:vertical { background: @track; border-radius: 3px; min-height: 40px; }
QScrollBar::handle:vertical:hover { background: @subtle; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
)";

void repolish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

QColor token(const char* name) {
    for (const Token& t : kTokens)
        if (qstrcmp(t.name, name) == 0) return QColor(QLatin1String(t.value));
    return Qt::magenta;
}

QString themed(const QString& qss) {
    static const QRegularExpression ref(QStringLiteral("@([A-Za-z]+)"));
    QString out;
    out.reserve(qss.size());
    qsizetype last = 0;
    for (auto it = ref.globalMatch(qss); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        out += qss.mid(last, m.capturedStart() - last);
        out += token(m.captured(1).toLatin1().constData()).name();
        last = m.capturedEnd();
    }
    out += qss.mid(last);
    return out;
}

QString pageStyleSheet() { return themed(QLatin1String(kPageStyle)); }

QIcon glyphIcon(Glyph glyph, const QColor& color) {
    const auto render = [glyph](const QColor& c) {
        constexpr qreal kDpr = 3.0;
        QPixmap pixmap(QSize(16, 16) * kDpr);
        pixmap.setDevicePixelRatio(kDpr);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(c, 1.75, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        switch (glyph) {
        case Glyph::Close:
            p.drawLine(QPointF(4.5, 4.5), QPointF(11.5, 11.5));
            p.drawLine(QPointF(11.5, 4.5), QPointF(4.5, 11.5));
            break;
        case Glyph::Plus:
            p.drawLine(QPointF(8, 3.5), QPointF(8, 12.5));
            [[fallthrough]];
        case Glyph::Minus:
            p.drawLine(QPointF(3.5, 8), QPointF(12.5, 8));
            break;
        case Glyph::Chevron: {
            const QPointF points[] = {{6.5, 3.5}, {11, 8}, {6.5, 12.5}};
            p.drawPolyline(points, 3);
            break;
        }
        }
        return pixmap;
    };
    QIcon icon;
    icon.addPixmap(render(color), QIcon::Normal);
    icon.addPixmap(render(token("track")), QIcon::Disabled);
    return icon;
}

QLabel* text(const QString& content, const char* role, bool wrap) {
    auto* label = new QLabel(content);
    label->setObjectName(QLatin1String(role));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(wrap);
    return label;
}

QPushButton* button(const QString& label, const char* variant) {
    auto* b = new QPushButton(label);
    b->setProperty("variant", QString::fromLatin1(variant));
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setFixedHeight(40);
    b->setIconSize(QSize(16, 16));
    return b;
}

QToolButton* iconButton(Glyph glyph, const QString& tooltip) {
    auto* b = new QToolButton;
    b->setObjectName(QStringLiteral("iconButton"));
    b->setIcon(glyphIcon(glyph, token("muted")));
    b->setIconSize(QSize(16, 16));
    b->setFixedSize(36, 36);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setToolTip(tooltip);
    b->setAccessibleName(tooltip);
    return b;
}

QFrame* divider() {
    auto* line = new QFrame;
    line->setObjectName(QStringLiteral("divider"));
    line->setFixedHeight(1);
    return line;
}

Card card(const QString& title, const QString& description, QWidget* accessory) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(0);
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(12);
    titleRow->addWidget(text(title, "cardTitle"), 1);
    if (accessory) titleRow->addWidget(accessory, 0, Qt::AlignVCenter);
    layout->addLayout(titleRow);
    layout->addSpacing(4);
    layout->addWidget(text(description, "cardDescription", true));
    layout->addSpacing(18);
    auto* body = new QVBoxLayout;
    body->setSpacing(0);
    layout->addLayout(body);
    return {frame, body};
}

QWidget* settingRow(const QString& title, const QString& description, QWidget* control) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    texts->addWidget(text(title, "rowTitle"));
    texts->addWidget(text(description, "rowDescription", true));
    layout->addLayout(texts, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QWidget* constrain(QWidget* inner, const QMargins& margins, QWidget* strip) {
    if (!strip) strip = new QWidget;
    auto* row = new QHBoxLayout(strip);
    row->setContentsMargins(margins);
    row->setSpacing(0);
    inner->setMaximumWidth(kMaxContentWidth);
    row->addStretch(1);
    row->addWidget(inner, 1000);
    row->addStretch(1);
    return strip;
}

Switch::Switch(QWidget* parent) : QAbstractButton(parent), anim_(new QVariantAnimation(this)) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    anim_->setDuration(150);
    anim_->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        pos_ = v.toReal();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        anim_->stop();
        if (!isVisible()) {
            pos_ = on ? 1.0 : 0.0;
            update();
            return;
        }
        anim_->setStartValue(pos_);
        anim_->setEndValue(on ? 1.0 : 0.0);
        anim_->start();
    });
}

QSize Switch::sizeHint() const { return {48, 32}; }

void Switch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled()) p.setOpacity(0.45);

    const QRectF track(4, 4, 40, 24);
    const bool hovered = isEnabled() && underMouse();
    if (hasFocus() && property("focusVisible").toBool()) {
        p.setPen(QPen(token("accentRing"), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-2.5, -2.5, 2.5, 2.5), 14.5, 14.5);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(mix(token(hovered ? "trackHover" : "track"), token(hovered ? "accentHover" : "accent"), pos_));
    p.drawRoundedRect(track, 12, 12);

    const QRectF knob(track.left() + 2 + pos_ * (track.width() - 24), track.top() + 2, 20, 20);
    p.setBrush(QColor(0, 0, 0, 40));
    p.drawEllipse(knob.translated(0, 1));
    p.setBrush(Qt::white);
    p.drawEllipse(knob);
}

QColor Switch::mix(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

bool FocusVisibleFilter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::FocusIn && event->type() != QEvent::FocusOut) return false;
    auto* widget = static_cast<QWidget*>(watched);
    const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
    const bool visible = event->type() == QEvent::FocusIn &&
                         (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason ||
                          reason == Qt::ShortcutFocusReason);
    if (widget->property("focusVisible").toBool() != visible) {
        widget->setProperty("focusVisible", visible);
        repolish(widget);
    }
    return false;
}

OverlayScrollArea::OverlayScrollArea(QWidget* parent)
    : QScrollArea(parent), overlay_(new QScrollBar(Qt::Vertical, this)) {
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScrollBar* real = verticalScrollBar();
    connect(real, &QScrollBar::rangeChanged, overlay_, [this, real](int min, int max) {
        overlay_->setRange(min, max);
        overlay_->setPageStep(real->pageStep());
        overlay_->setSingleStep(real->singleStep());
        overlay_->setVisible(max > min);
    });
    connect(real, &QScrollBar::valueChanged, overlay_, &QScrollBar::setValue);
    connect(overlay_, &QScrollBar::valueChanged, real, &QScrollBar::setValue);
    overlay_->hide();
}

void OverlayScrollArea::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);
    const int w = overlay_->sizeHint().width();
    overlay_->setGeometry(width() - w, 0, w, height());
    overlay_->setPageStep(verticalScrollBar()->pageStep());
    overlay_->raise();
}

void installFocusVisible(QWidget* page) {
    auto* filter = new FocusVisibleFilter(page);
    for (QAbstractButton* b : page->findChildren<QAbstractButton*>()) b->installEventFilter(filter);
    page->setFocusPolicy(Qt::ClickFocus);
}

} // namespace pokersoc::ui::qt::settings
