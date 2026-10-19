#include <stdexcept>

#include <QDate>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
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

    QFrame* verify = makePanel("#F5F5F5", color::Gray, 2, 0, 10);
    auto* verifyRow = new QHBoxLayout;
    verifyRow->setSpacing(0);
    verifyRow->addWidget(makeLabel(QStringLiteral("🔍 Verify Age:"), 14, true));
    verifyRow->addSpacing(10);
    idBox_ = new QLineEdit;
    idBox_->setFont(uiFont(14));
    idBox_->setPlaceholderText("Scan or type a player ID, then press Enter");
    verifyRow->addWidget(idBox_, 1);
    verifyRow->addSpacing(10);
    adultButton_ = makeButton(QStringLiteral("✓ Set 18+"), color::LightGreen, 13, true);
    adultButton_->setStyleSheet(buttonStyle(color::LightGreen, "#707070", "1px", "padding:8px 15px;"));
    underageButton_ = makeButton(QStringLiteral("👶 Set Underage"), "#FFFFCC", 13, true);
    underageButton_->setStyleSheet(buttonStyle("#FFFFCC", "#707070", "1px", "padding:8px 15px;"));
    adultButton_->setEnabled(false);
    underageButton_->setEnabled(false);
    verifyRow->addWidget(adultButton_);
    verifyRow->addSpacing(5);
    verifyRow->addWidget(underageButton_);
    panelLayout(verify)->addLayout(verifyRow);
    layout->addWidget(verify);
    layout->addSpacing(10);

    connect(idBox_, &QLineEdit::returnPressed, this, [this] { lookup(); });
    connect(idBox_, &QLineEdit::textChanged, this, [this](const QString& text) {
        const bool known = ctx_.players().contains(toStd(text.trimmed()));
        adultButton_->setEnabled(known);
        underageButton_->setEnabled(known);
    });
    connect(adultButton_, &QPushButton::clicked, this, [this] { setAge(false); });
    connect(underageButton_, &QPushButton::clicked, this, [this] { setAge(true); });

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
    connect(grid_, &QTableWidget::itemSelectionChanged, this, [this] {
        const auto selected = grid_->selectedItems();
        if (selected.isEmpty()) return;
        const QTableWidgetItem* idItem = grid_->item(selected.first()->row(), 0);
        if (idItem) idBox_->setText(idItem->text());
    });

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

    password_ = new PasswordOverlay(this, ctx_.admin(), color::LightGreen, false);
    buildNotification();
    refresh();
    idBox_->setFocus();
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
            player->studyYear() > 0 ? QString::number(player->studyYear()) : QString(),
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

void PlayersScreen::lookup() {
    const std::string id = toStd(idBox_->text().trimmed());
    if (id.empty()) return;
    if (!ctx_.players().contains(id)) {
        QMessageBox::warning(this, "Player Not Found",
                             QStringLiteral("Player '%1' not found in database.").arg(toQ(id)));
        adultButton_->setEnabled(false);
        underageButton_->setEnabled(false);
        return;
    }
    adultButton_->setEnabled(true);
    underageButton_->setEnabled(true);
    for (int row = 0; row < grid_->rowCount(); ++row) {
        if (grid_->item(row, 0) && grid_->item(row, 0)->text() == toQ(id)) {
            grid_->selectRow(row);
            grid_->scrollToItem(grid_->item(row, 0));
            break;
        }
    }
}

void PlayersScreen::setAge(bool underage) {
    const std::string id = toStd(idBox_->text().trimmed());
    if (id.empty()) {
        QMessageBox::warning(this, "No Player Selected", "Please scan or select a player first.");
        return;
    }
    const QString action =
        QStringLiteral("to set '%1' as %2\n🎂 18+ if born on or before: %3")
            .arg(toQ(id), underage ? "Underage" : "18+", toQ(util::adultCutoffDate()));
    password_->ask(action, [this, id, underage] {
        if (!ctx_.players().setUnderage(id, underage)) {
            QMessageBox::warning(this, "Player Not Found",
                                 QStringLiteral("Player '%1' not found in database.").arg(toQ(id)));
            return;
        }
        changed_ = true;
        refresh();
        showNotification(QStringLiteral("✓"), "Age Updated",
                         QStringLiteral("Player '%1' marked as %2.")
                             .arg(toQ(id), underage ? "Underage" : "18+"),
                         true);
    });
}

void PlayersScreen::buildNotification() {
    notification_ = new Overlay(this);
    notification_->card()->setMinimumWidth(320);
    QVBoxLayout* card = notification_->cardLayout();
    notificationIcon_ = makeLabel(QString(), 48, false, QString(), Qt::AlignCenter);
    notificationIcon_->setContentsMargins(0, 0, 0, 10);
    card->addWidget(notificationIcon_);
    notificationTitle_ = makeLabel(QString(), 20, true, QString(), Qt::AlignCenter);
    notificationTitle_->setContentsMargins(0, 0, 0, 10);
    card->addWidget(notificationTitle_);
    notificationText_ = makeLabel(QString(), 14, false, QString(), Qt::AlignCenter);
    notificationText_->setWordWrap(true);
    notificationText_->setContentsMargins(0, 0, 0, 20);
    card->addWidget(notificationText_);
    QPushButton* ok = makeButton("OK", color::LightGreen, 14, true);
    ok->setStyleSheet(buttonStyle(color::LightGreen, "#707070", "1px", "padding:12px 30px;"));
    card->addWidget(ok, 0, Qt::AlignHCenter);
    connect(ok, &QPushButton::clicked, notification_, &QWidget::hide);
}

void PlayersScreen::showNotification(const QString& icon, const QString& title,
                                     const QString& message, bool success) {
    notificationIcon_->setText(icon);
    notificationIcon_->setStyleSheet(QStringLiteral("color:%1;").arg(success ? color::Green : color::Red));
    notificationTitle_->setText(title);
    notificationText_->setText(message);
    notification_->open();
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
