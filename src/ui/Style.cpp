#include "ui/Style.h"

#include <QApplication>
#include <QLabel>

#include "domain/money/DenominationCount.h"
#include "domain/money/DenominationTable.h"

namespace pokersoc::ui {

QString toQ(const std::string& text) {
    return QString::fromStdString(text);
}

std::string toStd(const QString& text) {
    return text.toStdString();
}

QString money(domain::Money amount) {
    return money(amount.cents());
}

QString money(std::int64_t cents) {
    const bool negative = cents < 0;
    const std::int64_t magnitude = negative ? -cents : cents;
    QString dollars = QString::number(magnitude / 100);
    for (int i = dollars.size() - 3; i > 0; i -= 3) {
        dollars.insert(i, ',');
    }
    return QStringLiteral("%1$%2.%3")
        .arg(negative ? "-" : "")
        .arg(dollars)
        .arg(static_cast<int>(magnitude % 100), 2, 10, QChar('0'));
}

QString denomLabel(std::int64_t cents) {
    if (cents < 100) {
        return QString::number(cents) + "c";
    }
    if (cents % 100 == 0) {
        return "$" + QString::number(cents / 100);
    }
    return money(cents);
}

QString assetPath(const QString& relative) {
    QString path = relative;
    while (path.startsWith(QLatin1Char('/')) || path.startsWith(QLatin1Char('\\'))) path.remove(0, 1);
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QStringLiteral(":/Assets/") + path;
}

QFont uiFont(int pixelSize, bool bold) {
    QFont f(QStringLiteral("Segoe UI"));
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    return f;
}

QLabel* makeLabel(const QString& text, int pixelSize, bool bold, const QString& colorName,
                  Qt::Alignment align) {
    auto* label = new QLabel(text);
    label->setFont(uiFont(pixelSize, bold));
    label->setAlignment(align | Qt::AlignVCenter);
    if (!colorName.isEmpty()) {
        label->setStyleSheet(QStringLiteral("color:%1;").arg(colorName));
    }
    return label;
}

QString buttonStyle(const QString& background, const QString& border, const QString& borderWidth,
                    const QString& extra) {
    return QStringLiteral(
               "QPushButton { background:%1; border-style:solid; border-color:%2; border-width:%3;"
               " color:black; padding:0px; min-height:0px; %4 }"
               "QPushButton:hover { background:#BEE6FD; border-color:#3C7FB1; }"
               "QPushButton:pressed { background:#C4E5F6; border-color:#2C628B; }"
               "QPushButton:disabled { background:#F4F4F4; border-color:#ADB2B5; color:#838383; }"
               "QLabel { color:black; }"
               "QLabel#sub { color:rgba(0,0,0,178); }"
               "QLabel:disabled { color:#838383; }"
               "QLabel#sub:disabled { color:rgba(131,131,131,178); }")
        .arg(background, border, borderWidth, extra);
}

QString panelStyle(const QString& objectName, const QString& background, const QString& border,
                   int borderPx, int radius) {
    return QStringLiteral("QFrame#%1 { background:%2; border:%4px solid %3; border-radius:%5px; }")
        .arg(objectName, background, border)
        .arg(borderPx)
        .arg(radius);
}

void applyApplicationStyle(QApplication& app) {
    app.setFont(uiFont(12));
    app.setFont(uiFont(18), "QPushButton");
    app.setFont(uiFont(18), "QLineEdit");
    app.setFont(uiFont(16), "QTableWidget");
    app.setFont(uiFont(16), "QHeaderView");
    app.setFont(uiFont(14), "QCheckBox");
    app.setFont(uiFont(14), "QRadioButton");

    // WPF "Aero2" default control chrome.
    app.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: white; }
        QPushButton {
            background: #DDDDDD; border: 1px solid #707070; color: black;
            padding: 10px 14px; min-height: 22px; min-width: 16px;
        }
        QPushButton:hover { background: #BEE6FD; border-color: #3C7FB1; }
        QPushButton:pressed { background: #C4E5F6; border-color: #2C628B; }
        QPushButton:disabled { background: #F4F4F4; border-color: #ADB2B5; color: #838383; }
        QLineEdit {
            background: white; border: 1px solid #ABADB3; padding: 6px 8px; min-height: 24px;
            selection-background-color: #0078D7;
        }
        QLineEdit:hover { border-color: #7EB4EA; }
        QLineEdit:focus { border-color: #569DE5; }
        QLineEdit:read-only { color: black; }
        QGroupBox {
            border: 1px solid #D5DFE5; border-radius: 3px; margin-top: 9px; padding: 6px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 7px; padding: 0 3px; }
        QTableWidget {
            background: white; gridline-color: #000000; border: 1px solid #688CAF;
            selection-background-color: #0078D7; selection-color: white;
        }
        QHeaderView::section {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFFFFF, stop:1 #F1F2F4);
            border: none; border-right: 1px solid #D5D5D5; border-bottom: 1px solid #D5D5D5;
            padding: 2px 4px;
        }
        QScrollBar:vertical { width: 18px; background: #F0F0F0; margin: 0; }
        QScrollBar::handle:vertical { background: #CDCDCD; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #A6A6A6; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { height: 18px; background: #F0F0F0; margin: 0; }
        QScrollBar::handle:horizontal { background: #CDCDCD; min-width: 30px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QScrollArea { border: none; background: transparent; }
        QToolTip { font-size: 12px; }
    )"));
}

namespace {

domain::DenominationCount toCount(const Counts& counts, const domain::DenominationTable& table,
                                  bool chips) {
    domain::DenominationCount count;
    for (const auto& [cents, qty] : counts) {
        if (qty == 0) continue;
        const auto denomination = chips ? table.findChip(cents) : table.findCurrency(cents);
        if (denomination) {
            count.add(denomination, qty);
        }
    }
    return count;
}

} // namespace

domain::DenominationCount toCurrencyCount(const Counts& counts,
                                          const domain::DenominationTable& table) {
    return toCount(counts, table, false);
}

domain::DenominationCount toChipCount(const Counts& counts,
                                      const domain::DenominationTable& table) {
    return toCount(counts, table, true);
}

Counts toCounts(const domain::DenominationCount& count) {
    Counts counts;
    for (const auto& entry : count.entries()) {
        counts[entry.denomination->cents()] += entry.qty;
    }
    return counts;
}

std::int64_t totalCents(const Counts& counts) {
    std::int64_t total = 0;
    for (const auto& [cents, qty] : counts) {
        total += cents * qty;
    }
    return total;
}

} // namespace pokersoc::ui
