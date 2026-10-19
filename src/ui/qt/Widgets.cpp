#include "ui/qt/Widgets.h"

#include <algorithm>

#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#include "application/AdminService.h"

namespace pokersoc::ui::qt {

// ---------------------------------------------------------------------------
// TileButton

TileButton::TileButton(const QString& icon, const QString& title, const QString& subtitle,
                       QWidget* parent)
    : QPushButton(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 20, 15, 20);
    layout->setSpacing(0);

    icon_ = new QLabel(icon);
    title_ = new QLabel(title);
    subtitle_ = new QLabel(subtitle);
    subtitle_->setObjectName("sub");
    for (QLabel* label : {icon_, title_, subtitle_}) {
        label->setAlignment(Qt::AlignCenter);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    setSizes(36, 18, 10);

    layout->addStretch(1);
    layout->addWidget(icon_);
    layout->addSpacing(8);
    layout->addWidget(title_);
    layout->addWidget(subtitle_);
    layout->addStretch(1);
    icon_->setVisible(!icon.isEmpty());
    subtitle_->setVisible(!subtitle.isEmpty());

    setColors(color::ButtonFace);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void TileButton::setIconText(const QString& icon) {
    icon_->setText(icon);
    icon_->setVisible(!icon.isEmpty());
}

void TileButton::setTitle(const QString& title) {
    title_->setText(title);
}

void TileButton::setSubtitle(const QString& subtitle) {
    subtitle_->setText(subtitle);
    subtitle_->setVisible(!subtitle.isEmpty());
}

void TileButton::setColors(const QString& background, const QString& border,
                           const QString& borderWidth, const QString& extra) {
    setStyleSheet(buttonStyle(background, border, borderWidth, extra));
}

void TileButton::setSizes(int iconPx, int titlePx, int subtitlePx) {
    icon_->setFont(uiFont(iconPx));
    title_->setFont(uiFont(titlePx, true));
    subtitle_->setFont(uiFont(subtitlePx));
}

void TileButton::setFaded(bool faded) {
    for (QLabel* label : {icon_, title_, subtitle_}) {
        if (faded) {
            auto* effect = new QGraphicsOpacityEffect(label);
            effect->setOpacity(0.3);
            label->setGraphicsEffect(effect);
        } else {
            label->setGraphicsEffect(nullptr);
        }
    }
}

QLabel* TileButton::addLine(const QString& text, int pixelSize, bool bold,
                            const QString& colorName, int topMargin) {
    QLabel* label = makeLabel(text, pixelSize, bold, colorName, Qt::AlignCenter);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setContentsMargins(0, topMargin, 0, 0);
    auto* box = static_cast<QVBoxLayout*>(layout());
    box->insertWidget(box->count() - 1, label);
    return label;
}

QSize TileButton::sizeHint() const {
    return layout()->sizeHint() + QSize(4, 4);
}

QSize TileButton::minimumSizeHint() const {
    return layout()->minimumSize() + QSize(4, 4);
}

// ---------------------------------------------------------------------------
// DenomButton

DenomButton::DenomButton(std::int64_t cents, Look look, const QString& imageFile,
                         const QString& background, bool badgeLeft, QWidget* parent)
    : QAbstractButton(parent),
      cents_(cents),
      look_(look),
      image_(imageFile.isEmpty() ? QPixmap() : QPixmap(assetPath(imageFile))),
      background_(background.isEmpty() ? QColor(color::ButtonFace) : QColor(background)),
      badgeLeft_(badgeLeft) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setText(denomLabel(cents));
}

void DenomButton::setCount(int count) {
    if (count_ != count) {
        count_ = count;
        update();
    }
}

void DenomButton::enterEvent(QEnterEvent* event) {
    update();
    QAbstractButton::enterEvent(event);
}

void DenomButton::leaveEvent(QEvent* event) {
    update();
    QAbstractButton::leaveEvent(event);
}

void DenomButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const bool hover = underMouse() && isEnabled();
    const bool down = isDown();
    const QColor border = down ? QColor("#2C628B") : hover ? QColor("#3C7FB1") : QColor(color::Gray);
    QColor face = background_;
    if (!isEnabled()) face = QColor("#F4F4F4");
    else if (down) face = QColor("#C4E5F6");
    else if (hover) face = QColor("#BEE6FD");

    p.fillRect(rect(), border);
    const QRectF inner = QRectF(rect()).adjusted(2, 2, -2, -2);
    p.fillRect(inner, face);

    // Badge geometry (shrunk if the button is small).
    const QString badgeText = QStringLiteral("×%1").arg(count_);
    const auto badgeSize = [&](int px) {
        const QFontMetrics fm(uiFont(px, true));
        const double pad = px / 32.0;
        return QSizeF(fm.horizontalAdvance(badgeText) + (24 + 6) * pad, fm.height() + (16 + 6) * pad);
    };
    int badgePx = 32;
    const bool noteLook = look_ == Look::Note && !image_.isNull();
    const double maxBadgeWidth = noteLook ? inner.width() - 8 : inner.width() * 0.55;
    QSizeF badge = badgeSize(badgePx);
    while (badgePx > 10 && (badge.height() > inner.height() - 8 || badge.width() > maxBadgeWidth)) {
        badge = badgeSize(--badgePx);
    }

    QRectF badgeRect;
    if (noteLook) {
        p.drawPixmap(inner, image_, QRectF(image_.rect()));
        p.fillRect(inner, QColor(0, 0, 0, 0x40));
        badgeRect = QRectF(inner.center().x() - badge.width() / 2,
                           inner.center().y() - badge.height() / 2, badge.width(), badge.height());
    } else {
        // Columns *|badge|*|image|* (or mirrored), like the WPF grid.
        double imageHeight = inner.height();
        double imageWidth = 0;
        if (!image_.isNull()) {
            const double aspect = static_cast<double>(image_.width()) / image_.height();
            imageWidth = imageHeight * aspect;
            const double available = inner.width() - badge.width() - 12;
            if (imageWidth > available) {
                imageWidth = std::max(0.0, available);
                imageHeight = imageWidth / aspect;
            }
        } else {
            imageWidth = QFontMetrics(uiFont(28, true)).horizontalAdvance(text()) + 8;
        }
        const double gap = std::max(0.0, (inner.width() - badge.width() - imageWidth) / 3);
        const double badgeX = badgeLeft_ ? inner.left() + gap : inner.left() + gap * 2 + imageWidth;
        const double imageX = badgeLeft_ ? inner.left() + gap * 2 + badge.width() : inner.left() + gap;
        const QRectF imageRect(imageX, inner.center().y() - imageHeight / 2, imageWidth, imageHeight);
        if (!image_.isNull()) {
            p.drawPixmap(imageRect, image_, QRectF(image_.rect()));
        } else {
            p.setPen(Qt::black);
            p.setFont(uiFont(28, true));
            p.drawText(imageRect, Qt::AlignCenter, text());
        }
        badgeRect = QRectF(badgeX, inner.center().y() - badge.height() / 2, badge.width(),
                           badge.height());
    }

    const double scale = badgePx / 32.0;
    p.setPen(QPen(QColor("#404040"), 3 * scale));
    p.setBrush(QColor(255, 255, 255, 0xE0));
    const double half = 1.5 * scale;
    p.drawRoundedRect(badgeRect.adjusted(half, half, -half, -half), 8 * scale, 8 * scale);
    p.setPen(QColor("#202020"));
    p.setFont(uiFont(badgePx, true));
    p.drawText(badgeRect, Qt::AlignCenter, badgeText);
}

// ---------------------------------------------------------------------------
// MultiplierBar

MultiplierBar::MultiplierBar(std::vector<Option> options, int fontPx, QWidget* parent)
    : QWidget(parent), options_(std::move(options)), value_(options_.front().value) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    for (const auto& option : options_) {
        auto* button = new QPushButton(option.label);
        button->setFont(uiFont(fontPx));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        const int value = option.value;
        connect(button, &QPushButton::clicked, this, [this, value] {
            setValue(value);
            emit changed(value);
        });
        layout->addWidget(button);
        buttons_.push_back(button);
    }
    refresh();
}

void MultiplierBar::setValue(int value) {
    value_ = value;
    refresh();
}

void MultiplierBar::refresh() {
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const bool selected = options_[i].value == value_;
        buttons_[i]->setStyleSheet(
            buttonStyle(selected ? color::LightGreen : color::LightBlue, color::Gray, "2px"));
    }
}

// ---------------------------------------------------------------------------
// CountTable

CountTable::CountTable(const QStringList& headers, QWidget* parent)
    : CountTable(headers, Look{}, parent) {}

CountTable::CountTable(const QStringList& headers, const Look& look, QWidget* parent)
    : QWidget(parent), look_(look), headers_(headers) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    header_ = new QFrame;
    header_->setObjectName("hdr");
    layout->addWidget(header_);

    body_ = new QWidget;
    body_->setObjectName("body");
    body_->setAttribute(Qt::WA_StyledBackground);
    body_->setStyleSheet("QWidget#body { background: white; }");
    bodyLayout_ = new QVBoxLayout(body_);
    bodyLayout_->setContentsMargins(0, 0, 0, 0);
    bodyLayout_->setSpacing(2);
    layout->addWidget(body_, 1);

    buildHeader(headers);
}

void CountTable::setLook(const Look& look) {
    look_ = look;
    buildHeader(headers_);
}

void CountTable::buildHeader(const QStringList& headers) {
    delete header_->layout();
    qDeleteAll(header_->findChildren<QLabel*>(Qt::FindDirectChildrenOnly));
    header_->setStyleSheet(
        QStringLiteral("QFrame#hdr { background:%1; border-style:solid; border-color:%2;"
                       " border-width:%3; }")
            .arg(look_.headerBg, look_.headerBorder, look_.headerBorderWidth));
    header_->setFixedHeight(look_.headerHeight);
    auto* row = new QHBoxLayout(header_);
    row->setContentsMargins(5, 0, 5, 0);
    for (const QString& text : headers) {
        row->addWidget(makeLabel(text, look_.headerPx, true, QString(), Qt::AlignCenter), 1);
    }
}

void CountTable::clearBody() {
    while (QLayoutItem* item = bodyLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void CountTable::setBody(QWidget* widget) {
    clearBody();
    bodyLayout_->addWidget(widget, 1);
}

void CountTable::setRows(const std::vector<Row>& rows, const QString& placeholder) {
    clearBody();

    if (rows.empty()) {
        auto* box = new QFrame;
        box->setObjectName("ph");
        box->setStyleSheet(
            QStringLiteral("QFrame#ph { background:%1; border-style:solid; border-color:%2;"
                           " border-width:%3; }")
                .arg(look_.placeholderBg, look_.placeholderBorder, look_.placeholderBorderWidth));
        auto* inner = new QVBoxLayout(box);
        QLabel* text = makeLabel(placeholder, look_.placeholderPx, false, color::Gray,
                                 Qt::AlignCenter);
        QFont italic = text->font();
        italic.setItalic(true);
        text->setFont(italic);
        text->setWordWrap(true);
        inner->addWidget(text);
        bodyLayout_->addWidget(box, 1);
        return;
    }

    const int n = static_cast<int>(rows.size());
    int textPx = 12;
    int countPx = 14;
    if (look_.compact) {
        if (n <= 3) { textPx = 14; countPx = 16; }
        else if (n <= 5) { textPx = 12; countPx = 14; }
        else { textPx = 10; countPx = 12; }
    } else {
        if (n <= 4) { textPx = 22; countPx = 28; }
        else if (n <= 6) { textPx = 18; countPx = 24; }
        else if (n <= 8) { textPx = 14; countPx = 18; }
    }

    for (const Row& data : rows) {
        auto* row = new QFrame;
        row->setObjectName("row");
        row->setStyleSheet(QStringLiteral("QFrame#row { background:%1; border-style:solid;"
                                          " border-color:%2; border-width:%3; border-radius:%4px; }")
                               .arg(look_.rowBg, look_.rowBorder, look_.rowBorderWidth)
                               .arg(look_.rowRadius));
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(4, 0, 4, 0);
        QFont semi = uiFont(textPx);
        semi.setWeight(QFont::DemiBold);
        QLabel* label = makeLabel(data.label, textPx, false, QString(), Qt::AlignCenter);
        label->setFont(semi);
        QLabel* count = makeLabel(data.count, countPx, true, look_.countColor, Qt::AlignCenter);
        QLabel* value = makeLabel(data.value, textPx, false, QString(), Qt::AlignCenter);
        value->setFont(semi);
        layout->addWidget(label, 1);
        layout->addWidget(count, 1);
        layout->addWidget(value, 1);
        bodyLayout_->addWidget(row, 1);
    }
    for (int i = n; i < look_.minRows; ++i) {
        bodyLayout_->addWidget(new QWidget, 1);
    }
}

void CountTable::setCounts(const Counts& counts, const QString& placeholder) {
    std::vector<Row> rows;
    for (auto it = counts.rbegin(); it != counts.rend(); ++it) {
        if (it->second <= 0) continue;
        rows.push_back(Row{denomLabel(it->first), QStringLiteral("×%1").arg(it->second),
                           money(it->first * it->second)});
    }
    setRows(rows, placeholder);
}

// ---------------------------------------------------------------------------
// CurrencyPad / ChipPad

namespace {

QPushButton* makeSpacerButton() {
    auto* spacer = new QPushButton;
    spacer->setEnabled(false);
    spacer->setStyleSheet(buttonStyle(color::ButtonFace, color::Gray, "2px"));
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return spacer;
}

} // namespace

CurrencyPad::CurrencyPad(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    const std::int64_t coins[] = {5, 10, 20, 50, 100, 200};
    for (int i = 0; i < 6; ++i) {
        const std::int64_t cents = coins[i];
        auto* button = new DenomButton(cents, DenomButton::Look::Coin, currencyImage(cents),
                                       cents >= 100 ? color::GoldCoin : color::SilverCoin,
                                       i % 2 == 0);
        grid->addWidget(button, i / 2, i % 2);
        buttons_.push_back(button);
    }
    const std::int64_t notes[] = {500, 1000, 2000, 5000, 10000};
    for (int i = 0; i < 5; ++i) {
        const std::int64_t cents = notes[i];
        auto* button = new DenomButton(cents, DenomButton::Look::Note, currencyImage(cents),
                                       QString(), true);
        grid->addWidget(button, 3 + i / 2, i % 2);
        buttons_.push_back(button);
    }
    for (DenomButton* button : buttons_) {
        const std::int64_t cents = button->cents();
        connect(button, &QAbstractButton::clicked, this, [this, cents] { emit denomClicked(cents); });
    }

    multipliers_ = new MultiplierBar({{QStringLiteral("×1"), 1}, {QStringLiteral("×5"), 5},
                                      {QStringLiteral("×20"), 20}});
    grid->addWidget(multipliers_, 5, 1);
    makeUniform(grid, 6, 2);
}

void CurrencyPad::setCounts(const Counts& counts) {
    for (DenomButton* button : buttons_) {
        const auto it = counts.find(button->cents());
        button->setCount(it == counts.end() ? 0 : it->second);
    }
}

void CurrencyPad::setInteractive(bool interactive) {
    for (DenomButton* button : buttons_) {
        button->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
    }
    multipliers_->setAttribute(Qt::WA_TransparentForMouseEvents, !interactive);
}

ChipPad::ChipPad(QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    const std::int64_t chips[] = {5, 25, 100, 500, 2500};
    for (int i = 0; i < 5; ++i) {
        const std::int64_t cents = chips[i];
        auto* button = new DenomButton(cents, DenomButton::Look::Coin, chipImage(cents), QString(),
                                       i % 2 == 0);
        grid->addWidget(button, i / 2, i % 2);
        connect(button, &QAbstractButton::clicked, this, [this, cents] { emit denomClicked(cents); });
        buttons_.push_back(button);
    }
    grid->addWidget(makeSpacerButton(), 2, 1);
    grid->addWidget(makeSpacerButton(), 3, 0);
    grid->addWidget(makeSpacerButton(), 3, 1);
    grid->addWidget(makeSpacerButton(), 4, 0);
    grid->addWidget(makeSpacerButton(), 4, 1);
    grid->addWidget(makeSpacerButton(), 5, 0);

    multipliers_ = new MultiplierBar({{QStringLiteral("×1"), 1}, {QStringLiteral("×4/×5"), 5},
                                      {QStringLiteral("×20"), 20}});
    grid->addWidget(multipliers_, 5, 1);
    makeUniform(grid, 6, 2);
}

void ChipPad::setCounts(const Counts& counts) {
    for (DenomButton* button : buttons_) {
        const auto it = counts.find(button->cents());
        button->setCount(it == counts.end() ? 0 : it->second);
    }
}

int ChipPad::addAmount(std::int64_t cents) const {
    const int multiplier = multipliers_->value();
    if (multiplier == 5) {
        return cents == 25 || cents == 2500 ? 4 : 5;
    }
    return multiplier;
}

// ---------------------------------------------------------------------------
// Numpad

Numpad::Numpad(const QStringList& keys, int fontPx, QWidget* parent) : QWidget(parent) {
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    for (int i = 0; i < keys.size(); ++i) {
        const QString key = keys[i];
        auto* button = new QPushButton(key);
        button->setFont(uiFont(fontPx, true));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        connect(button, &QPushButton::clicked, this, [this, key] { emit pressed(key); });
        grid->addWidget(button, i / 3, i % 3);
        buttons_.push_back(button);
    }
    makeUniform(grid, (static_cast<int>(keys.size()) + 2) / 3, 3);
}

QPushButton* Numpad::button(const QString& key) const {
    for (QPushButton* button : buttons_) {
        if (button->text() == key) return button;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Overlay

Overlay::Overlay(QWidget* host, const QColor& dim) : QWidget(host), host_(host), dim_(dim) {
    card_ = new QFrame;
    card_->setObjectName("card");
    card_->setStyleSheet("QFrame#card { background: white; border-radius: 10px; }");
    cardLayout_ = new QVBoxLayout(card_);
    cardLayout_->setContentsMargins(30, 30, 30, 30);
    cardLayout_->setSpacing(0);

    auto* outer = new QGridLayout(this);
    outer->addWidget(card_, 0, 0, Qt::AlignCenter);

    host_->installEventFilter(this);
    hide();
}

void Overlay::open() {
    setGeometry(host_->rect());
    raise();
    show();
}

bool Overlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host_ && event->type() == QEvent::Resize) {
        setGeometry(host_->rect());
    }
    return QWidget::eventFilter(watched, event);
}

void Overlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), dim_);
}

// ---------------------------------------------------------------------------
// PasswordOverlay

PasswordOverlay::PasswordOverlay(QWidget* host, application::AdminService& admin,
                                 const QString& confirmBackground, bool confirmWhiteText)
    : Overlay(host), admin_(admin) {
    card()->setMinimumWidth(320);
    QVBoxLayout* layout = cardLayout();

    layout->addWidget(makeLabel(QStringLiteral("🔐"), 48, false, QString(), Qt::AlignCenter));
    layout->addSpacing(10);
    layout->addWidget(makeLabel("Admin Password Required", 20, true, QString(), Qt::AlignCenter));
    layout->addSpacing(10);
    action_ = makeLabel(QString(), 13, false, QString(), Qt::AlignCenter);
    action_->setWordWrap(true);
    layout->addWidget(action_);

    QFrame* box = makePanel("#F0F0F0", color::Gray, 1, 5, 0);
    box->layout()->setContentsMargins(15, 10, 15, 10);
    box->setMinimumWidth(200);
    display_ = new QLabel;
    QFont mono(QStringLiteral("Consolas"));
    mono.setPixelSize(28);
    mono.setBold(true);
    display_->setFont(mono);
    display_->setAlignment(Qt::AlignCenter);
    display_->setMinimumHeight(34);
    box->layout()->addWidget(display_);
    layout->addWidget(box);
    layout->addSpacing(10);

    error_ = makeLabel("Incorrect password", 12, false, color::Red, Qt::AlignCenter);
    error_->hide();
    layout->addWidget(error_);
    layout->addSpacing(4);

    auto* pad = new Numpad({"1", "2", "3", "4", "5", "6", "7", "8", "9", QStringLiteral("⌫"), "0",
                            "C"},
                           24);
    pad->setFixedSize(240, 4 * 60);
    pad->button(QStringLiteral("⌫"))->setStyleSheet(buttonStyle(color::LightCoral, "#707070", "1px"));
    pad->button("C")->setStyleSheet(buttonStyle(color::Orange, "#707070", "1px"));
    connect(pad, &Numpad::pressed, this, [this](const QString& k) { key(k); });
    layout->addWidget(pad, 0, Qt::AlignHCenter);
    layout->addSpacing(14);

    auto* buttons = new QHBoxLayout;
    QPushButton* cancel = makeButton("Cancel", color::LightGray, 14);
    QPushButton* ok = makeButton("Confirm", confirmBackground, 14, true);
    if (confirmWhiteText) {
        ok->setStyleSheet(buttonStyle(confirmBackground, "#707070", "1px",
                                      "padding:12px 15px; color:white;"));
    }
    buttons->addWidget(cancel);
    buttons->addSpacing(5);
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, this, [this] {
        entered_.clear();
        hide();
    });
    connect(ok, &QPushButton::clicked, this, [this] { confirm(); });
}

void PasswordOverlay::ask(const QString& action, std::function<void()> onSuccess) {
    onSuccess_ = std::move(onSuccess);
    entered_.clear();
    action_->setText(action);
    action_->setVisible(!action.isEmpty());
    error_->hide();
    refreshDisplay();
    open();
}

void PasswordOverlay::key(const QString& key) {
    if (key == QStringLiteral("⌫")) {
        entered_.chop(1);
    } else if (key == "C") {
        entered_.clear();
    } else {
        entered_ += key;
    }
    error_->hide();
    refreshDisplay();
}

void PasswordOverlay::confirm() {
    if (admin_.checkPassword(toStd(entered_))) {
        entered_.clear();
        hide();
        auto callback = std::move(onSuccess_);
        onSuccess_ = nullptr;
        if (callback) callback();
    } else {
        entered_.clear();
        error_->show();
        refreshDisplay();
    }
}

void PasswordOverlay::refreshDisplay() {
    display_->setText(QString(entered_.size(), QChar(0x25CF)));
}

// ---------------------------------------------------------------------------
// Builders

QFrame* makePanel(const QString& background, const QString& border, int borderPx, int radius,
                  int padding) {
    auto* panel = new QFrame;
    panel->setObjectName("panel");
    panel->setStyleSheet(panelStyle("panel", background, border, borderPx, radius));
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(padding, padding, padding, padding);
    layout->setSpacing(0);
    return panel;
}

QVBoxLayout* panelLayout(QFrame* panel) {
    return static_cast<QVBoxLayout*>(panel->layout());
}

QFrame* makePlayerBox(const QString& id, const QString& name) {
    QFrame* box = makePanel(color::LightGray, color::DarkGray, 2, 0, 10);
    QVBoxLayout* layout = panelLayout(box);
    layout->addWidget(makeLabel("Current Player", 12, true));
    layout->addSpacing(5);
    const auto addLine = [&](const QString& caption, const QString& value) {
        auto* row = new QHBoxLayout;
        QLabel* label = makeLabel(caption, 12, true);
        label->setFixedWidth(80);
        row->addWidget(label);
        row->addWidget(makeLabel(value, 14), 1);
        layout->addLayout(row);
    };
    addLine("Player ID: ", id);
    addLine("Name: ", name);
    return box;
}

QFrame* makeInfoTile(const QString& icon, const QString& title, QLabel** valueLabel,
                     const QString& background, const QString& border,
                     const QString& borderWidth, int iconPx, int titlePx, int valuePx) {
    auto* tile = new QFrame;
    tile->setObjectName("tile");
    tile->setStyleSheet(QStringLiteral("QFrame#tile { background:%1; border-style:solid;"
                                       " border-color:%2; border-width:%3; }")
                            .arg(background, border, borderWidth));
    auto* layout = new QVBoxLayout(tile);
    layout->setContentsMargins(15, 16, 15, 16);
    layout->setSpacing(0);
    layout->addStretch(1);
    if (!icon.isEmpty()) {
        layout->addWidget(makeLabel(icon, iconPx, false, QString(), Qt::AlignCenter));
        layout->addSpacing(8);
    }
    layout->addWidget(makeLabel(title, titlePx, true, QString(), Qt::AlignCenter));
    QLabel* value = makeLabel(QStringLiteral("$0.00"), valuePx, true, QString(), Qt::AlignCenter);
    layout->addSpacing(5);
    layout->addWidget(value);
    layout->addStretch(1);
    if (valueLabel) *valueLabel = value;
    tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return tile;
}

QFrame* makeChangeErrorPanel() {
    QFrame* panel = makePanel("#FFE6E6", color::Red, 2, 5, 10);
    QVBoxLayout* layout = panelLayout(panel);
    layout->addStretch(1);
    layout->addWidget(makeLabel(QStringLiteral("⚠️"), 36, false, QString(), Qt::AlignCenter));
    layout->addWidget(makeLabel("Cannot give this change", 16, true, color::Red, Qt::AlignCenter));
    QLabel* detail = makeLabel("Not enough cash in cashbox.\nAdjust denominations or add more float.",
                               12, false, color::DarkRed, Qt::AlignCenter);
    detail->setWordWrap(true);
    layout->addWidget(detail);
    layout->addStretch(1);
    return panel;
}

QPushButton* makeButton(const QString& text, const QString& background, int fontPx, bool bold) {
    auto* button = new QPushButton(text);
    button->setFont(uiFont(fontPx, bold));
    if (!background.isEmpty()) {
        button->setStyleSheet(buttonStyle(background, "#707070", "1px", "padding:10px 14px;"));
    }
    return button;
}

void clearLayout(QLayout* layout) {
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        if (QLayout* child = item->layout()) clearLayout(child);
        delete item;
    }
}

void makeUniform(QGridLayout* grid, int rows, int columns) {
    for (int r = 0; r < rows; ++r) grid->setRowStretch(r, 1);
    for (int c = 0; c < columns; ++c) grid->setColumnStretch(c, 1);
}

Columns makeColumns(QWidget* page) {
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(12, 12, 12, 12);
    row->setSpacing(12);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    left->setSpacing(10);
    right->setSpacing(10);
    row->addLayout(left, 2);
    row->addLayout(right, 3);
    return {left, right};
}

QHBoxLayout* makePair(QWidget* first, QWidget* second) {
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(first, 1);
    row->addWidget(second, 1);
    return row;
}

QLabel* makeHeading(const QString& text, int pixelSize) {
    return makeLabel(text, pixelSize, true);
}

QString currencyImage(std::int64_t cents) {
    switch (cents) {
        case 5: return "Currency/coin_5c.png";
        case 10: return "Currency/coin_10c.png";
        case 20: return "Currency/coin_20c.png";
        case 50: return "Currency/coin_50c.png";
        case 100: return "Currency/coin_1.png";
        case 200: return "Currency/coin_2.png";
        default: return QStringLiteral("Currency/note_%1.png").arg(cents / 100);
    }
}

QString chipImage(std::int64_t cents) {
    switch (cents) {
        case 5: return "Chips/chip_5c.png";
        case 25: return "Chips/chip_25c.png";
        case 100: return "Chips/chip_1.png";
        case 500: return "Chips/chip_5.png";
        case 2500: return "Chips/plaque_25.png";
        case 10000: return "Chips/plaque_100.png";
        default: return QString();
    }
}

} // namespace pokersoc::ui::qt
