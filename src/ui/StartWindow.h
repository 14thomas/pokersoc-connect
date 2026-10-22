#pragma once

#include <optional>

#include <QDialog>
#include <QShowEvent>
#include <QString>

#include "domain/session/CashGameMode.h"

class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QSpacerItem;

namespace pokersoc::ui {

/// "What type of session are you creating?" — Cash Game or Tournament.
/// Used when Settings starts another session from inside an open one.
class SessionModeDialog : public QDialog {
    Q_OBJECT
public:
    explicit SessionModeDialog(QWidget* parent = nullptr);
    domain::SessionModeType mode() const;

private:
    QRadioButton* cashGame_;
    QRadioButton* tournament_;
};

/// Ask for a session type, then where to save the new session file.
/// `withTime` adds HHmm to the suggested file name. Returns false if cancelled.
bool askNewSession(QWidget* parent, bool withTime, QString& path, domain::SessionModeType& mode);

/// The launcher. New session expands on this same window: player database,
/// cash game or tournament, and cash-game limits. Nothing else pops up.
class StartWindow : public QDialog {
    Q_OBJECT
public:
    explicit StartWindow(QWidget* parent = nullptr);

    const QString& sessionPath() const noexcept { return path_; }
    /// Set when the user chose "New session".
    std::optional<domain::SessionModeType> newMode() const noexcept { return newMode_; }
    /// Player CSV to import. Empty when this is a new database, or when opening
    /// an existing session.
    QString importPath() const { return importPath_; }
    /// Limits entered for a new cash game. Empty for tournaments and for
    /// sessions that are only being opened.
    std::optional<domain::CashGameConfig> cashConfig() const { return cashConfig_; }

protected:
    void showEvent(QShowEvent* event) override;

private:
    void showNewSessionSetup();
    void refreshSetup();
    void fitWindow();
    void browseDatabase();
    void browseSessionFile();
    void setNewDatabase(bool enabled);
    void updateNewDatabaseButton();
    void updateDatabaseStatus();
    void updateSuggestedSessionPath();
    QString suggestedSessionPath() const;
    void clearError();
    void showError(const QString& text);
    void startNewSession();

    QString path_;
    std::optional<domain::SessionModeType> newMode_;
    QString importPath_;
    std::optional<domain::CashGameConfig> cashConfig_;

    bool setupVisible_ = false;
    bool newDatabase_ = false;
    bool fileEdited_ = false;

    QScrollArea* formScroll_ = nullptr;
    QSpacerItem* bottomSpacer_ = nullptr;
    QGroupBox* databaseBox_ = nullptr;
    QLineEdit* databasePath_ = nullptr;
    QPushButton* newDatabaseButton_ = nullptr;
    QLabel* databaseStatus_ = nullptr;
    QGroupBox* modeBox_ = nullptr;
    QRadioButton* cashGame_ = nullptr;
    QRadioButton* tournament_ = nullptr;
    QGroupBox* limitsBox_ = nullptr;
    QLineEdit* minBox_ = nullptr;
    QLineEdit* maxBox_ = nullptr;
    QLineEdit* limitBox_ = nullptr;
    QGroupBox* fileBox_ = nullptr;
    QLineEdit* sessionFile_ = nullptr;
    QLabel* formError_ = nullptr;
    QPushButton* startButton_ = nullptr;
};

} // namespace pokersoc::ui
