#pragma once

#include <optional>

#include <QDialog>
#include <QString>

#include "domain/session/SessionMode.h"

class QCheckBox;
class QRadioButton;

namespace pokersoc::ui::qt {

/// "What type of session are you creating?" — Cash Game or Tournament.
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

/// The launcher: start a new session, open an existing one, and optionally pick
/// a player database to import once the session is open.
class StartWindow : public QDialog {
    Q_OBJECT
public:
    explicit StartWindow(QWidget* parent = nullptr);

    const QString& sessionPath() const noexcept { return path_; }
    /// Set when the user chose "New Session...".
    std::optional<domain::SessionModeType> newMode() const noexcept { return newMode_; }
    /// The player CSV to import on open (empty if none / auto-import is off).
    QString importPath() const;

private:
    QString path_;
    std::optional<domain::SessionModeType> newMode_;
    QString importPath_;
    QCheckBox* autoImport_;
};

} // namespace pokersoc::ui::qt
