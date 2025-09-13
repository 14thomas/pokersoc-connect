#pragma once
#include <QMainWindow>

class QTabWidget;
class QLineEdit;
class QSqlTableModel;
class QTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
private:
    QTabWidget*     m_tabs{nullptr};

    QSqlTableModel* m_playersModel{nullptr};
    QTableView*     m_playersView{nullptr};
    QLineEdit*      m_scanBox{nullptr};

    QSqlTableModel* m_sessionsModel{nullptr};
    QTableView*     m_sessionsView{nullptr};

    QSqlTableModel* m_txModel{nullptr};
    QTableView*     m_txView{nullptr};

    void buildPlayersTab();
    void buildSessionsTab();
    void buildTransactionsTab();

    void addPlayer();
    void addSession();
    void buyIn();
    void cashOut();
};
