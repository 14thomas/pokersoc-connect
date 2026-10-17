#include <cmath>
#include <stdexcept>

#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "application/AdminService.h"
#include "application/FoodService.h"
#include "application/ImportExportService.h"
#include "domain/food/ProductCatalog.h"
#include "domain/session/CashGameMode.h"
#include "persistence/AppStore.h"
#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/StartWindow.h"
#include "ui/qt/Widgets.h"

namespace pokersoc::ui::qt {

using namespace domain;

QWidget* makeScreenHeader(const QString& title, QPushButton** close, QLabel** subtitle) {
    auto* header = new QWidget;
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(0, 0, 0, 0);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(makeLabel(title, 24, true));
    if (subtitle) {
        *subtitle = makeLabel(QString(), 12, false, color::Gray);
        titles->addWidget(*subtitle);
    }
    row->addLayout(titles, 1);
    auto* button = new QPushButton(QStringLiteral("✕"));
    button->setFont(uiFont(16));
    button->setFixedSize(30, 30);
    row->addWidget(button, 0, Qt::AlignVCenter);
    if (close) *close = button;
    return header;
}

namespace {

QGroupBox* makeGroup(const QString& title) {
    auto* group = new QGroupBox(title);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);
    return group;
}

QVBoxLayout* groupLayout(QGroupBox* group) { return static_cast<QVBoxLayout*>(group->layout()); }

QPushButton* fixedButton(const QString& text, int width) {
    auto* button = new QPushButton(text);
    button->setFixedWidth(width);
    return button;
}

QHBoxLayout* buttonRow(std::initializer_list<QPushButton*> buttons) {
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    for (QPushButton* button : buttons) row->addWidget(button);
    row->addStretch(1);
    return row;
}

/// "12.50" / "$12.50" -> cents; nullopt for anything else.
std::optional<std::int64_t> parseDollars(const QString& text) {
    QString cleaned = text.trimmed();
    if (cleaned.startsWith('$')) cleaned.remove(0, 1);
    bool ok = false;
    const double dollars = cleaned.toDouble(&ok);
    if (!ok || dollars < 0 || !std::isfinite(dollars)) return std::nullopt;
    return static_cast<std::int64_t>(std::llround(dollars * 100.0));
}

QString dollars(Money amount) { return QString::number(amount.toDollars(), 'f', 2); }

QString stamp(const char* format) { return QDateTime::currentDateTime().toString(format); }

} // namespace

SettingsScreen::SettingsScreen(SessionContext& context, QWidget* parent) : Screen(context, parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    QPushButton* close = nullptr;
    layout->addWidget(makeScreenHeader("Settings", &close));
    layout->addSpacing(16);
    connect(close, &QPushButton::clicked, this, [this] { emit closed(changed_); });

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(0, 0, 8, 0);
    sections->setSpacing(16);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    // ---- Database Management ----
    QGroupBox* database = makeGroup("Database Management");
    QPushButton* importSettings = fixedButton("Import Settings...", 160);
    QPushButton* exportSettings = fixedButton("Export Settings...", 160);
    QPushButton* backup = fixedButton("Backup Database...", 160);
    QPushButton* restore = fixedButton("Restore Database...", 160);
    groupLayout(database)->addLayout(buttonRow({importSettings, exportSettings}));
    groupLayout(database)->addSpacing(10);
    groupLayout(database)->addLayout(buttonRow({backup, restore}));
    sections->addWidget(database);

    connect(importSettings, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Import Settings", QString(),
                                                          "JSON files (*.json);;All files (*.*)");
        if (path.isEmpty()) return;
        const auto merge = QMessageBox::question(this, "Import Settings",
                                                 "Merge into existing catalog? (No = replace)",
                                                 QMessageBox::Yes | QMessageBox::No |
                                                     QMessageBox::Cancel);
        if (merge == QMessageBox::Cancel) return;
        try {
            const std::size_t count = ctx_.files().importCatalog(toStd(path), merge == QMessageBox::Yes);
            changed_ = true;
            refreshProducts();
            QMessageBox::information(this, "Import Settings",
                                     QStringLiteral("Imported %1 product(s).").arg(count));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Import Error",
                                  QStringLiteral("Error importing settings:\n%1").arg(error.what()));
        }
    });
    connect(exportSettings, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, "Export Settings", "pokersoc-settings.json",
                                                          "JSON files (*.json);;All files (*.*)");
        if (path.isEmpty()) return;
        try {
            ctx_.files().exportCatalog(toStd(path));
            QMessageBox::information(this, "Export Settings",
                                     QStringLiteral("Settings exported to:\n%1").arg(path));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Export Error",
                                  QStringLiteral("Error exporting settings:\n%1").arg(error.what()));
        }
    });
    connect(backup, &QPushButton::clicked, this, [this] {
        const QString suggested =
            QStringLiteral("pokersoc-backup-%1.db").arg(stamp("yyyy-MM-dd-HHmm"));
        const QString path = QFileDialog::getSaveFileName(this, "Backup Database", suggested,
                                                          "SQLite Database (*.db);;All files (*.*)");
        if (path.isEmpty()) return;
        try {
            ctx_.store().backupTo(toStd(path));
            QMessageBox::information(this, "Backup Complete",
                                     QStringLiteral("Database backed up successfully to:\n%1").arg(path));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Backup Error",
                                  QStringLiteral("Error backing up database:\n%1").arg(error.what()));
        }
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Restore Database", QString(),
            "SQLite Database (*.db *.sqlite);;All files (*.*)");
        if (path.isEmpty()) return;
        if (QMessageBox::question(this, "Restore Database",
                                  "This will close the current session and open the selected "
                                  "database. Continue?") != QMessageBox::Yes) {
            return;
        }
        emit switchSession(path, std::nullopt);
    });

    // ---- Session Management ----
    QGroupBox* session = makeGroup("Session Management");
    QPushButton* newSession = fixedButton("New Session...", 160);
    QPushButton* closeSessionButton = fixedButton("Close Session", 160);
    groupLayout(session)->addLayout(buttonRow({newSession, closeSessionButton}));
    groupLayout(session)->addSpacing(10);
    QLabel* exportCaption = makeLabel("Export players who participated in this session:");
    exportCaption->setContentsMargins(0, 0, 0, 5);
    groupLayout(session)->addWidget(exportCaption);
    QPushButton* exportSession = fixedButton("Export Session Players...", 200);
    groupLayout(session)->addLayout(buttonRow({exportSession}));
    sections->addWidget(session);

    connect(newSession, &QPushButton::clicked, this, [this] {
        QString path;
        SessionModeType mode = SessionModeType::CashGame;
        if (!askNewSession(this, true, path, mode)) return;
        if (QMessageBox::question(this, "New Session",
                                  "This will close the current session and create a new one. "
                                  "Continue?") != QMessageBox::Yes) {
            return;
        }
        emit switchSession(path, mode);
    });
    connect(closeSessionButton, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "Close Session",
                                  "This will close the current session. Continue?") ==
            QMessageBox::Yes) {
            emit closeSession();
        }
    });
    connect(exportSession, &QPushButton::clicked, this, [this] {
        const QString suggested =
            QStringLiteral("session_players_%1.csv").arg(stamp("yyyy-MM-dd_HHmmss"));
        const QString path = QFileDialog::getSaveFileName(this, "Export Session Players", suggested,
                                                          "CSV files (*.csv);;All files (*.*)");
        if (path.isEmpty()) return;
        try {
            ctx_.files().exportSessionPlayers(toStd(path));
            QMessageBox::information(
                this, "Export Complete",
                QStringLiteral("Session players exported successfully to:\n%1").arg(path));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Export Error",
                                  QStringLiteral("Error exporting session players:\n%1")
                                      .arg(error.what()));
        }
    });

    // ---- Cash Game Limits (cash games only) ----
    if (!ctx_.isTournament()) {
        QGroupBox* limits = makeGroup("Cash Game Limits");
        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(5);
        const CashGameConfig& config = ctx_.cashGame().config();
        auto* minBox = new QLineEdit(dollars(config.minBuyIn));
        auto* maxBox = new QLineEdit(dollars(config.maxBuyIn));
        auto* limitBox = new QLineEdit(dollars(config.sessionLimit));
        const QString captions[] = {"Minimum buy-in ($):",
                                    "Maximum buy-in ($, 0 = none):",
                                    "Session cap per player ($, 0 = none):"};
        QLineEdit* boxes[] = {minBox, maxBox, limitBox};
        for (int i = 0; i < 3; ++i) {
            grid->addWidget(makeLabel(captions[i]), i, 0);
            boxes[i]->setFixedWidth(120);
            grid->addWidget(boxes[i], i, 1);
        }
        grid->setColumnStretch(2, 1);
        groupLayout(limits)->addLayout(grid);
        groupLayout(limits)->addSpacing(10);
        QPushButton* saveLimits = fixedButton("Save Limits", 120);
        groupLayout(limits)->addLayout(buttonRow({saveLimits}));
        sections->addWidget(limits);
        connect(saveLimits, &QPushButton::clicked, this, [this, minBox, maxBox, limitBox] {
            const auto min = parseDollars(minBox->text());
            const auto max = parseDollars(maxBox->text());
            const auto limit = parseDollars(limitBox->text());
            if (!min || !max || !limit || *min < 0 || *max < 0 || *limit < 0 ||
                (*max > 0 && *max < *min)) {
                QMessageBox::warning(this, "Cash Game Limits",
                                     "Please enter valid amounts. Maximum 0 means no per-buy-in "
                                     "limit; otherwise maximum must be at least the minimum. "
                                     "Session cap 0 means no session cap.");
                return;
            }
            CashGameConfig& config = ctx_.cashGame().config();
            config.minBuyIn = Money(*min);
            config.maxBuyIn = Money(*max);
            config.sessionLimit = Money(*limit);
            ctx_.saveCashGameConfig();
            changed_ = true;
            QMessageBox::information(this, "Cash Game Limits", "Cash game limits saved.");
        });
    }

    // ---- Food Items Management ----
    QGroupBox* foodGroup = makeGroup("Food Items Management");
    QLabel* foodCaption = makeLabel("Manage food items for sale:");
    foodCaption->setContentsMargins(0, 0, 0, 10);
    groupLayout(foodGroup)->addWidget(foodCaption);
    QGroupBox* addItem = makeGroup("Add New Item");
    auto* form = new QGridLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(5);
    name_ = new QLineEdit;
    price_ = new QLineEdit("0.00");
    image_ = new QLineEdit;
    auto* addButton = new QPushButton("Add Item");
    auto* browse = new QPushButton("Browse");
    form->addWidget(makeLabel("Name:"), 0, 0);
    form->addWidget(name_, 0, 1);
    form->addWidget(makeLabel("Price:"), 0, 2);
    form->addWidget(price_, 0, 3);
    form->addWidget(addButton, 0, 4);
    form->addWidget(makeLabel("Image:"), 1, 0);
    form->addWidget(image_, 1, 1, 1, 3);
    form->addWidget(browse, 1, 4);
    form->setColumnMinimumWidth(0, 80);
    form->setColumnMinimumWidth(2, 80);
    form->setColumnMinimumWidth(4, 100);
    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);
    groupLayout(addItem)->addLayout(form);
    groupLayout(foodGroup)->addWidget(addItem);
    groupLayout(foodGroup)->addSpacing(10);
    deleteProduct_ = fixedButton("Delete Selected", 120);
    deleteProduct_->setEnabled(false);
    groupLayout(foodGroup)->addLayout(buttonRow({deleteProduct_}));
    groupLayout(foodGroup)->addSpacing(10);
    products_ = new QTableWidget(0, 3);
    products_->setHorizontalHeaderLabels({"Name", "Price", "Image Path"});
    products_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    products_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    products_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    products_->setColumnWidth(1, 120);
    products_->horizontalHeader()->setHighlightSections(false);
    products_->verticalHeader()->setVisible(false);
    products_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    products_->setSelectionBehavior(QAbstractItemView::SelectRows);
    products_->setSelectionMode(QAbstractItemView::SingleSelection);
    products_->setMinimumHeight(200);
    groupLayout(foodGroup)->addWidget(products_);
    sections->addWidget(foodGroup);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select Image", QString(),
            "Image files (*.png *.jpg *.jpeg *.gif *.bmp);;All files (*.*)");
        if (!path.isEmpty()) image_->setText(path);
    });
    connect(addButton, &QPushButton::clicked, this, [this] {
        const QString name = name_->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, "Add Item", "Please enter a product name.");
            return;
        }
        const auto cents = parseDollars(price_->text());
        if (!cents) {
            QMessageBox::warning(this, "Add Item", "Please enter a valid price.");
            return;
        }
        Product product;
        product.name = toStd(name);
        product.price = Money(*cents);
        product.image = toStd(image_->text().trimmed());
        try {
            ctx_.food().saveProduct(product);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Add Item",
                                  QStringLiteral("Error adding item:\n%1").arg(error.what()));
            return;
        }
        changed_ = true;
        name_->clear();
        price_->setText("0.00");
        image_->clear();
        refreshProducts();
    });
    connect(products_, &QTableWidget::itemSelectionChanged, this,
            [this] { deleteProduct_->setEnabled(!products_->selectedItems().isEmpty()); });
    connect(deleteProduct_, &QPushButton::clicked, this, [this] {
        const auto selected = products_->selectedItems();
        if (selected.isEmpty()) return;
        const QTableWidgetItem* item = products_->item(selected.first()->row(), 0);
        const long long id = item->data(Qt::UserRole).toLongLong();
        if (QMessageBox::question(this, "Confirm Delete",
                                  QStringLiteral("Are you sure you want to delete '%1'?")
                                      .arg(item->text())) != QMessageBox::Yes) {
            return;
        }
        ctx_.food().removeProduct(id);
        changed_ = true;
        refreshProducts();
    });

    // ---- Admin Settings ----
    QGroupBox* admin = makeGroup("Admin Settings");
    QLabel* passwordCaption = makeLabel("Admin Password (for deleting transactions):");
    passwordCaption->setContentsMargins(0, 0, 0, 5);
    groupLayout(admin)->addWidget(passwordCaption);
    auto* password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setFixedWidth(200);
    QPushButton* savePassword = fixedButton("Save Password", 120);
    auto* passwordRow = new QHBoxLayout;
    passwordRow->setSpacing(10);
    passwordRow->addWidget(password);
    passwordRow->addWidget(savePassword);
    passwordRow->addStretch(1);
    groupLayout(admin)->addLayout(passwordRow);
    QLabel* defaultHint = makeLabel("Default password: 1234", 11, false, color::Gray);
    defaultHint->setContentsMargins(0, 5, 0, 0);
    groupLayout(admin)->addWidget(defaultHint);
    sections->addWidget(admin);
    connect(savePassword, &QPushButton::clicked, this, [this, password] {
        if (!ctx_.admin().setPassword(toStd(password->text()))) {
            QMessageBox::warning(this, "Admin Password", "Password cannot be empty.");
            return;
        }
        password->clear();
        QMessageBox::information(this, "Admin Password", "Admin password updated successfully.");
    });

    // ---- About ----
    QGroupBox* about = makeGroup("About");
    const auto addAbout = [&](QLabel* label, int bottom, int left = 0) {
        label->setWordWrap(true);
        label->setContentsMargins(left, 0, 0, bottom);
        groupLayout(about)->addWidget(label);
    };
    addAbout(makeLabel("pokersoc-connect v1.6.7", 12, true), 5);
    addAbout(makeLabel("A poker society management application for PokerSoc UNSW, developed in house."),
             10);
    addAbout(makeLabel("Features:", 12, true), 5);
    const char* features[] = {
        "Added a very light tournament mode.",
        "Activity log filters to the scanned or entered player ID.",
        "Player logs track membership tier (Bronze, Silver, Gold, etc.).",
        "New player signup prompts for membership type (tournament mode only).",
        "Tournament buy-in and rebuy prices vary by membership type.",
        "Tournament info shows total buy-ins and rebuys, with per-tier breakdown.",
        "tom is definitely the coolest person in the world.",
        "suyee is decent too.",
    };
    for (const char* feature : features) addAbout(makeLabel(QStringLiteral("• ") + feature), 2, 20);
    sections->addWidget(about);
    sections->addStretch(1);

    refreshProducts();
}

void SettingsScreen::refreshProducts() {
    const auto all = ctx_.catalog().all();
    products_->setRowCount(static_cast<int>(all.size()));
    int row = 0;
    for (const Product* product : all) {
        auto* name = new QTableWidgetItem(toQ(product->name));
        name->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(product->id));
        products_->setItem(row, 0, name);
        products_->setItem(row, 1, new QTableWidgetItem(money(product->price)));
        products_->setItem(row, 2, new QTableWidgetItem(toQ(product->image)));
        ++row;
    }
    deleteProduct_->setEnabled(false);
}

} // namespace pokersoc::ui::qt
