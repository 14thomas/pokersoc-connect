#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMargins>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class QScrollBar;
class QVariantAnimation;

namespace pokersoc::ui::qt::settings {

constexpr int kMaxContentWidth = 800;

QColor token(const char* name);
QString themed(const QString& qss);
QString pageStyleSheet();

enum class Glyph { Close, Plus, Minus, Chevron };
QIcon glyphIcon(Glyph glyph, const QColor& color);

QLabel* text(const QString& content, const char* role, bool wrap = false);
QPushButton* button(const QString& label, const char* variant);
QToolButton* iconButton(Glyph glyph, const QString& tooltip);
QFrame* divider();

struct Card {
    QFrame* frame = nullptr;
    QVBoxLayout* body = nullptr;
};
Card card(const QString& title, const QString& description, QWidget* accessory = nullptr);
QWidget* settingRow(const QString& title, const QString& description, QWidget* control);
QWidget* constrain(QWidget* inner, const QMargins& margins, QWidget* strip = nullptr);

/// Animated on/off switch (reuses QAbstractButton::toggled).
class Switch final : public QAbstractButton {
public:
    explicit Switch(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static QColor mix(const QColor& a, const QColor& b, qreal t);
    QVariantAnimation* anim_ = nullptr;
    qreal pos_ = 0.0;
};

class FocusVisibleFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

class OverlayScrollArea final : public QScrollArea {
public:
    explicit OverlayScrollArea(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QScrollBar* overlay_ = nullptr;
};

void installFocusVisible(QWidget* page);

} // namespace pokersoc::ui::qt::settings
