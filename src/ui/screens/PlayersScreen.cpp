#include <stdexcept>

#include <QDate>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "application/ImportExportService.h"
#include "application/PlayerService.h"
#include "domain/player/Player.h"
#include "ui/screens/Screens.h"
#include "ui/SessionContext.h"
#include "ui/Widgets.h"
#include "util/Time.h"

namespace pokersoc::ui {

using namespace domain;

PlayersScreen::PlayersScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(0);

    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Player Database", &close, &subtitle_));
    layout->addSpacing(10);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(changed_); });

    const QStringList headers{"Player ID", "Name",   "Age Status", "Email",      "Student Number",
                              "Degree",    "Study Year", "Arc Member", "Membership", "Created"};
    const int widths[] = {120, 150, 90, 130, 110, 100, 80, 80, 90, 130};
    grid_ = new QTableWidget(0, headers.size());
    grid_->setHorizontalHeaderLabels(headers);
    for (int i = 0; i < headers.size(); ++i) grid_->setColumnWidth(i, widths[i]);
    grid_->horizontalHeader()->setStretchLastSection(true);
    grid_->horizontalHeader()->setHighlightSections(false);
    grid_->verticalHeader()->setVisible(false);
    grid_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    grid_->setSelectionBehavior(QAbstractItemView::SelectRows);
    grid_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(grid_, 1);

    layout->addSpacing(10);
    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    const auto footerButton = [](const QString& text) {
        QPushButton* button = makeButton(text, QString(), 14, true);
        button->setStyleSheet("QPushButton { padding:10px 15px; }");
        return button;
    };
    QPushButton* import = footerButton("Import Players");
    QPushButton* exportButton = footerButton("Export Players");
    QPushButton* closeButton = footerButton("Close");
    footer->addWidget(import);
    footer->addSpacing(10);
    footer->addWidget(exportButton);
    footer->addSpacing(10);
    footer->addWidget(closeButton);
    layout->addLayout(footer);
    connect(import, &QPushButton::clicked, this, [this] { importPlayers(); });
    connect(exportButton, &QPushButton::clicked, this, [this] { exportPlayers(); });
    connect(closeButton, &QPushButton::clicked, this, [this] { emit closed(changed_); });

    refresh();
}

void PlayersScreen::refresh() {
    const auto players = ctx_.players().all();
    subtitle_->setText(QStringLiteral("%1 unique players today")
                           .arg(ctx_.players().attendance().uniquePlayerCount()));

    grid_->setRowCount(static_cast<int>(players.size()));
    int row = 0;
    for (const Player* player : players) {
        const QString arc = player->arcStatus() == ArcStatus::Arc      ? "Yes"
                            : player->arcStatus() == ArcStatus::NonArc ? "No"
                                                                       : "";
        const QString created =
            player->createdAt().empty() ? QString()
                                        : toQ(util::utcToLocal(player->createdAt(), "%Y-%m-%d %H:%M"));
        const QString cells[] = {
            toQ(player->id()),
            toQ(player->name()),
            player->isUnderage() ? "Underage" : "18+",
            toQ(player->email()),
            toQ(player->studentNumber()),
            toQ(player->degree()),
            toQ(player->studyYear()),
            arc,
            membershipTypeLabel(player->membership()),
            created,
        };
        for (int column = 0; column < 10; ++column) {
            auto* item = new QTableWidgetItem(cells[column]);
            if (column == 2 && player->isUnderage()) item->setForeground(QColor(color::Red));
            grid_->setItem(row, column, item);
        }
        ++row;
    }
}

void PlayersScreen::importPlayers() {
    const QString path = QFileDialog::getOpenFileName(this, "Import Player Database", QString(),
                                                      "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) return;
    const auto mode = QMessageBox::question(
        this, "Import Mode",
        "Merge with existing players?\n\nYes = update existing players with the file's data\n"
        "No = only add players that are not already in the database",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (mode == QMessageBox::Cancel) return;
    try {
        const auto result = ctx_.files().importPlayers(toStd(path), mode == QMessageBox::Yes);
        changed_ = true;
        refresh();
        QMessageBox::information(this, "Success",
                                 QStringLiteral("Players imported successfully!\n\n"
                                                "Added: %1\nUpdated: %2\nSkipped: %3")
                                     .arg(result.added)
                                     .arg(result.updated)
                                     .arg(result.skipped));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Error importing players: %1").arg(error.what()));
    }
}

void PlayersScreen::exportPlayers() {
    const QString suggested =
        QStringLiteral("players_%1.csv").arg(QDate::currentDate().toString("yyyy-MM-dd"));
    const QString path = QFileDialog::getSaveFileName(this, "Export Player Database", suggested,
                                                      "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) return;
    try {
        ctx_.files().exportPlayers(toStd(path));
        QMessageBox::information(this, "Success",
                                 QStringLiteral("Players exported successfully to:\n%1").arg(path));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Error",
                              QStringLiteral("Error exporting players: %1").arg(error.what()));
    }
}

} // namespace pokersoc::ui
