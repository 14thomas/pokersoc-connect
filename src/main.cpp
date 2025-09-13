#include <QApplication>
#include <QDir>
#include "Database.h"
#include "MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QDir dir(QDir::currentPath());
    if (!dir.exists("data")) dir.mkdir("data");

    if (!Database::open(QDir("data").filePath("pokersoc.sqlite")))
        return 1;
    if (!Database::ensureSchema("schema.sql"))
        return 1;

    MainWindow w;
    w.show();
    return app.exec();
}
