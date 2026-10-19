#include "ui/StartWindow.h"

#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "ui/Style.h"

namespace pokersoc::ui {

using domain::SessionModeType;

namespace {

const char* const kSessionFilter = "Session (*.sqlite);;All files (*.*)";

QLabel* grayNote(const QString& text) {
    QLabel* label = makeLabel(text, 12, false, color::Gray);
    label->setWordWrap(true);
    label->setContentsMargins(24, 0, 0, 0);
    return label;
}

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
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    layout->addWidget(makeLabel("pokersoc-connect", 28, true));
    layout->addSpacing(8);
    layout->addWidget(makeLabel("Start a new session or open an existing one."));
    layout->addSpacing(16);

    const auto addButton = [layout](const QString& text) {
        auto* button = new QPushButton(text);
        button->setFixedWidth(320);
        layout->addWidget(button, 0, Qt::AlignHCenter);
        layout->addSpacing(10);
        return button;
    };
    QPushButton* newButton = addButton("New Session...");
    QPushButton* openButton = addButton("Open Session...");
    QPushButton* importButton = addButton(QStringLiteral("📥 Import Player Database..."));
    importButton->setToolTip("Import players from a previous session");
    autoImport_ = new QCheckBox("Auto-import players on session open");
    autoImport_->setChecked(true);
    layout->addWidget(autoImport_, 0, Qt::AlignHCenter);
    layout->addSpacing(16);

    auto* footer = new QHBoxLayout;
    footer->addStretch(1);
    auto* exitButton = new QPushButton("Exit");
    footer->addWidget(exitButton);
    layout->addLayout(footer);

    connect(newButton, &QPushButton::clicked, this, [this] {
        QString path;
        SessionModeType mode = SessionModeType::CashGame;
        if (!askNewSession(this, false, path, mode)) return;
        path_ = path;
        newMode_ = mode;
        accept();
    });
    connect(openButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QString(), QString(), kSessionFilter);
        if (path.isEmpty()) return;
        path_ = path;
        newMode_.reset();
        if (!QFileInfo::exists(path)) newMode_ = SessionModeType::CashGame;
        accept();
    });
    connect(importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, "Select Player Database to Import",
                                                          QString(), "CSV Files (*.csv);;All files (*.*)");
        if (path.isEmpty()) return;
        importPath_ = path;
        QMessageBox::information(this, "Import Ready",
                                 QStringLiteral("Player database selected:\n%1\n\nPlayers will be "
                                                "imported when you open or create a session.")
                                     .arg(path));
    });
    connect(exitButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString StartWindow::importPath() const {
    return autoImport_->isChecked() ? importPath_ : QString();
}

} // namespace pokersoc::ui
