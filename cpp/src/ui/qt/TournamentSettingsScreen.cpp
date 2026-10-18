#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Style.h"
#include "ui/qt/TournamentEditors.h"

#include "domain/session/TournamentMode.h"

#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIODevice>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEasingCurve>
#include <QEvent>
#include <QFocusEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
#include <exception>
#include <functional>
#include <vector>

namespace pokersoc::ui::qt {

using namespace domain;

namespace {
namespace tsui {

// ── Design tokens ────────────────────────────────────────────────────────────
// Single source of truth for colour. Stylesheets reference these as @name and
// the custom-painted widgets read them through token(), so a retheme is one edit.
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
};

QColor token(const char* name) {
    for (const Token& t : kTokens)
        if (qstrcmp(t.name, name) == 0) return QColor(QLatin1String(t.value));
    qWarning("Unknown design token: %s", name);
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

constexpr int kMaxContentWidth = 800;
constexpr int kRowHeight = 44;
constexpr int kHeaderHeight = 36;
enum BlindColumn { kLevelCol = 0, kSmallCol, kBigCol, kAnteCol, kMinutesCol };

// Focus styles key off [focusVisible="true"] (keyboard focus only), not :focus. See FocusVisibleFilter.
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

#textField { background: @surface; border: 1px solid @border; border-radius: 8px;
             padding: 8px 12px; color: @text; font-size: 14px; min-width: 220px;
             selection-background-color: @accentTint; selection-color: @text; }
#textField:focus { border-color: @accent; }
#combo { background: @surface; border: 1px solid @border; border-radius: 8px;
         padding: 8px 12px; color: @text; font-size: 14px; min-width: 200px; }
#combo:focus { border-color: @accent; }
#combo::drop-down { border: none; width: 22px; }
#combo QAbstractItemView { background: @surface; color: @text;
                           selection-background-color: @accentSoft; }

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
QPushButton[variant="primary"][focusVisible="true"] { border: 2px solid @accentRing; padding: 0 15px; }
QPushButton[variant="secondary"][focusVisible="true"],
QPushButton[variant="ghost"][focusVisible="true"] { border: 2px solid @accent; padding: 0 15px; }

QToolButton#iconButton { background: transparent; border: none; border-radius: 8px; }
QToolButton#iconButton:hover { background: @pressed; }
QToolButton#iconButton[focusVisible="true"] { border: 2px solid @accent; }

#stepper { background: @surface; border: 1px solid @border; border-radius: 8px; }
#stepper:disabled { background: @tile; border-color: @divider; }
#stepper QToolButton { background: transparent; border: none; }
#stepper QToolButton:hover, #stepper QToolButton[focusVisible="true"] { background: @hover; }
#stepper QToolButton:pressed { background: @pressed; }
#stepDown { border-top-left-radius: 7px; border-bottom-left-radius: 7px; }
#stepUp { border-top-right-radius: 7px; border-bottom-right-radius: 7px; }
#stepper QSpinBox { background: transparent; border: none; border-left: 1px solid @divider;
                    border-right: 1px solid @divider; color: @text; font-size: 14px; font-weight: 600;
                    selection-background-color: @accentTint; selection-color: @text; }
#stepper QSpinBox:focus { background: @accentSoft; }
#stepper QSpinBox:disabled { color: @subtle; }

#footer { background: @surface; border: none; border-top: 1px solid @border; }
#status { color: @muted; font-size: 13px; }
#status[tone="error"] { color: @danger; }

QScrollArea#scroll, #scrollContent { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 4px 3px 4px 3px; border: none; }
QScrollBar::handle:vertical { background: @track; border-radius: 3px; min-height: 40px; }
QScrollBar::handle:vertical:hover { background: @subtle; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
)";

// Set on the table itself so it beats any sheet makeEditorTable() applied.
constexpr char kTableStyle[] = R"(
QTableWidget { background: @surface; border: none; outline: 0; color: @text; font-size: 14px;
               gridline-color: transparent; selection-background-color: @accentSoft; selection-color: @text; }
QTableWidget::item { border: none; border-bottom: 1px solid @divider; padding: 0 4px 0 9px; }
QTableWidget::item:hover { background: @tile; }
QTableWidget::item:selected { background: @accentSoft; color: @text; }
QHeaderView { background: @surface; border: none; }
QHeaderView::section { background: @surface; color: @muted; border: none; border-bottom: 1px solid @border;
                       padding: 0 4px 0 12px; font-size: 12px; font-weight: 500; }
QTableCornerButton::section { background: @surface; border: none; }
#cellEditor { background: @surface; border: 1px solid @accent; border-radius: 6px; padding: 0 5px;
              color: @text; font-size: 14px; selection-background-color: @accentTint; selection-color: @text; }
)";

// ── Icons ────────────────────────────────────────────────────────────────────
// Stroked at 3x so they stay crisp at any Windows display scaling; no font or resource dependency.
enum class Glyph { Close, Plus, Minus, Chevron };

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

// ── Widgets ──────────────────────────────────────────────────────────────────
// Animated on/off switch. No Q_OBJECT (and so no moc) needed: it only reuses
// QAbstractButton's checkable state and toggled() signal.
class Switch final : public QAbstractButton {
public:
    explicit Switch(QWidget* parent = nullptr) : QAbstractButton(parent) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);  // focus ring only appears for keyboard users
        setAttribute(Qt::WA_Hover);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        anim_.setDuration(150);
        anim_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&anim_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            pos_ = v.toReal();
            update();
        });
        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            anim_.stop();
            if (!isVisible()) {  // initial state: snap instead of animating on open
                pos_ = on ? 1.0 : 0.0;
                update();
                return;
            }
            anim_.setStartValue(pos_);
            anim_.setEndValue(on ? 1.0 : 0.0);
            anim_.start();
        });
    }

    QSize sizeHint() const override { return {48, 32}; }

protected:
    void paintEvent(QPaintEvent*) override {
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

private:
    static QColor mix(const QColor& a, const QColor& b, qreal t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    }

    QVariantAnimation anim_;
    qreal pos_ = 0.0;
};

void repolish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

// Qt's :focus also matches mouse and programmatic focus (e.g. the first button when the screen
// opens). This mirrors CSS :focus-visible: only keyboard focus sets focusVisible=true.
class FocusVisibleFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
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
};

// The scrollbar floats over the right margin instead of taking layout width, so the content
// column stays aligned with the header and footer whether or not the page scrolls.
class OverlayScrollArea final : public QScrollArea {
public:
    explicit OverlayScrollArea(QWidget* parent = nullptr)
        : QScrollArea(parent), overlay_(new QScrollBar(Qt::Vertical, this)) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // still drives wheel/keyboard scrolling
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

protected:
    void resizeEvent(QResizeEvent* event) override {
        QScrollArea::resizeEvent(event);
        const int w = overlay_->sizeHint().width();
        overlay_->setGeometry(width() - w, 0, w, height());
        overlay_->setPageStep(verticalScrollBar()->pageStep());
        overlay_->raise();
    }

private:
    QScrollBar* overlay_;
};

// Ignores the wheel unless focused, so scrolling the page never edits the value under the cursor.
class CalmSpinBox final : public QSpinBox {
public:
    explicit CalmSpinBox(QWidget* parent = nullptr) : QSpinBox(parent) { setFocusPolicy(Qt::StrongFocus); }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (hasFocus())
            QSpinBox::wheelEvent(event);
        else
            event->ignore();
    }
};

// Same as CalmSpinBox: page wheel must not change the selection under the cursor.
class CalmComboBox final : public QComboBox {
public:
    explicit CalmComboBox(QWidget* parent = nullptr) : QComboBox(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!hasFocus()) {
            event->ignore();
            return;
        }
        QComboBox::wheelEvent(event);
    }
};

// Blinds table: free-text chip amounts (1.5K / 150K), spin for minutes, level as a pill.
class BlindCellDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        Q_UNUSED(option);
        if (index.column() == kLevelCol) return nullptr;
        if (index.column() != kMinutesCol && index.siblingAtColumn(kLevelCol).data(Qt::UserRole).toBool())
            return nullptr;
        if (index.column() == kMinutesCol) {
            auto* spin = new QSpinBox(parent);
            spin->setObjectName(QStringLiteral("cellEditor"));
            spin->setFrame(false);
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            spin->setRange(0, 999);
            spin->setSuffix(QStringLiteral(" min"));
            return spin;
        }
        auto* edit = new QLineEdit(parent);
        edit->setObjectName(QStringLiteral("cellEditor"));
        edit->setFrame(false);
        edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        edit->setPlaceholderText(index.column() == kAnteCol ? QStringLiteral("0 or 1.5K")
                                                            : QStringLiteral("100 or 1.5K"));
        return edit;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->setValue(index.data(Qt::EditRole).toString().trimmed().toInt());
            return;
        }
        if (auto* edit = qobject_cast<QLineEdit*>(editor)) {
            QString text = index.data(Qt::EditRole).toString().trimmed();
            if (text == QStringLiteral("\u2014")) text.clear();
            edit->setText(text);
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            spin->interpretText();
            model->setData(index, spin->value(), Qt::EditRole);
            return;
        }
        if (auto* edit = qobject_cast<QLineEdit*>(editor)) {
            model->setData(index, edit->text().trimmed(), Qt::EditRole);
            return;
        }
        QStyledItemDelegate::setModelData(editor, model, index);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex&) const override {
        editor->setGeometry(option.rect.adjusted(6, 6, -6, -7));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if (index.column() != kLevelCol) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QString level = opt.text;
        opt.text.clear();
        const QWidget* view = opt.widget;
        QStyle* style = view ? view->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, view);  // background, hover, row rule
        if (level.isEmpty()) return;

        const bool selected = opt.state & QStyle::State_Selected;
        QFont font = opt.font;
        font.setPixelSize(12);
        font.setWeight(QFont::DemiBold);
        const int width = std::max(28, QFontMetrics(font).horizontalAdvance(level) + 16);
        const QRect pill(opt.rect.left() + 12, opt.rect.center().y() - 11, width, 22);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(token(selected ? "accentTint" : "hover"));
        painter->drawRoundedRect(pill, 6, 6);
        painter->setFont(font);
        painter->setPen(token(selected ? "accentHover" : "secondary"));
        painter->drawText(pill, Qt::AlignCenter, level);
        painter->restore();
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->state &= ~QStyle::State_HasFocus;  // no dotted focus rectangle
        const QString raw = index.data(Qt::DisplayRole).toString().trimmed();
        switch (index.column()) {
        case kLevelCol: break;
        case kMinutesCol: {
            bool ok = false;
            const int value = raw.toInt(&ok);
            if (ok) option->text = QStringLiteral("%1 min").arg(value);
            break;
        }
        case kAnteCol:
            option->text = (raw.isEmpty() || raw == QStringLiteral("0") || raw == QStringLiteral("\u2014"))
                               ? QStringLiteral("\u2014")
                               : raw;
            break;
        default:
            option->text = raw;
            break;
        }
    }
};

// ── Builders ─────────────────────────────────────────────────────────────────
QLabel* text(const QString& content, const char* role, bool wrap = false) {
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
    b->setFixedHeight(36);
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

struct Card {
    QFrame* frame;
    QVBoxLayout* body;
};

Card card(const QString& title, const QString& description, QWidget* accessory = nullptr) {
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

// Label + description on the left, control on the right — the standard settings row.
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

// [ − | 2 | + ] around an existing spin box.
QFrame* stepper(QSpinBox* spin) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("stepper"));
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    const auto stepButton = [](Glyph glyph, const char* name, const QString& tooltip) {
        auto* b = new QToolButton;
        b->setObjectName(QLatin1String(name));
        b->setIcon(glyphIcon(glyph, token("secondary")));
        b->setIconSize(QSize(16, 16));
        b->setFixedSize(34, 34);
        b->setAutoRepeat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tooltip);
        b->setAccessibleName(tooltip);
        return b;
    };
    QToolButton* down = stepButton(Glyph::Minus, "stepDown", QStringLiteral("Decrease"));
    QToolButton* up = stepButton(Glyph::Plus, "stepUp", QStringLiteral("Increase"));
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setAlignment(Qt::AlignCenter);
    spin->setFrame(false);
    spin->setFixedSize(56, 34);
    layout->addWidget(down);
    layout->addWidget(spin);
    layout->addWidget(up);
    QObject::connect(down, &QToolButton::clicked, spin, &QSpinBox::stepDown);
    QObject::connect(up, &QToolButton::clicked, spin, &QSpinBox::stepUp);
    const auto sync = [spin, down, up] {
        down->setEnabled(spin->value() > spin->minimum());
        up->setEnabled(spin->value() < spin->maximum());
    };
    QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), frame, sync);
    sync();
    return frame;
}

// Centres `inner` in a full-width strip and caps its width so lines stay readable on wide windows.
QWidget* constrain(QWidget* inner, const QMargins& margins, QWidget* strip = nullptr) {
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

// Moves a whole blind row. QTableWidget's built-in move duplicates cells.
class BlindTable final : public QTableWidget {
public:
    explicit BlindTable(QWidget* parent = nullptr) : QTableWidget(parent) {
        setDragEnabled(true);
        setAcceptDrops(true);
        // Copy, not Move: Qt deletes the dragged row itself after a MoveAction,
        // which removed the level after we had already relocated it.
        setDragDropMode(QAbstractItemView::DragDrop);
        setDragDropOverwriteMode(false);
        setDropIndicatorShown(true);
        setDefaultDropAction(Qt::CopyAction);
    }

    std::function<void()> onReordered;

protected:
    void startDrag(Qt::DropActions) override {
        dragRow_ = currentRow();
        QTableWidget::startDrag(Qt::CopyAction);
    }

    void dragEnterEvent(QDragEnterEvent* event) override {
        if (event->source() == this) event->acceptProposedAction();
        else event->ignore();
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (event->source() == this) event->acceptProposedAction();
        else event->ignore();
    }

    void dropEvent(QDropEvent* event) override {
        if (event->source() != this) {
            event->ignore();
            return;
        }
        const int from = dragRow_;
        const QModelIndex over = indexAt(event->position().toPoint());
        int to = over.isValid() ? over.row() : rowCount();
        if (over.isValid() && event->position().toPoint().y() > visualRect(over).center().y()) ++to;
        event->setDropAction(Qt::CopyAction);
        event->accept();
        if (from < 0 || from >= rowCount() || from == to || from + 1 == to) return;

        const bool blocked = blockSignals(true);
        QList<QTableWidgetItem*> taken;
        taken.reserve(columnCount());
        for (int column = 0; column < columnCount(); ++column) taken.append(takeItem(from, column));
        removeRow(from);
        if (to > from) --to;
        insertRow(to);
        for (int column = 0; column < taken.size(); ++column) setItem(to, column, taken.at(column));
        blockSignals(blocked);
        selectRow(to);
        if (onReordered) onReordered();
    }

private:
    int dragRow_ = -1;
};

void styleBlindsTable(QTableWidget* table) {
    table->setHorizontalHeaderLabels({QStringLiteral("Level"), QStringLiteral("Small blind"),
                                      QStringLiteral("Big blind"), QStringLiteral("Big blind ante"),
                                      QStringLiteral("Duration")});
    table->setItemDelegate(new BlindCellDelegate(table));
    table->setStyleSheet(themed(QLatin1String(kTableStyle)));
    table->setFrameShape(QFrame::NoFrame);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setCornerButtonEnabled(false);
    table->setAlternatingRowColors(false);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::AnyKeyPressed);
    // The page scrolls, not the table: no nested scrollbars.
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setMouseTracking(true);
    table->viewport()->setAttribute(Qt::WA_Hover);

    table->verticalHeader()->hide();
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    QHeaderView* header = table->horizontalHeader();
    header->setFixedHeight(kHeaderHeight);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setHighlightSections(false);
    header->setSectionsClickable(false);
    header->setSectionResizeMode(QHeaderView::Stretch);
    header->setSectionResizeMode(kLevelCol, QHeaderView::Fixed);
    header->resizeSection(kLevelCol, 96);
}

void fitTableToRows(QTableWidget* table) {
    int height = table->horizontalHeader()->height() + 2 * table->frameWidth();
    for (int row = 0; row < table->rowCount(); ++row) height += table->rowHeight(row);
    table->setFixedHeight(height);
}

void revealRow(QScrollArea* scroll, QTableWidget* table, int row) {
    if (row < 0 || row >= table->rowCount()) return;
    const QRect cell = table->visualRect(table->model()->index(row, 0));
    const QPoint bottom = table->viewport()->mapTo(scroll->widget(), cell.bottomLeft());
    scroll->ensureVisible(bottom.x(), bottom.y(), 0, 96);  // margin keeps the action row in view too
}

// Flush an open cell editor or a half-typed spin box before reading values.
void commitPendingEdits(QWidget* scope) {
    if (QWidget* focused = QApplication::focusWidget(); focused && scope->isAncestorOf(focused))
        focused->clearFocus();
}

QString durationText(int minutes) {
    if (minutes < 60) return QStringLiteral("%1 min").arg(minutes);
    const int h = minutes / 60;
    const int m = minutes % 60;
    return m == 0 ? QStringLiteral("%1 h").arg(h) : QStringLiteral("%1 h %2 min").arg(h).arg(m);
}

QString blindsSummary(int levels, int breaks, int minutes) {
    const QString count = levels == 1 ? QStringLiteral("1 level") : QStringLiteral("%1 levels").arg(levels);
    QString text = count;
    if (breaks == 1) text += QStringLiteral(" \u00B7 1 break");
    else if (breaks > 1) text += QStringLiteral(" \u00B7 %1 breaks").arg(breaks);
    return text + QStringLiteral(" \u00B7 ") + durationText(minutes);
}

}  // namespace tsui
}  // namespace

TournamentSettingsScreen::TournamentSettingsScreen(SessionContext& context, QWidget* parent)
    : Screen(context, parent) {
    const TournamentConfig& initial = ctx_.tournament().config();

    // ── Shell: header · scrolling cards · sticky footer ─────────────────────
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("tsPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setStyleSheet(tsui::themed(QLatin1String(tsui::kPageStyle)));
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
    headings->addWidget(tsui::text("Tournament settings", "pageTitle"));
    headings->addWidget(tsui::text("Pricing, rebuys and blind structure for this tournament.", "pageSubtitle", true));
    headerRow->addLayout(headings, 1);
    QToolButton* close = tsui::iconButton(tsui::Glyph::Close, "Close");
    headerRow->addWidget(close, 0, Qt::AlignTop);
    auto* headerBar = new QFrame;
    headerBar->setObjectName(QStringLiteral("headerBar"));
    pageLayout->addWidget(tsui::constrain(header, {32, 28, 32, 20}, headerBar));
    connect(close, &QToolButton::clicked, this, [this] { emit closed(true); });

    auto* column = new QWidget;
    auto* cards = new QVBoxLayout(column);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setSpacing(16);

    // ── Pricing ─────────────────────────────────────────────────────────────
    tsui::Card pricingCard = tsui::card(
        "Pricing", "Choose ARC vs Non-ARC or membership-tier pricing, and set buy-in, bounty and rake together.");
    auto* tile = new QFrame;
    tile->setObjectName(QStringLiteral("summaryTile"));
    auto* tileRow = new QHBoxLayout(tile);
    tileRow->setContentsMargins(16, 14, 14, 14);
    tileRow->setSpacing(16);
    auto* tileText = new QVBoxLayout;
    tileText->setSpacing(4);
    tileText->addWidget(tsui::text("CURRENT STRUCTURE", "overline"));
    const QString summary = toQ(initial.pricingSummary());
    tileText->addWidget(tsui::text(summary.isEmpty() ? QStringLiteral("Not set up yet") : summary, "summaryText", true));
    tileRow->addLayout(tileText, 1);
    QPushButton* configure = tsui::button("Configure", "secondary");
    configure->setIcon(tsui::glyphIcon(tsui::Glyph::Chevron, tsui::token("muted")));
    configure->setLayoutDirection(Qt::RightToLeft);  // chevron after the label
    tileRow->addWidget(configure, 0, Qt::AlignVCenter);
    pricingCard.body->addWidget(tile);
    cards->addWidget(pricingCard.frame);
    connect(configure, &QPushButton::clicked, this, [this] { emit configurePricing(); });

    // ── Rebuys ──────────────────────────────────────────────────────────────
    tsui::Card rebuysCard = tsui::card("Rebuys", "Control how many times a player can buy back in.");
    auto* unlimited = new tsui::Switch;
    unlimited->setObjectName(QStringLiteral("unlimitedRebuys"));
    unlimited->setAccessibleName(QStringLiteral("Unlimited rebuys"));
    rebuysCard.body->addWidget(tsui::settingRow("Unlimited rebuys", "No cap on rebuys for any player.", unlimited));
    rebuysCard.body->addSpacing(14);
    rebuysCard.body->addWidget(tsui::divider());
    rebuysCard.body->addSpacing(14);
    auto* cap = new tsui::CalmSpinBox;
    cap->setObjectName(QStringLiteral("rebuyCap"));
    cap->setAccessibleName(QStringLiteral("Rebuys per player"));
    cap->setRange(1, 999);
    QWidget* capRow =
        tsui::settingRow("Rebuys per player", "On top of each player's initial buy-in.", tsui::stepper(cap));
    rebuysCard.body->addWidget(capRow);
    cards->addWidget(rebuysCard.frame);

    const bool isUnlimited = initial.rebuyCap <= 0;
    unlimited->setChecked(isUnlimited);
    cap->setValue(isUnlimited ? 2 : initial.rebuyCap);
    capRow->setEnabled(!isUnlimited);
    connect(unlimited, &QAbstractButton::toggled, capRow, [capRow](bool on) { capRow->setEnabled(!on); });

    // ── Event ───────────────────────────────────────────────────────────────
    tsui::Card eventCard = tsui::card(
        "Event", "Name the tournament and set when registration closes on the TV board.");
    auto* nameEdit = new QLineEdit;
    nameEdit->setObjectName(QStringLiteral("textField"));
    nameEdit->setPlaceholderText(QStringLiteral("Tournament name"));
    nameEdit->setMaxLength(80);
    nameEdit->setText(toQ(initial.name));
    eventCard.body->addWidget(
        tsui::settingRow("Tournament name", "Shown at the top centre of the TV board.", nameEdit));
    eventCard.body->addSpacing(14);
    eventCard.body->addWidget(tsui::divider());
    eventCard.body->addSpacing(14);
    auto* stack = new tsui::CalmSpinBox;
    stack->setObjectName(QStringLiteral("startingStack"));
    stack->setAccessibleName(QStringLiteral("Starting stack"));
    stack->setRange(100, 99999999);
    stack->setSingleStep(100);
    stack->setGroupSeparatorShown(true);
    stack->setValue(initial.startingStack);
    eventCard.body->addWidget(tsui::settingRow(
        "Starting stack", "Chips each buy-in and re-entry puts in play, for average stack.",
        tsui::stepper(stack)));
    eventCard.body->addSpacing(14);
    eventCard.body->addWidget(tsui::divider());
    eventCard.body->addSpacing(14);
    auto* regoBox = new tsui::CalmComboBox;
    regoBox->setObjectName(QStringLiteral("combo"));
    regoBox->setAccessibleName(QStringLiteral("Registration ends"));
    eventCard.body->addWidget(tsui::settingRow(
        "Registration ends", "Closes when this playing level begins. Breaks are not counted.",
        regoBox));
    cards->addWidget(eventCard.frame);

    // ── Board chips ─────────────────────────────────────────────────────────
    tsui::Card chipsCard = tsui::card(
        "Board chips",
        "Up to six denominations for the TV board (2 columns × 3 rows). "
        "Choose an image from Assets/BoardChips, or Browse… to pick any filepath. "
        "Values accept K/M (e.g. 1.5K, 100K). Highest value sits bottom-right.");
    auto* chipsList = new QVBoxLayout;
    chipsList->setSpacing(10);
    chipsCard.body->addLayout(chipsList);
    chipsCard.body->addSpacing(12);
    auto* chipActions = new QHBoxLayout;
    chipActions->setSpacing(8);
    QPushButton* addChip = tsui::button("Add chip", "secondary");
    QPushButton* removeChip = tsui::button("Remove last", "ghost");
    chipActions->addWidget(addChip);
    chipActions->addStretch(1);
    chipActions->addWidget(removeChip);
    chipsCard.body->addLayout(chipActions);
    cards->addWidget(chipsCard.frame);

    struct ChipRowWidgets {
        QWidget* row = nullptr;
        tsui::CalmComboBox* image = nullptr;
        QPushButton* browse = nullptr;
        QLineEdit* value = nullptr;
    };
    auto chipRows = std::make_shared<std::vector<ChipRowWidgets>>();

    const auto boardChipImages = [] {
        QStringList names;
        const QDir dir(assetPath(QStringLiteral("BoardChips")));
        for (const QFileInfo& info : dir.entryInfoList(
                 {QStringLiteral("*.png"), QStringLiteral("*.PNG"), QStringLiteral("*.jpg"),
                  QStringLiteral("*.jpeg"), QStringLiteral("*.webp")},
                 QDir::Files, QDir::Name)) {
            names += info.fileName();
        }
        return names;
    };

    const auto fillImageCombo = [boardChipImages](QComboBox* box, const QString& selected) {
        box->clear();
        box->addItem(QStringLiteral("(none)"), QString());
        for (const QString& name : boardChipImages()) {
            box->addItem(name, name);
        }
        if (!selected.isEmpty()) {
            int idx = box->findData(selected);
            if (idx < 0) {
                // Custom / absolute path chosen earlier.
                box->addItem(QFileInfo(selected).fileName(), selected);
                idx = box->count() - 1;
            }
            box->setCurrentIndex(idx);
        } else {
            box->setCurrentIndex(0);
        }
    };

    // ── Blind structure ─────────────────────────────────────────────────────
    QLabel* chip = tsui::text(QString(), "chip");
    chip->setFixedHeight(22);  // QSS drops border-radius entirely if it exceeds half the height
    tsui::Card blindsCard = tsui::card(
        "Blind structure",
        "Double-click a cell to edit (numbers or shorthand like 1.5K / 150K). Drag a row to reorder. "
        "Import or save a JSON rounds file. A break keeps the blinds and only runs the clock.",
        chip);
    auto* blindsTable = new tsui::BlindTable;
    blindsTable->setColumnCount(5);
    for (const BlindLevel& level : initial.blinds) addBlindRow(blindsTable, level);
    refreshBlindLabels(blindsTable);
    blindsTable->setObjectName(QStringLiteral("blindsTable"));
    tsui::styleBlindsTable(blindsTable);
    blindsCard.body->addWidget(blindsTable);
    QLabel* empty = tsui::text("No levels yet. Add one to get started.", "emptyState");
    empty->setAlignment(Qt::AlignCenter);
    empty->setMinimumHeight(72);
    blindsCard.body->addWidget(empty);
    blindsCard.body->addSpacing(16);
    auto* blindActions = new QHBoxLayout;
    blindActions->setSpacing(8);
    QPushButton* addBlind = tsui::button("Add level", "secondary");
    addBlind->setIcon(tsui::glyphIcon(tsui::Glyph::Plus, tsui::token("text")));
    QPushButton* addBreak = tsui::button("Add break", "secondary");
    QPushButton* importBlinds = tsui::button("Import…", "ghost");
    QPushButton* saveBlinds = tsui::button("Save…", "ghost");
    QPushButton* removeBlind = tsui::button("Remove last", "ghost");
    blindActions->addWidget(addBlind);
    blindActions->addWidget(addBreak);
    blindActions->addWidget(importBlinds);
    blindActions->addWidget(saveBlinds);
    blindActions->addStretch(1);
    blindActions->addWidget(removeBlind);
    blindsCard.body->addLayout(blindActions);
    cards->addWidget(blindsCard.frame);
    cards->addStretch(1);

    auto* scroll = new tsui::OverlayScrollArea;
    scroll->setObjectName(QStringLiteral("scroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget* content = tsui::constrain(column, {32, 0, 32, 32});
    content->setObjectName(QStringLiteral("scrollContent"));
    scroll->setWidget(content);
    pageLayout->addWidget(scroll, 1);
    // Hairline under the header only once content has scrolled beneath it.
    connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, headerBar, [headerBar](int value) {
        if (headerBar->property("scrolled").toBool() == (value > 0)) return;
        headerBar->setProperty("scrolled", value > 0);
        tsui::repolish(headerBar);
    });

    // ── Behaviour ───────────────────────────────────────────────────────────
    auto quiet = std::make_shared<bool>(false);
    const auto rebuildRego = [regoBox, blindsTable, quiet](int select) {
        *quiet = true;
        regoBox->clear();
        regoBox->addItem(QStringLiteral("Never"), 0);
        int playing = 0;
        for (int row = 0; row < blindsTable->rowCount(); ++row) {
            if (blindRowIsBreak(blindsTable, row)) continue;
            ++playing;
            regoBox->addItem(QStringLiteral("Start of level %1").arg(playing), playing);
        }
        int idx = regoBox->findData(select);
        if (idx < 0) idx = 0;
        regoBox->setCurrentIndex(idx);
        *quiet = false;
    };
    const auto syncBlinds = [blindsTable, chip, empty, removeBlind, rebuildRego, regoBox] {
        int minutes = 0;
        int levels = 0;
        int breaks = 0;
        for (int row = 0; row < blindsTable->rowCount(); ++row) {
            blindsTable->setRowHeight(row, tsui::kRowHeight);
            bool ok = false;
            const int m = cellInt(blindsTable, row, tsui::kMinutesCol, &ok);
            if (ok) minutes += m;
            if (blindRowIsBreak(blindsTable, row)) ++breaks;
            else ++levels;
        }
        tsui::fitTableToRows(blindsTable);
        chip->setText(tsui::blindsSummary(levels, breaks, minutes));
        chip->setVisible(blindsTable->rowCount() > 0);
        empty->setVisible(blindsTable->rowCount() == 0);
        removeBlind->setEnabled(blindsTable->rowCount() > 0);
        rebuildRego(regoBox->currentData().toInt());
    };
    rebuildRego(initial.registrationEndsLevel);
    syncBlinds();

    const auto syncChipButtons = [chipRows, addChip, removeChip] {
        addChip->setEnabled(static_cast<int>(chipRows->size()) < 6);
        removeChip->setEnabled(!chipRows->empty());
    };

    const auto persist = [this, unlimited, cap, blindsTable, quiet, nameEdit, stack, regoBox, chipRows] {
        if (*quiet) return;
        std::vector<BlindLevel> blinds;
        QString problem;
        if (blindsTable->rowCount() > 0 && !readBlinds(blindsTable, blinds, problem)) return;
        TournamentConfig& config = ctx_.tournament().config();
        config.rebuyCap = unlimited->isChecked() ? 0 : cap->value();
        config.name = toStd(nameEdit->text().trimmed());
        config.startingStack = stack->value();
        config.registrationEndsLevel = regoBox->currentData().toInt();
        config.blinds = std::move(blinds);
        config.boardChips.clear();
        for (const ChipRowWidgets& row : *chipRows) {
            BoardChip chip;
            chip.imageFile = toStd(row.image->currentData().toString());
            chip.value = toStd(row.value->text().trimmed());
            if (chip.imageFile.empty() && parseChipAmount(chip.value) <= 0) continue;
            config.boardChips.push_back(chip);
        }
        std::sort(config.boardChips.begin(), config.boardChips.end(),
                  [](const BoardChip& a, const BoardChip& b) {
                      return parseChipAmount(a.value) < parseChipAmount(b.value);
                  });
        try {
            ctx_.saveTournamentConfig();
        } catch (const std::exception& saveError) {
            QMessageBox::critical(this, "Tournament settings",
                                  QStringLiteral("Couldn't save settings: %1")
                                      .arg(QString::fromUtf8(saveError.what())));
            return;
        }
        emit structureChanged();
    };

    const auto addChipRow = [this, chipsList, chipRows, fillImageCombo, quiet, persist,
                             syncChipButtons](const BoardChip& chip) {
        if (static_cast<int>(chipRows->size()) >= 6) return;
        ChipRowWidgets widgets;
        widgets.row = new QWidget;
        auto* rowLayout = new QHBoxLayout(widgets.row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        widgets.image = new tsui::CalmComboBox;
        widgets.image->setObjectName(QStringLiteral("combo"));
        widgets.image->setMinimumWidth(140);
        fillImageCombo(widgets.image, toQ(chip.imageFile));
        widgets.browse = tsui::button("Browse…", "ghost");
        widgets.browse->setFixedWidth(88);
        widgets.value = new QLineEdit;
        widgets.value->setObjectName(QStringLiteral("textField"));
        widgets.value->setAccessibleName(QStringLiteral("Chip value"));
        widgets.value->setPlaceholderText(QStringLiteral("e.g. 1.5K"));
        widgets.value->setFixedWidth(96);
        widgets.value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        widgets.value->setText(toQ(chip.value));
        rowLayout->addWidget(tsui::text("Image", "caption"), 0);
        rowLayout->addWidget(widgets.image, 1);
        rowLayout->addWidget(widgets.browse, 0);
        rowLayout->addWidget(tsui::text("Value", "caption"), 0);
        rowLayout->addWidget(widgets.value, 0);
        chipsList->addWidget(widgets.row);
        chipRows->push_back(widgets);
        connect(widgets.image, qOverload<int>(&QComboBox::currentIndexChanged), this, [quiet, persist] {
            if (*quiet) return;
            persist();
        });
        connect(widgets.browse, &QPushButton::clicked, this,
                [this, image = widgets.image, quiet, persist] {
                    const QString chosen = QFileDialog::getOpenFileName(
                        this, QStringLiteral("Select chip image"),
                        assetPath(QStringLiteral("BoardChips")),
                        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.PNG *.JPG *.JPEG *.WEBP)"));
                    if (chosen.isEmpty()) return;
                    const QString path = QFileInfo(chosen).absoluteFilePath();
                    int idx = image->findData(path);
                    if (idx < 0) {
                        // Prefer listing a BoardChips filename when the file lives there.
                        const QDir boardDir(assetPath(QStringLiteral("BoardChips")));
                        const QString relative = boardDir.relativeFilePath(path);
                        const bool inBoardChips =
                            !relative.startsWith(QLatin1String("..")) && !QFileInfo(relative).isAbsolute();
                        const QString data = inBoardChips ? QFileInfo(path).fileName() : path;
                        const QString label = QFileInfo(path).fileName();
                        idx = image->findData(data);
                        if (idx < 0) {
                            image->addItem(label, data);
                            idx = image->count() - 1;
                        }
                    }
                    *quiet = true;
                    image->setCurrentIndex(idx);
                    *quiet = false;
                    persist();
                });
        connect(widgets.value, &QLineEdit::editingFinished, this, [quiet, persist] {
            if (*quiet) return;
            persist();
        });
        syncChipButtons();
    };

    *quiet = true;
    for (const BoardChip& chip : initial.boardChips) addChipRow(chip);
    *quiet = false;
    syncChipButtons();
    connect(addChip, &QPushButton::clicked, this, [addChipRow, persist] {
        addChipRow(BoardChip{});
        persist();
    });
    connect(removeChip, &QPushButton::clicked, this, [chipRows, persist, syncChipButtons] {
        if (chipRows->empty()) return;
        ChipRowWidgets last = chipRows->back();
        chipRows->pop_back();
        if (last.row) {
            last.row->setParent(nullptr);
            last.row->deleteLater();
        }
        syncChipButtons();
        persist();
    });
    const auto relabel = [blindsTable, quiet] {
        *quiet = true;
        refreshBlindLabels(blindsTable);
        *quiet = false;
    };

    connect(unlimited, &QAbstractButton::toggled, this, persist);
    connect(cap, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(nameEdit, &QLineEdit::textChanged, this, persist);
    connect(stack, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(regoBox, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(blindsTable, &QTableWidget::itemChanged, this, [syncBlinds, persist, quiet] {
        if (*quiet) return;
        syncBlinds();
        persist();
    });
    blindsTable->onReordered = [relabel, syncBlinds, persist] {
        relabel();
        syncBlinds();
        persist();
    };

    connect(addBlind, &QPushButton::clicked, this, [page, scroll, blindsTable, relabel, syncBlinds, persist] {
        tsui::commitPendingEdits(page);
        BlindLevel next{"25", "50", "", 15};
        for (int row = blindsTable->rowCount() - 1; row >= 0; --row) {
            if (blindRowIsBreak(blindsTable, row)) continue;
            bool ok = false;
            next.smallBlind = toStd(cellText(blindsTable, row, tsui::kSmallCol));
            next.bigBlind = toStd(cellText(blindsTable, row, tsui::kBigCol));
            next.ante = toStd(cellText(blindsTable, row, tsui::kAnteCol));
            next.durationMinutes = cellInt(blindsTable, row, tsui::kMinutesCol, &ok);
            const long long bb = parseChipAmount(next.bigBlind);
            if (bb > 0) {
                next.smallBlind = next.bigBlind;
                next.bigBlind = formatChipAmount(bb * 2);
                const long long ante = parseChipAmount(next.ante);
                if (ante > 0) next.ante = formatChipAmount(ante * 2);
            }
            break;
        }
        addBlindRow(blindsTable, next);
        relabel();
        syncBlinds();
        persist();
        const int added = blindsTable->rowCount() - 1;
        blindsTable->setCurrentCell(added, tsui::kSmallCol);
        blindsTable->setFocus();
        QTimer::singleShot(0, scroll, [scroll, blindsTable, added] { tsui::revealRow(scroll, blindsTable, added); });
    });
    connect(addBreak, &QPushButton::clicked, this, [page, scroll, blindsTable, relabel, syncBlinds, persist] {
        tsui::commitPendingEdits(page);
        BlindLevel brk;
        brk.isBreak = true;
        brk.durationMinutes = 10;
        addBlindRow(blindsTable, brk);
        relabel();
        syncBlinds();
        persist();
        const int added = blindsTable->rowCount() - 1;
        blindsTable->setCurrentCell(added, tsui::kMinutesCol);
        blindsTable->setFocus();
        QTimer::singleShot(0, scroll, [scroll, blindsTable, added] { tsui::revealRow(scroll, blindsTable, added); });
    });
    connect(removeBlind, &QPushButton::clicked, this, [page, blindsTable, relabel, syncBlinds, persist] {
        tsui::commitPendingEdits(page);
        if (blindsTable->rowCount() <= 0) return;
        blindsTable->removeRow(blindsTable->rowCount() - 1);
        relabel();
        syncBlinds();
        persist();
    });
    connect(importBlinds, &QPushButton::clicked, this,
            [this, page, blindsTable, quiet, relabel, syncBlinds, persist] {
                tsui::commitPendingEdits(page);
                const QString path = QFileDialog::getOpenFileName(
                    this, QStringLiteral("Import blind structure"),
                    assetPath(QStringLiteral("BlindStructures")),
                    QStringLiteral("Blind structure JSON (*.json);;All files (*.*)"));
                if (path.isEmpty()) return;
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QMessageBox::critical(this, QStringLiteral("Import blinds"),
                                          QStringLiteral("Couldn't open:\n%1").arg(path));
                    return;
                }
                const QByteArray bytes = file.readAll();
                TournamentConfig probe;
                probe.blinds.clear();
                probe.setBlindsFromJson(toStd(QString::fromUtf8(bytes)));
                if (probe.blinds.empty()) {
                    QMessageBox::critical(
                        this, QStringLiteral("Import blinds"),
                        QStringLiteral("No rounds found. Expected a JSON file with a \"rounds\" "
                                       "array (or a bare array of levels)."));
                    return;
                }
                *quiet = true;
                blindsTable->setRowCount(0);
                for (const BlindLevel& level : probe.blinds) addBlindRow(blindsTable, level);
                *quiet = false;
                relabel();
                syncBlinds();
                persist();
            });
    connect(saveBlinds, &QPushButton::clicked, this, [this, page, blindsTable] {
        tsui::commitPendingEdits(page);
        std::vector<BlindLevel> blinds;
        QString problem;
        if (!readBlinds(blindsTable, blinds, problem)) {
            QMessageBox::warning(this, QStringLiteral("Save blinds"), problem);
            return;
        }
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save blind structure"),
            assetPath(QStringLiteral("BlindStructures")) + QStringLiteral("/blinds.json"),
            QStringLiteral("Blind structure JSON (*.json);;All files (*.*)"));
        if (path.isEmpty()) return;
        TournamentConfig exportConfig;
        exportConfig.blinds = std::move(blinds);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QMessageBox::critical(this, QStringLiteral("Save blinds"),
                                  QStringLiteral("Couldn't write:\n%1").arg(path));
            return;
        }
        const std::string json = exportConfig.blindsStructureJson();
        if (file.write(json.data(), static_cast<qint64>(json.size())) < 0) {
            QMessageBox::critical(this, QStringLiteral("Save blinds"),
                                  QStringLiteral("Couldn't write:\n%1").arg(path));
        }
    });

    // Focus rings for keyboard users only; clicking empty space takes focus (committing a cell edit).
    auto* focusVisible = new tsui::FocusVisibleFilter(page);
    for (QAbstractButton* b : page->findChildren<QAbstractButton*>()) b->installEventFilter(focusVisible);
    page->setFocusPolicy(Qt::ClickFocus);
}

} // namespace pokersoc::ui::qt
