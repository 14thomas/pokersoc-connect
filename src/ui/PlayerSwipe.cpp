#include "ui/PlayerSwipe.h"

#include <QAbstractButton>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QWidget>

namespace pokersoc::ui {

PlayerSwipe::PlayerSwipe(QWidget* card, std::function<bool()> occupied, std::function<void()> clear,
                         QObject* parent)
    : QObject(parent), card_(card), occupied_(std::move(occupied)), clear_(std::move(clear)) {}

void PlayerSwipe::watch(QWidget* root) {
    if (!root) return;
    const auto arm = [this](QWidget* widget) {
        if (!widget || qobject_cast<QAbstractButton*>(widget)) return;
        // Composite buttons own their clicks. A drag that starts on one still
        // doesn't begin here; the card surface around them does.
        if (widget->inherits("pokersoc::ui::TapFrame")) return;
        widget->removeEventFilter(this);
        widget->installEventFilter(this);
    };
    arm(root);
    const auto children = root->findChildren<QWidget*>();
    for (QWidget* child : children) arm(child);
}

void PlayerSwipe::begin() {
    if (dragging_ || !card_) return;
    dragging_ = true;
    QWidget* host = card_->parentWidget();
    if (!host) return;
    ghost_ = new QLabel(host);
    ghost_->setPixmap(card_->grab());
    ghost_->setAttribute(Qt::WA_TransparentForMouseEvents);
    ghost_->setFixedSize(card_->size());
    origin_ = card_->pos();
    ghost_->move(origin_);
    ghost_->show();
    ghost_->raise();
    auto* fade = new QGraphicsOpacityEffect(card_);
    fade->setOpacity(0);
    card_->setGraphicsEffect(fade);
}

void PlayerSwipe::finish(bool dismiss) {
    tracking_ = false;
    dragging_ = false;
    // Clear while the real card is still hidden, so it never snaps back into
    // place and then waits to disappear.
    if (dismiss && clear_) clear_();
    if (ghost_) {
        ghost_->hide();
        ghost_->deleteLater();
        ghost_ = nullptr;
    }
    if (card_) card_->setGraphicsEffect(nullptr);
}

bool PlayerSwipe::eventFilter(QObject* watched, QEvent* event) {
    if (!card_ || qobject_cast<QAbstractButton*>(watched)) {
        return QObject::eventFilter(watched, event);
    }
    const bool press = event->type() == QEvent::MouseButtonPress;
    const bool move = event->type() == QEvent::MouseMove;
    const bool release = event->type() == QEvent::MouseButtonRelease;
    if (!press && !move && !release) return QObject::eventFilter(watched, event);

    auto* mouse = static_cast<QMouseEvent*>(event);
    if (press) {
        if (mouse->button() != Qt::LeftButton) return QObject::eventFilter(watched, event);
        // Moving the card used to retarget the finger and start a new drag, which is why it froze.
        if (tracking_) return true;
        tracking_ = occupied_ && occupied_();
        dragging_ = false;
        start_ = mouse->globalPosition().toPoint();
        return QObject::eventFilter(watched, event);
    }
    if (!tracking_) return QObject::eventFilter(watched, event);

    const QPoint delta = mouse->globalPosition().toPoint() - start_;
    if (move) {
        if (!dragging_) {
            if (delta.y() > -8 || qAbs(delta.x()) > qAbs(delta.y())) {
                return QObject::eventFilter(watched, event);
            }
            begin();
        }
        if (dragging_ && -delta.y() >= 90) {
            finish(true);
            return true;
        }
        if (ghost_) {
            QWidget* window = card_->window();
            const int limit = window ? qMax(card_->height(), window->height()) : card_->height();
            const int lift = qBound(0, -delta.y(), limit);
            ghost_->move(origin_.x(), origin_.y() - lift);
            ghost_->raise();
        }
        return true;
    }

    const bool dragged = dragging_;
    const bool dismiss = dragged && -delta.y() >= 90;
    finish(dismiss);
    return dragged;
}

} // namespace pokersoc::ui
