#include "MainWindow.h"
#include "Database.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>
#include <QTabWidget>
#include <QTableView>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("pokersoc-connect");
    resize(1100, 700);

    auto* tb = addToolBar("Actions");
    tb->addAction("Add Player", [this]{ addPlayer(); });
    tb->addAction("Add Session", [this]{ addSession(); });
    tb->addSeparator();
    tb->addAction("Buy-in", [this]{ buyIn(); });
    tb->addAction("Cash-out", [this]{ cashOut(); });

    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    buildPlayersTab();
    buildSessionsTab();
    buildTransactionsTab();
}

void MainWindow::buildPlayersTab() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);

    m_scanBox = new QLineEdit(page);
    m_scanBox->setPlaceholderText("Focus here and scan membership card...");
    lay->addWidget(m_scanBox);

    m_playersModel = new QSqlTableModel(page, Database::db());
    m_playersModel->setTable("players");
    m_playersModel->setEditStrategy(QSqlTableModel::OnFieldChange);
    m_playersModel->select();

    m_playersView = new QTableView(page);
    m_playersView->setModel(m_playersModel);
    m_playersView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_playersView->setAlternatingRowColors(true);
    m_playersView->setSortingEnabled(true);

    lay->addWidget(m_playersView);
    page->setLayout(lay);
    m_tabs->addTab(page, "Players");

    connect(m_scanBox, &QLineEdit::returnPressed, [this]{
        const auto code = m_scanBox->text().trimmed();
        if (code.isEmpty()) return;
        for (int r=0; r<m_playersModel->rowCount(); ++r) {
            if (m_playersModel->record(r).value("member_no").toString() == code) {
                m_playersView->selectRow(r);
                return;
            }
        }
        int row = m_playersModel->rowCount();
        m_playersModel->insertRow(row);
        auto rec = m_playersModel->record(row);
        rec.setValue("member_no", code);
        rec.setValue("first_name","New");
        rec.setValue("last_name","Player");
        rec.setValue("display_name", code);
        m_playersModel->setRecord(row, rec);
        if (!m_playersModel->submitAll())
            QMessageBox::critical(this, "DB Error", m_playersModel->lastError().text());
        else
            m_playersView->selectRow(row);
    });
}

void MainWindow::buildSessionsTab() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);

    m_sessionsModel = new QSqlTableModel(page, Database::db());
    m_sessionsModel->setTable("sessions");
    m_sessionsModel->setEditStrategy(QSqlTableModel::OnFieldChange);
    m_sessionsModel->select();

    m_sessionsView = new QTableView(page);
    m_sessionsView->setModel(m_sessionsModel);
    m_sessionsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsView->setAlternatingRowColors(true);
    m_sessionsView->setSortingEnabled(true);

    lay->addWidget(m_sessionsView);
    page->setLayout(lay);
    m_tabs->addTab(page, "Sessions");
}

void MainWindow::buildTransactionsTab() {
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);

    m_txModel = new QSqlTableModel(page, Database::db());
    m_txModel->setTable("transactions");
    m_txModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_txModel->select();

    m_txView = new QTableView(page);
    m_txView->setModel(m_txModel);
    m_txView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_txView->setAlternatingRowColors(true);
    m_txView->setSortingEnabled(true);

    lay->addWidget(m_txView);
    page->setLayout(lay);
    m_tabs->addTab(page, "Transactions");
}

void MainWindow::addPlayer() {
    int row = m_playersModel->rowCount();
    m_playersModel->insertRow(row);
    auto rec = m_playersModel->record(row);
    rec.setValue("first_name","New");
    rec.setValue("last_name","Player");
    rec.setValue("display_name","New Player");
    m_playersModel->setRecord(row, rec);
    if (!m_playersModel->submitAll())
        QMessageBox::critical(this, "DB Error", m_playersModel->lastError().text());
}

void MainWindow::addSession() {
    int row = m_sessionsModel->rowCount();
    m_sessionsModel->insertRow(row);
    if (!m_sessionsModel->submitAll())
        QMessageBox::critical(this, "DB Error", m_sessionsModel->lastError().text());
}

static bool promptThree(const QString& title, QString aLabel, QString bLabel, QString cLabel,
                        double& a, double& b, QString& staff) {
    bool ok1=false, ok2=false, ok3=false;
    const double av = QInputDialog::getDouble(nullptr, title, aLabel, a, 0, 1e9, 2, &ok1);
    const double bv = QInputDialog::getDouble(nullptr, title, bLabel, b, 0, 1e9, 2, &ok2);
    const QString sv= QInputDialog::getText(nullptr, title, cLabel, QLineEdit::Normal, staff, &ok3);
    if (ok1 && ok2 && ok3) { a=av; b=bv; staff=sv; return true; }
    return false;
}

void MainWindow::buyIn() {
    // minimal inline dialog: cash->chips for selected player/session
    bool ok=false;
    const int playerRow = m_playersView->currentIndex().row();
    if (playerRow < 0) { QMessageBox::information(this,"Buy-in","Select a player first."); return; }

    const int sessionRow = m_sessionsView->currentIndex().row();
    if (sessionRow < 0) { QMessageBox::information(this,"Buy-in","Select a session first."); return; }

    const int playerId = m_playersModel->record(playerRow).value("player_id").toInt();
    const int sessionId= m_sessionsModel->record(sessionRow).value("session_id").toInt();

    double cash=300.0, chips=300.0; QString staff="Dealer";
    if (!promptThree("Buy-in", "Cash In (AUD):", "Chips Out:", "Staff:", cash, chips, staff)) return;

    QSqlQuery q(Database::db());
    q.prepare(R"(INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff, notes)
                 VALUES (?, ?, 'BUYIN', ?, ?, 'Cash', ?, ''))");
    q.addBindValue(sessionId);
    q.addBindValue(playerId);
    q.addBindValue(cash);
    q.addBindValue(chips);
    q.addBindValue(staff);
    if (!q.exec())
        QMessageBox::critical(this, "DB Error", q.lastError().text());
    else
        m_txModel->select();
}

void MainWindow::cashOut() {
    const int playerRow = m_playersView->currentIndex().row();
    if (playerRow < 0) { QMessageBox::information(this,"Cash-out","Select a player first."); return; }

    const int sessionRow = m_sessionsView->currentIndex().row();
    if (sessionRow < 0) { QMessageBox::information(this,"Cash-out","Select a session first."); return; }

    const int playerId = m_playersModel->record(playerRow).value("player_id").toInt();
    const int sessionId= m_sessionsModel->record(sessionRow).value("session_id").toInt();

    double chips=0.0, cash=0.0; QString staff="Dealer";
    if (!promptThree("Cash-out", "Chips In:", "Cash Out (AUD):", "Staff:", chips, cash, staff)) return;

    QSqlQuery q(Database::db());
    q.prepare(R"(INSERT INTO transactions(session_id, player_id, type, cash_amt, chips_amt, method, staff, notes)
                 VALUES (?, ?, 'CASHOUT', ?, ?, 'Cash', ?, ''))");
    q.addBindValue(sessionId);
    q.addBindValue(playerId);
    q.addBindValue(cash);
    q.addBindValue(chips);
    q.addBindValue(staff);
    if (!q.exec())
        QMessageBox::critical(this, "DB Error", q.lastError().text());
    else
        m_txModel->select();
}
