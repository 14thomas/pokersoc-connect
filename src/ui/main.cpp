#include <exception>
#include <memory>
#include <optional>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QScreen>

#include "application/ImportExportService.h"
#include "domain/money/DenominationTable.h"
#include "ui/MainWindow.h"
#include "ui/SessionContext.h"
#include "ui/StartWindow.h"
#include "ui/Style.h"

using namespace pokersoc;
using namespace pokersoc::ui;

namespace {

/// Windows draws the window icon in a square. Pad the mark instead of stretching it.
QIcon appIcon() {
    const QPixmap mark(assetPath(QStringLiteral("Brand/pokersoc_black_icon.png")));
    if (mark.isNull()) return {};
    const int side = qMax(mark.width(), mark.height());
    QPixmap square(side, side);
    square.fill(Qt::transparent);
    const QPixmap fitted = mark.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter painter(&square);
    painter.drawPixmap((side - fitted.width()) / 2, (side - fitted.height()) / 2, fitted);
    return QIcon(square);
}

/// Shows anything thrown out of an event handler instead of terminating
/// (the WPF app's DispatcherUnhandledException handler).
class Application : public QApplication {
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override {
        try {
            return QApplication::notify(receiver, event);
        } catch (const std::exception& error) {
            QMessageBox::critical(nullptr, "pokersoc-connect",
                                  QStringLiteral("Unhandled error:\n%1").arg(error.what()));
        }
        return false;
    }
};

/// Owns the session windows: opens a session file into a MainWindow and swaps
/// windows when Settings asks for another session.
class SessionLauncher {
public:
    explicit SessionLauncher(const domain::DenominationTable& table) : table_(table) {}

    bool open(const QString& path, std::optional<domain::SessionModeType> newMode,
              const QString& importPath, QWidget* parent,
              std::optional<domain::CashGameConfig> cashConfig = std::nullopt) {
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (window_ && QFileInfo(toQ(window_->context().path())).absoluteFilePath() == absolute) {
            QMessageBox::information(parent, "pokersoc-connect", "That session is already open.");
            return false;
        }
        if (!QDir().mkpath(QFileInfo(absolute).absolutePath())) {
            QMessageBox::critical(parent, "pokersoc-connect",
                                  "Could not create folder for database:\n" + QFileInfo(absolute).absolutePath());
            return false;
        }

        std::unique_ptr<SessionContext> context;
        try {
            context = std::make_unique<SessionContext>(
                toStd(absolute), table_, newMode.value_or(domain::SessionModeType::CashGame),
                cashConfig);
        } catch (const std::exception& error) {
            QMessageBox::critical(parent, "pokersoc-connect",
                                  QStringLiteral("Could not open database:\n%1").arg(error.what()));
            return false;
        }

        if (!importPath.isEmpty() && QFileInfo::exists(importPath)) {
            try {
                context->files().importPlayers(toStd(importPath), true);
            } catch (const std::exception& error) {
                QMessageBox::warning(parent, "Import Warning",
                                     QStringLiteral("Could not import players:\n%1\n\nContinuing without "
                                                    "import.")
                                         .arg(error.what()));
            }
        }

        QPointer<MainWindow> previous = window_;
        auto* window = new MainWindow(std::move(context));
        window_ = window;
        QObject::connect(window, &MainWindow::switchSessionRequested, window,
                         [this, window](const QString& next, std::optional<domain::SessionModeType> mode) {
                             open(next, mode, QString(), window);
                         });
        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            const QRect area = screen->availableGeometry();
            window->move(area.center() - QPoint(window->width() / 2, window->height() / 2));
        }
        window->show();
        if (previous) previous->forceClose();
        return true;
    }

private:
    const domain::DenominationTable& table_;
    QPointer<MainWindow> window_;
};

} // namespace

int main(int argc, char* argv[]) {
    Application app(argc, argv);
    app.setApplicationName("pokersoc-connect");
    app.setWindowIcon(appIcon());
    applyApplicationStyle(app);

    const domain::DenominationTable table = domain::DenominationTable::standardAustralian();
    SessionLauncher launcher(table);

    // `pokersoc_gui <session file> [--tournament]` skips the start window.
    QString path;
    domain::SessionModeType mode = domain::SessionModeType::CashGame;
    for (const QString& argument : app.arguments().mid(1)) {
        if (argument == "--tournament") {
            mode = domain::SessionModeType::Tournament;
        } else if (path.isEmpty()) {
            path = argument;
        }
    }

    app.setQuitOnLastWindowClosed(false);
    if (!path.isEmpty()) {
        if (!launcher.open(path, mode, QString(), nullptr)) return 1;
    } else {
        StartWindow start;
        if (start.exec() != QDialog::Accepted || start.sessionPath().isEmpty()) return 0;
        if (!launcher.open(start.sessionPath(), start.newMode(), start.importPath(), nullptr,
                           start.cashConfig()))
            return 1;
    }
    app.setQuitOnLastWindowClosed(true);
    return app.exec();
}
