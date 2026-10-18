#pragma once

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <vector>

#include "domain/session/TournamentConfig.h"
#include "ui/qt/Style.h"

namespace pokersoc::ui::qt {

inline QTableWidget* makeEditorTable(const QStringList& headers) {
    auto* table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setMinimumHeight(160);
    table->setMaximumHeight(240);
    table->setFont(uiFont(12));
    return table;
}

inline void setIntCell(QTableWidget* table, int row, int column, int value) {
    auto* item = new QTableWidgetItem(QString::number(value));
    item->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, column, item);
}

inline void setTextCell(QTableWidget* table, int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, column, item);
}

inline int cellInt(const QTableWidget* table, int row, int column, bool* ok) {
    const QTableWidgetItem* item = table->item(row, column);
    const QString text = item ? item->text().trimmed() : QString();
    return text.toInt(ok);
}

inline QString cellText(const QTableWidget* table, int row, int column) {
    const QTableWidgetItem* item = table->item(row, column);
    return item ? item->text().trimmed() : QString();
}

inline bool blindRowIsBreak(const QTableWidget* table, int row) {
    const QTableWidgetItem* item = table->item(row, 0);
    return item && item->data(Qt::UserRole).toBool();
}

inline void refreshBlindLabels(QTableWidget* table) {
    int number = 1;
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* item = table->item(row, 0);
        if (!item) continue;
        item->setText(blindRowIsBreak(table, row) ? QStringLiteral("Break") : QString::number(number++));
    }
}

inline void addBlindRow(QTableWidget* table, const domain::BlindLevel& level) {
    const int row = table->rowCount();
    table->insertRow(row);
    const auto dragFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
                           Qt::ItemIsDropEnabled;
    setIntCell(table, row, 0, row + 1);
    QTableWidgetItem* label = table->item(row, 0);
    label->setFlags(dragFlags);
    label->setData(Qt::UserRole, level.isBreak);
    label->setText(level.isBreak ? QStringLiteral("Break") : QString::number(row + 1));
    setTextCell(table, row, 1, toQ(level.smallBlind));
    setTextCell(table, row, 2, toQ(level.bigBlind));
    setTextCell(table, row, 3, level.hasAnte() ? toQ(level.ante) : QString());
    setIntCell(table, row, 4, level.durationMinutes);
    if (level.isBreak) {
        for (int column : {1, 2, 3}) {
            QTableWidgetItem* item = table->item(row, column);
            item->setText(QStringLiteral("\u2014"));
            item->setFlags(dragFlags);
        }
    }
}

inline void addPrizeRow(QTableWidget* table, const domain::PrizeShare& prize) {
    const int row = table->rowCount();
    table->insertRow(row);
    setIntCell(table, row, 0, prize.place);
    setIntCell(table, row, 1, prize.percent);
}

inline bool readBlinds(QTableWidget* table, std::vector<domain::BlindLevel>& out, QString& error) {
    out.clear();
    if (table->rowCount() < 1) {
        error = "Add at least one blind level.";
        return false;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        domain::BlindLevel level;
        level.isBreak = blindRowIsBreak(table, row);
        bool okMin = false;
        level.durationMinutes = cellInt(table, row, 4, &okMin);
        if (level.isBreak) {
            if (!okMin || level.durationMinutes < 1) {
                error = QStringLiteral("Break %1 needs a duration of 1 minute or more.").arg(row + 1);
                return false;
            }
            out.push_back(level);
            continue;
        }
        level.smallBlind = toStd(cellText(table, row, 1));
        level.bigBlind = toStd(cellText(table, row, 2));
        QString anteText = cellText(table, row, 3);
        if (anteText == QStringLiteral("\u2014") || anteText == QStringLiteral("-")) anteText.clear();
        level.ante = toStd(anteText);
        if (level.smallBlind.empty() || level.bigBlind.empty() || !okMin || level.durationMinutes < 1) {
            error = QStringLiteral("Level %1 needs small/big blind text (e.g. 100 or 1.5K) and a "
                                   "duration of 1 minute or more.")
                        .arg(row + 1);
            return false;
        }
        out.push_back(level);
    }
    return true;
}

inline bool readPrizes(QTableWidget* table, std::vector<domain::PrizeShare>& out, QString& error) {
    out.clear();
    if (table->rowCount() < 1) {
        error = "Add at least one prize place.";
        return false;
    }
    int total = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        bool okPlace = false, okPct = false;
        domain::PrizeShare prize;
        prize.place = cellInt(table, row, 0, &okPlace);
        prize.percent = cellInt(table, row, 1, &okPct);
        if (!okPlace || !okPct || prize.place < 1 || prize.percent < 0 || prize.percent > 100) {
            error = QStringLiteral("Prize row %1 needs a place of 1 or more and a percent from 0 to 100.")
                        .arg(row + 1);
            return false;
        }
        total += prize.percent;
        out.push_back(prize);
    }
    if (total > 100) {
        error = QStringLiteral("Prize percents add up to %1%. They cannot exceed 100%.").arg(total);
        return false;
    }
    return true;
}

} // namespace pokersoc::ui::qt
