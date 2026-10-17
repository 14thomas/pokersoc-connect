#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <QAbstractButton>
#include <QFrame>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

#include "ui/qt/Style.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLayout;
class QVBoxLayout;

namespace pokersoc::application {
class AdminService;
}

namespace pokersoc::ui::qt {

/// The app's signature button: a big emoji, a bold title and a faded subtitle,
/// stacked and centred (the WPF "icon / title / subtitle" StackPanel).
class TileButton : public QPushButton {
    Q_OBJECT
public:
    TileButton(const QString& icon, const QString& title, const QString& subtitle = QString(),
               QWidget* parent = nullptr);

    void setIconText(const QString& icon);
    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);
    void setColors(const QString& background, const QString& border = color::Gray,
                   const QString& borderWidth = QStringLiteral("2px"),
                   const QString& extra = QString());
    void setSizes(int iconPx, int titlePx, int subtitlePx);
    /// The "coming soon" look: everything at 30% opacity.
    void setFaded(bool faded);
    /// Append another centred line of text below the subtitle.
    QLabel* addLine(const QString& text, int pixelSize, bool bold = false,
                    const QString& colorName = QString(), int topMargin = 0);

    QLabel* iconLabel() const { return icon_; }
    QLabel* titleLabel() const { return title_; }
    QLabel* subtitleLabel() const { return subtitle_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    QLabel* icon_;
    QLabel* title_;
    QLabel* subtitle_;
};

/// A note/coin/chip button: the denomination's picture plus a "×N" count badge.
/// Coins sit on a coloured face with the badge beside the image; notes fill the
/// button with the (darkened) note picture and centre the badge on it.
class DenomButton : public QAbstractButton {
    Q_OBJECT
public:
    enum class Look { Coin, Note };

    DenomButton(std::int64_t cents, Look look, const QString& imageFile,
                const QString& background, bool badgeLeft, QWidget* parent = nullptr);

    std::int64_t cents() const noexcept { return cents_; }
    void setCount(int count);

    QSize sizeHint() const override { return {120, 80}; }
    QSize minimumSizeHint() const override { return {60, 40}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    std::int64_t cents_;
    Look look_;
    QPixmap image_;
    QColor background_;
    bool badgeLeft_;
    int count_ = 0;
};

/// "×1 / ×5 / ×20" style multiplier selector. The selected button is light
/// green, the others light blue.
class MultiplierBar : public QWidget {
    Q_OBJECT
public:
    struct Option {
        QString label;
        int value;
    };
    MultiplierBar(std::vector<Option> options, int fontPx = 24, QWidget* parent = nullptr);

    int value() const noexcept { return value_; }
    void setValue(int value);

signals:
    void changed(int value);

private:
    void refresh();

    std::vector<Option> options_;
    std::vector<QPushButton*> buttons_;
    int value_;
};

/// The three-column tally used all over the app (Denomination / Count / Value).
/// Rows share the height equally (a WPF UniformGrid), fonts shrink as rows are
/// added, and short lists are padded so 1-3 rows look the same size as 4.
class CountTable : public QWidget {
    Q_OBJECT
public:
    struct Row {
        QString label;
        QString count;
        QString value;
    };
    struct Look {
        QString headerBg = QStringLiteral("#E0E0E0");
        QString headerBorder = color::Gray;
        QString headerBorderWidth = QStringLiteral("1px");
        int headerPx = 14;
        int headerHeight = 40;
        QString rowBg = QStringLiteral("#F5FFF5");
        QString rowBorder = QStringLiteral("#646464");
        QString rowBorderWidth = QStringLiteral("2px");
        int rowRadius = 5;
        QString countColor = color::DarkGreen;
        QString placeholderBg = QStringLiteral("#FAFAFA");
        QString placeholderBorder = color::Gray;
        QString placeholderBorderWidth = QStringLiteral("0px 1px 1px 1px");
        int placeholderPx = 16;
        int minRows = 4;
        bool compact = false; // smaller fonts (exchange tables)
    };

    explicit CountTable(const QStringList& headers, QWidget* parent = nullptr);
    CountTable(const QStringList& headers, const Look& look, QWidget* parent = nullptr);

    void setLook(const Look& look);
    Look& look() noexcept { return look_; }
    void setRows(const std::vector<Row>& rows, const QString& placeholder);
    /// Rows for a tally, high face value first; "×N" counts.
    void setCounts(const Counts& counts, const QString& placeholder);
    /// Replace the body with an arbitrary widget (e.g. an error panel).
    void setBody(QWidget* widget);

private:
    void clearBody();
    void buildHeader(const QStringList& headers);

    Look look_;
    QStringList headers_;
    QFrame* header_ = nullptr;
    QWidget* body_ = nullptr;
    QVBoxLayout* bodyLayout_ = nullptr;
};

/// The 6×2 note/coin grid (5c..$100 with pictures) plus the ×1/×5/×20 bar.
class CurrencyPad : public QWidget {
    Q_OBJECT
public:
    explicit CurrencyPad(QWidget* parent = nullptr);
    void setCounts(const Counts& counts);
    int multiplier() const { return multipliers_->value(); }
    void resetMultiplier() { multipliers_->setValue(1); }
    /// Ignore clicks without greying anything out (free tournament buy-ins).
    void setInteractive(bool interactive);

signals:
    void denomClicked(std::int64_t cents);

private:
    std::vector<DenomButton*> buttons_;
    MultiplierBar* multipliers_;
};

/// The chip grid (5c..$25 chip pictures) plus ×1 / ×4/×5 / ×20 stack counting.
class ChipPad : public QWidget {
    Q_OBJECT
public:
    explicit ChipPad(QWidget* parent = nullptr);
    void setCounts(const Counts& counts);
    /// How many chips one click adds: 25c and $25 chips stack in 4s, others in 5s.
    int addAmount(std::int64_t cents) const;

signals:
    void denomClicked(std::int64_t cents);

private:
    std::vector<DenomButton*> buttons_;
    MultiplierBar* multipliers_;
};

/// A 3×4 keypad. Emits the key text ("0"-"9", ".", "⌫", ...).
class Numpad : public QWidget {
    Q_OBJECT
public:
    explicit Numpad(const QStringList& keys = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".",
                                               "0", QStringLiteral("⌫")},
                    int fontPx = 24, QWidget* parent = nullptr);
    QPushButton* button(const QString& key) const;

signals:
    void pressed(const QString& key);

private:
    std::vector<QPushButton*> buttons_;
};

/// A dimmed full-window layer with a white rounded card in the middle — the
/// WPF "overlay panel" used for confirmations and pop-up inputs.
class Overlay : public QWidget {
    Q_OBJECT
public:
    explicit Overlay(QWidget* host, const QColor& dim = QColor(0, 0, 0, 0xCC));

    QFrame* card() const { return card_; }
    QVBoxLayout* cardLayout() const { return cardLayout_; }
    void open();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QWidget* host_;
    QColor dim_;
    QFrame* card_;
    QVBoxLayout* cardLayout_;
};

/// "🔐 Admin Password Required" keypad overlay guarding destructive actions.
class PasswordOverlay : public Overlay {
    Q_OBJECT
public:
    PasswordOverlay(QWidget* host, application::AdminService& admin,
                    const QString& confirmBackground = color::OrangeRed,
                    bool confirmWhiteText = true);

    /// Show the overlay; `onSuccess` runs once the right password is entered.
    void ask(const QString& action, std::function<void()> onSuccess);

private:
    void key(const QString& key);
    void confirm();
    void refreshDisplay();

    application::AdminService& admin_;
    QString entered_;
    QLabel* action_;
    QLabel* display_;
    QLabel* error_;
    std::function<void()> onSuccess_;
};

// ---- small builders ----

/// A QFrame with a WPF Border look and a vertical layout padded by `padding`.
QFrame* makePanel(const QString& background, const QString& border, int borderPx = 2,
                  int radius = 0, int padding = 10);
QVBoxLayout* panelLayout(QFrame* panel);

/// The "Current Player" box shown on the buy-in / cash-out screens.
QFrame* makePlayerBox(const QString& id, const QString& name);

/// A non-clickable tile (icon / title / value), e.g. "💰 Total Cash $0.00".
QFrame* makeInfoTile(const QString& icon, const QString& title, QLabel** valueLabel,
                     const QString& background, const QString& border = color::Gray,
                     const QString& borderWidth = QStringLiteral("0px 1px 1px 1px"),
                     int iconPx = 36, int titlePx = 18, int valuePx = 14);

/// Red "⚠️ Cannot give this change" box shown when the cashbox is short.
QFrame* makeChangeErrorPanel();

/// A plain button with WPF styling and its own background.
QPushButton* makeButton(const QString& text, const QString& background = QString(),
                        int fontPx = 18, bool bold = false);

/// Remove (and schedule deletion of) everything in a layout. Safe to call from
/// a click handler of one of the widgets being removed.
void clearLayout(QLayout* layout);

/// Rows for a uniform grid: every row/column stretches equally.
void makeUniform(QGridLayout* grid, int rows, int columns);

/// The standard page split: a 2* left column and a 3* right column, 12px margins.
struct Columns {
    QVBoxLayout* left;
    QVBoxLayout* right;
};
Columns makeColumns(QWidget* page);

/// Two widgets side by side with a 10px gap (Clear | Undo, Back | Next, ...).
QHBoxLayout* makePair(QWidget* first, QWidget* second);

/// A section heading ("Cash Received", "Activity Log", ...).
QLabel* makeHeading(const QString& text, int pixelSize = 18);

/// The images on the cash input pads.
QString currencyImage(std::int64_t cents);
QString chipImage(std::int64_t cents);

} // namespace pokersoc::ui::qt
