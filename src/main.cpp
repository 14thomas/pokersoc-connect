#include <QApplication>
#include <QWidget>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget w;
    w.setWindowTitle("pokersoc-connect");
    w.resize(480, 280);
    w.show();

    return app.exec();
}
