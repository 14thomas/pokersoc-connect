#include "ui/StartWindow.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTime>
#include <QVBoxLayout>

#include "ui/Style.h"

namespace pokersoc::ui {

using domain::CashGameConfig;
using domain::Money;
using domain::SessionModeType;

namespace {

const char* const kSessionFilter = "Session (*.sqlite);;All files (*.*)";

QLabel* grayNote(const QString& text) {
    QLabel* label = makeLabel(text, 12, false, color::Gray);
    label->setWordWrap(true);
    return label;
}

QGroupBox* section(const QString& title) {
    auto* box = new QGroupBox(title);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(14, 14, 14, 10);
    layout->setSpacing(6);
    return box;
}

QVBoxLayout* sectionLayout(QGroupBox* box) { return static_cast<QVBoxLayout*>(box->layout()); }

/// "12.50" / "$12.50" -> cents; nullopt for anything else.
std::optional<std::int64_t> parseDollars(const QString& text) {
    QString cleaned = text.trimmed();
    if (cleaned.startsWith('$')) cleaned.remove(0, 1);
    cleaned.remove(',');
    bool ok = false;
    const double dollars = cleaned.toDouble(&ok);
    if (!ok || dollars < 0 || !std::isfinite(dollars)) return std::nullopt;
    return static_cast<std::int64_t>(std::llround(dollars * 100.0));
}

QString dollars(Money amount) { return QString::number(amount.toDollars(), 'f', 2); }

} // namespace

// ---------------------------------------------------------------------------

SessionModeDialog::SessionModeDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Select Session Type");
    setMinimumWidth(400);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    layout->addWidget(makeLabel("What type of session are you creating?", 16, true));
    layout->addSpacing(16);

    cashGame_ = new QRadioButton("Cash Game");
    cashGame_->setFont(uiFont(14));
    cashGame_->setChecked(true);
    layout->addWidget(cashGame_);
    layout->addSpacing(8);
    layout->addWidget(grayNote("Standard buy-ins with flexible amounts and cash-outs."));
    layout->addSpacing(12);
    tournament_ = new QRadioButton("Tournament");
    tournament_->setFont(uiFont(14));
    layout->addWidget(tournament_);
    layout->addSpacing(8);
    layout->addWidget(grayNote("Fixed buy-in amounts, tournament settings, and blind structure."));
    layout->addSpacing(20);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton("Cancel");
    auto* ok = new QPushButton("OK");
    cancel->setFixedWidth(90);
    ok->setFixedWidth(90);
    ok->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addSpacing(10);
    buttons->addWidget(ok);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}

SessionModeType SessionModeDialog::mode() const {
    return tournament_->isChecked() ? SessionModeType::Tournament : SessionModeType::CashGame;
}

bool askNewSession(QWidget* parent, bool withTime, QString& path, SessionModeType& mode) {
    SessionModeDialog dialog(parent);
    if (dialog.exec() != QDialog::Accepted) return false;
    mode = dialog.mode();
    const QString stamp = withTime ? QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmm")
                                   : QDate::currentDate().toString("yyyy-MM-dd");
    const QString suggested =
        QStringLiteral("Session-%1-%2.sqlite")
            .arg(mode == SessionModeType::Tournament ? "Tournament" : "CashGame", stamp);
    path = QFileDialog::getSaveFileName(parent, withTime ? "New Session" : QString(), suggested,
                                        kSessionFilter);
    return !path.isEmpty();
}

// ---------------------------------------------------------------------------

StartWindow::StartWindow(QWidget* parent) : QDialog(parent) {
    setWindowTitle("pokersoc-connect - Start");
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 20, 28, 16);
    layout->setSpacing(10);

    layout->addWidget(makeLabel("pokersoc-connect", 28, true));
    layout->addSpacing(8);
    layout->addWidget(makeLabel("New session or open session.", 16));

    auto* newButton = new QPushButton("New session");
    auto* openButton = new QPushButton("Open session");
    newButton->setFixedSize(240, 48);
    openButton->setFixedSize(240, 48);
    newButton->setFont(uiFont(18, true));
    openButton->setFont(uiFont(18, true));
    auto* choices = new QHBoxLayout;
    choices->addStretch(1);
    choices->addWidget(newButton);
    choices->addSpacing(16);
    choices->addWidget(openButton);
    choices->addStretch(1);
    layout->addLayout(choices);

    auto* form = new QWidget;
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 4, 0, 0);
    formLayout->setSpacing(12);

    databaseBox_ = section("Player database");
    databasePath_ = new QLineEdit;
    databasePath_->setPlaceholderText("Select a player database (.csv)");
    auto* browseDatabaseButton = new QPushButton("Browse...");
    browseDatabaseButton->setFixedWidth(120);
    auto* databaseRow = new QHBoxLayout;
    databaseRow->setSpacing(10);
    databaseRow->addWidget(databasePath_, 1);
    databaseRow->addWidget(browseDatabaseButton);
    sectionLayout(databaseBox_)->addLayout(databaseRow);
    newDatabaseButton_ = new QPushButton("This is a new database");
    newDatabaseButton_->setCheckable(true);
    newDatabaseButton_->setMinimumHeight(40);
    sectionLayout(databaseBox_)->addWidget(newDatabaseButton_);
    databaseStatus_ = grayNote("Select a player database, or press \"This is a new database\".");
    sectionLayout(databaseBox_)->addWidget(databaseStatus_);
    formLayout->addWidget(databaseBox_);

    modeBox_ = section("Cash game or tournament");
    cashGame_ = new QRadioButton("Cash game");
    cashGame_->setFont(uiFont(15, true));
    cashGame_->setChecked(true);
    tournament_ = new QRadioButton("Tournament");
    tournament_->setFont(uiFont(15, true));
    sectionLayout(modeBox_)->addWidget(cashGame_);
    sectionLayout(modeBox_)->addWidget(
        grayNote("Standard buy-ins with flexible amounts and cash-outs."));
    sectionLayout(modeBox_)->addSpacing(6);
    sectionLayout(modeBox_)->addWidget(tournament_);
    sectionLayout(modeBox_)->addWidget(
        grayNote("Fixed buy-in amounts, tournament settings, and blind structure."));
    formLayout->addWidget(modeBox_);

    limitsBox_ = section("Cash game limits");
    sectionLayout(limitsBox_)->addWidget(grayNote(
        "Buy-in rules for this cash game. Maximum 0 means no per-buy-in limit. "
        "Session cap 0 means no cap."));
    const CashGameConfig defaults;
    minBox_ = new QLineEdit(dollars(defaults.minBuyIn));
    maxBox_ = new QLineEdit(dollars(defaults.maxBuyIn));
    limitBox_ = new QLineEdit(dollars(defaults.sessionLimit));
    const QString captions[] = {"Minimum buy-in ($):", "Maximum buy-in ($, 0 = none):",
                                "Session cap per player ($, 0 = none):"};
    QLineEdit* boxes[] = {minBox_, maxBox_, limitBox_};
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);
    for (int i = 0; i < 3; ++i) {
        grid->addWidget(makeLabel(captions[i], 14), i, 0);
        boxes[i]->setFixedWidth(140);
        grid->addWidget(boxes[i], i, 1);
    }
    grid->setColumnStretch(2, 1);
    sectionLayout(limitsBox_)->addLayout(grid);
    formLayout->addWidget(limitsBox_);

    fileBox_ = section("Session file");
    sessionFile_ = new QLineEdit;
    auto* browseSessionButton = new QPushButton("Browse...");
    browseSessionButton->setFixedWidth(120);
    auto* fileRow = new QHBoxLayout;
    fileRow->setSpacing(10);
    fileRow->addWidget(sessionFile_, 1);
    fileRow->addWidget(browseSessionButton);
    sectionLayout(fileBox_)->addLayout(fileRow);
    sectionLayout(fileBox_)->addWidget(
        grayNote("Where this new session is saved. Change the name if that file is already taken."));
    formLayout->addWidget(fileBox_);
    formLayout->addStretch(1);

    formScroll_ = new QScrollArea;
    formScroll_->setWidgetResizable(true);
    formScroll_->setFrameShape(QFrame::NoFrame);
    formScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    formScroll_->setWidget(form);
    form->setAutoFillBackground(false);
    layout->addWidget(formScroll_, 1);

    formError_ = makeLabel(QString(), 13, false, color::DarkRed);
    formError_->setWordWrap(true);
    layout->addWidget(formError_);

    startButton_ = new QPushButton("Start session");
    startButton_->setFixedSize(240, 44);
    startButton_->setFont(uiFont(18, true));
    startButton_->setDefault(true);
    layout->addWidget(startButton_, 0, Qt::AlignHCenter);
    bottomSpacer_ = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addSpacerItem(bottomSpacer_);

    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    auto* exitButton = new QPushButton("Exit");
    exitButton->setFixedWidth(100);
    footer->addWidget(exitButton);
    layout->addLayout(footer);

    connect(newButton, &QPushButton::clicked, this, &StartWindow::showNewSessionSetup);
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path =
            QFileDialog::getOpenFileName(this, "Open session", QString(), kSessionFilter);
        if (path.isEmpty()) return;
        path_ = path;
        newMode_.reset();
        importPath_.clear();
        cashConfig_.reset();
        if (!QFileInfo::exists(path)) newMode_ = SessionModeType::CashGame;
        accept();
    });
    connect(browseDatabaseButton, &QPushButton::clicked, this, &StartWindow::browseDatabase);
    connect(newDatabaseButton_, &QPushButton::toggled, this, &StartWindow::setNewDatabase);
    connect(databasePath_, &QLineEdit::textEdited, this, [this] {
        if (!databasePath_->text().trimmed().isEmpty()) {
            QSignalBlocker blocker(newDatabaseButton_);
            newDatabase_ = false;
            newDatabaseButton_->setChecked(false);
        }
        clearError();
        refreshSetup();
    });
    const auto onMode = [this](bool checked) {
        if (!checked) return;
        updateSuggestedSessionPath();
        clearError();
        refreshSetup();
    };
    connect(cashGame_, &QRadioButton::toggled, this, onMode);
    connect(tournament_, &QRadioButton::toggled, this, onMode);
    connect(browseSessionButton, &QPushButton::clicked, this, &StartWindow::browseSessionFile);
    connect(sessionFile_, &QLineEdit::textEdited, this, [this] {
        fileEdited_ = true;
        clearError();
    });
    for (QLineEdit* box : boxes) {
        connect(box, &QLineEdit::textEdited, this, &StartWindow::clearError);
    }
    connect(startButton_, &QPushButton::clicked, this, &StartWindow::startNewSession);
    connect(exitButton, &QPushButton::clicked, this, &QDialog::reject);

    refreshSetup();
}

void StartWindow::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    fitWindow();
}

void StartWindow::showNewSessionSetup() {
    setupVisible_ = true;
    if (sessionFile_->text().trimmed().isEmpty()) updateSuggestedSessionPath();
    refreshSetup();
}

void StartWindow::refreshSetup() {
    const bool databaseReady =
        setupVisible_ && (newDatabase_ || !databasePath_->text().trimmed().isEmpty());
    formScroll_->setVisible(setupVisible_);
    databaseBox_->setVisible(setupVisible_);
    modeBox_->setVisible(databaseReady);
    limitsBox_->setVisible(databaseReady && cashGame_->isChecked());
    fileBox_->setVisible(databaseReady);
    startButton_->setVisible(databaseReady);
    formError_->setVisible(!formError_->text().isEmpty());
    if (setupVisible_) {
        bottomSpacer_->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum);
    } else {
        bottomSpacer_->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    }
    layout()->invalidate();
    updateNewDatabaseButton();
    updateDatabaseStatus();
    fitWindow();
}

void StartWindow::fitWindow() {
    QScreen* screen = this->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1600, 900);
    const int frame = std::max(0, frameGeometry().height() - height());
    const int maxWidth = std::max(840, area.width() - 48);
    const int maxHeight = std::max(520, area.height() - frame - 8);

    int content = 0;
    if (formScroll_->isVisible()) {
        const QWidget* sections[] = {databaseBox_, modeBox_, limitsBox_, fileBox_};
        int shown = 0;
        for (const QWidget* sectionWidget : sections) {
            if (!sectionWidget->isVisible()) continue;
            content += sectionWidget->sizeHint().height();
            ++shown;
        }
        if (shown > 1) content += 6 * (shown - 1);
        content += 20;
    }

    auto* box = static_cast<QVBoxLayout*>(layout());
    int chrome = box->contentsMargins().top() + box->contentsMargins().bottom();
    int visible = 0;
    for (int i = 0; i < box->count(); ++i) {
        QLayoutItem* item = box->itemAt(i);
        if (!item || item->widget() == formScroll_) continue;
        if (QWidget* widget = item->widget()) {
            if (!widget->isVisible()) continue;
            chrome += widget->sizeHint().height();
            ++visible;
        } else if (QLayout* nested = item->layout()) {
            chrome += nested->sizeHint().height();
            ++visible;
        }
    }
    chrome += box->spacing() * (visible + (content > 0 ? 1 : 0));

    int height = std::max(520, chrome + content + 8);
    int scrollHeight = content;
    if (height > maxHeight) {
        scrollHeight = std::max(180, content - (height - maxHeight));
        height = maxHeight;
    }
    formScroll_->setMinimumHeight(formScroll_->isVisible() ? scrollHeight : 0);
    formScroll_->setMaximumHeight(formScroll_->isVisible() ? std::max(scrollHeight, 1)
                                                           : QWIDGETSIZE_MAX);

    setMaximumSize(maxWidth, maxHeight);
    resize(std::min(maxWidth, std::max(840, box->sizeHint().width())), height);
}

void StartWindow::browseDatabase() {
    const QString path = QFileDialog::getOpenFileName(this, "Select player database", QString(),
                                                      "CSV files (*.csv);;All files (*.*)");
    if (path.isEmpty()) return;
    databasePath_->setText(path);
    QSignalBlocker blocker(newDatabaseButton_);
    newDatabase_ = false;
    newDatabaseButton_->setChecked(false);
    clearError();
    refreshSetup();
}

void StartWindow::browseSessionFile() {
    const QString current = sessionFile_->text().trimmed();
    const QString path = QFileDialog::getSaveFileName(
        this, "New session", current.isEmpty() ? suggestedSessionPath() : current, kSessionFilter);
    if (path.isEmpty()) return;
    fileEdited_ = true;
    sessionFile_->setText(path);
    clearError();
}

void StartWindow::setNewDatabase(bool enabled) {
    newDatabase_ = enabled;
    if (enabled) databasePath_->clear();
    if (newDatabaseButton_->isChecked() != enabled) {
        QSignalBlocker blocker(newDatabaseButton_);
        newDatabaseButton_->setChecked(enabled);
    }
    clearError();
    refreshSetup();
}

void StartWindow::updateNewDatabaseButton() {
    if (newDatabaseButton_->isChecked()) {
        newDatabaseButton_->setStyleSheet(buttonStyle(color::LightGreen, color::DarkGreen));
    } else {
        newDatabaseButton_->setStyleSheet(QString());
    }
}

void StartWindow::updateDatabaseStatus() {
    if (newDatabase_) {
        databaseStatus_->setText("New database. This session starts with no players.");
    } else if (!databasePath_->text().trimmed().isEmpty()) {
        databaseStatus_->setText("Players will be imported from this database when the session starts.");
    } else {
        databaseStatus_->setText("Select a player database, or press \"This is a new database\".");
    }
}

void StartWindow::updateSuggestedSessionPath() {
    if (fileEdited_) return;
    sessionFile_->setText(suggestedSessionPath());
}

QString StartWindow::suggestedSessionPath() const {
    const QString stamp = QDate::currentDate().toString("yyyy-MM-dd");
    const QString kind = tournament_->isChecked() ? QStringLiteral("Tournament")
                                                  : QStringLiteral("CashGame");
    const QString base = QStringLiteral("Session-%1-%2").arg(kind, stamp);
    QString path = QDir::current().filePath(base + QStringLiteral(".sqlite"));
    if (QFileInfo::exists(path)) {
        path = QDir::current().filePath(base + QLatin1Char('-') + QTime::currentTime().toString("HHmm") +
                                        QStringLiteral(".sqlite"));
    }
    return path;
}

void StartWindow::clearError() {
    if (formError_->text().isEmpty()) return;
    formError_->clear();
    formError_->hide();
    fitWindow();
}

void StartWindow::showError(const QString& text) {
    formError_->setText(text);
    formError_->setVisible(true);
    fitWindow();
}

void StartWindow::startNewSession() {
    if (newDatabase_) {
        importPath_.clear();
    } else {
        const QString database = databasePath_->text().trimmed();
        if (database.isEmpty() || !QFileInfo::exists(database)) {
            showError("Choose a player database that exists, or press \"This is a new database\".");
            return;
        }
        importPath_ = database;
    }

    const bool tournament = tournament_->isChecked();
    if (!tournament) {
        const auto min = parseDollars(minBox_->text());
        const auto max = parseDollars(maxBox_->text());
        const auto limit = parseDollars(limitBox_->text());
        if (!min || !max || !limit || (*max > 0 && *max < *min)) {
            showError("Enter valid cash game limits. Maximum 0 means no per-buy-in limit; "
                      "otherwise it must be at least the minimum. Session cap 0 means no cap.");
            return;
        }
        CashGameConfig config;
        config.minBuyIn = Money(*min);
        config.maxBuyIn = Money(*max);
        config.sessionLimit = Money(*limit);
        cashConfig_ = config;
    } else {
        cashConfig_.reset();
    }

    QString session = sessionFile_->text().trimmed();
    if (session.isEmpty()) {
        showError("Enter a file name for the new session.");
        return;
    }
    if (!session.endsWith(".sqlite", Qt::CaseInsensitive)) session += QStringLiteral(".sqlite");
    const QFileInfo info(session);
    if (info.exists()) {
        showError("That session file already exists. Choose a different name.");
        return;
    }
    path_ = info.absoluteFilePath();
    newMode_ = tournament ? SessionModeType::Tournament : SessionModeType::CashGame;
    accept();
}

} // namespace pokersoc::ui
