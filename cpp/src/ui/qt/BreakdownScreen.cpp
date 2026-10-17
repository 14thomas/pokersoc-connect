#include <cstdlib>

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "application/Ledger.h"
#include "domain/money/DenominationCount.h"
#include "domain/transaction/BuyIn.h"
#include "domain/transaction/CashOut.h"
#include "domain/transaction/CashboxTransactions.h"
#include "domain/transaction/FoodSale.h"
#include "domain/transaction/TransactionLog.h"
#include "ui/qt/Screens.h"
#include "ui/qt/SessionContext.h"
#include "ui/qt/Widgets.h"
#include "util/Time.h"

namespace pokersoc::ui::qt {

using namespace domain;

namespace {

QString chipColour(std::int64_t cents) {
    switch (cents) {
        case 5: return "White (5c)";
        case 25: return "Red (25c)";
        case 100: return "Blue ($1)";
        case 500: return "Black ($5)";
        case 2500: return "White Plaque ($25)";
        default: return "Custom";
    }
}

/// A read-only DataGrid-style table sized to its rows.
QTableWidget* makeTable(const DenominationCount& count, bool chips, int qtyWidth, int valueWidth,
                        const QString& method = QString()) {
    QStringList headers;
    if (!method.isEmpty()) headers << "Method";
    headers << "Denomination";
    if (chips) headers << "Colour";
    headers << "Qty" << "Value";
    auto* table = new QTableWidget(static_cast<int>(count.entries().size()), headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QHeaderView* header = table->horizontalHeader();
    const int last = headers.size() - 1;
    for (int column = 0; column < headers.size(); ++column) {
        if (column == last) {
            header->setSectionResizeMode(column, QHeaderView::Fixed);
            table->setColumnWidth(column, valueWidth);
        } else if (column == last - 1) {
            header->setSectionResizeMode(column, QHeaderView::Fixed);
            table->setColumnWidth(column, qtyWidth);
        } else if (!method.isEmpty() && column == 0) {
            header->setSectionResizeMode(column, QHeaderView::Fixed);
            table->setColumnWidth(column, 80);
        } else {
            header->setSectionResizeMode(column, QHeaderView::Stretch);
        }
    }
    int row = 0;
    for (const auto& entry : count.entries()) {
        const std::int64_t cents = entry.denomination->cents();
        int column = 0;
        if (!method.isEmpty()) table->setItem(row, column++, new QTableWidgetItem(method));
        table->setItem(row, column++, new QTableWidgetItem(denomLabel(cents)));
        if (chips) table->setItem(row, column++, new QTableWidgetItem(chipColour(cents)));
        table->setItem(row, column++, new QTableWidgetItem(QString::number(std::abs(entry.qty))));
        table->setItem(row, column, new QTableWidgetItem(money(cents * std::abs(entry.qty))));
        ++row;
    }
    table->resizeRowsToContents();
    int height = header->sizeHint().height() + 2 * table->frameWidth();
    for (int r = 0; r < table->rowCount(); ++r) height += table->rowHeight(r);
    table->setFixedHeight(height);
    return table;
}

QLabel* sectionTitle(const QString& text, int top = 10) {
    QLabel* label = makeLabel(text, 12, true);
    label->setContentsMargins(0, top, 0, 6);
    return label;
}

QFrame* separator() {
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color:#A0A0A0;");
    return line;
}

QString titleFor(const Transaction& transaction) {
    switch (transaction.type()) {
        case TransactionType::BuyIn: return "Buy-in breakdown";
        case TransactionType::Rebuy: return "Rebuy breakdown";
        case TransactionType::CashOut: return "Cash-out breakdown";
        case TransactionType::FloatAdd: return "Float addition";
        case TransactionType::LostChips: return "Lost Chips (Tips)";
        case TransactionType::CashTip: return "Cash Tip";
        case TransactionType::Exchange: return "Exchange breakdown";
        case TransactionType::FoodSale: return "Food sale";
    }
    return "Transaction";
}

} // namespace

BreakdownScreen::BreakdownScreen(SessionContext& context, long long transactionId, QWidget* parent)
    : Screen(context, parent), id_(transactionId) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* body = new QVBoxLayout(content);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    const Transaction* transaction = ctx_.log().find(id_);
    if (!transaction) {
        body->addWidget(makeLabel("Transaction not found", 18, true));
    } else if (transaction->type() == TransactionType::CashOut) {
        buildCashOut(*transaction, body);
    } else {
        buildGeneric(*transaction, body);
    }
    body->addStretch(1);

    // Footer: [Total]  [🗑️ Delete]  [Back]
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 10, 0, 0);
    footer->addStretch(1);
    if (transaction && transaction->type() != TransactionType::CashOut) {
        Money total;
        switch (transaction->type()) {
            case TransactionType::BuyIn:
            case TransactionType::Rebuy:
                total = static_cast<const BuyIn&>(*transaction).cashReceived().total();
                break;
            case TransactionType::FoodSale:
                total = static_cast<const FoodSale&>(*transaction).payment().total();
                break;
            default:
                total = transaction->amount();
                break;
        }
        footer->addWidget(makeLabel("Total: " + money(total), 12, true));
        footer->addSpacing(12);
    }
    if (transaction) {
        QPushButton* remove = makeButton(QStringLiteral("🗑️ Delete"), QString(), 12, true);
        remove->setStyleSheet(buttonStyle(color::OrangeRed, "#707070", "1px", "padding:2px; color:white;"));
        remove->setFixedWidth(100);
        footer->addWidget(remove);
        footer->addSpacing(10);
        connect(remove, &QPushButton::clicked, this, [this] {
            password_->ask("to delete this transaction", [this] {
                const auto result = ctx_.ledger().undo(id_);
                if (!result.success) {
                    QMessageBox::critical(this, "Delete Error",
                                          "Error deleting: " + toQ(result.message));
                    return;
                }
                emit closed(true);
            });
        });
    }
    QPushButton* back = makeButton("Back", QString(), 12);
    back->setStyleSheet(buttonStyle(color::LightCoral, "#707070", "1px", "padding:2px;"));
    back->setFixedWidth(120);
    footer->addWidget(back);
    layout->addLayout(footer);
    connect(back, &QPushButton::clicked, this, [this] { emit closed(false); });

    password_ = new PasswordOverlay(this, ctx_.admin());
}

namespace {

void addHeader(QVBoxLayout* layout, const QString& title, const QString& subtitle,
               const std::string& time, const QString& info) {
    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(0);
    titleRow->addWidget(makeLabel(title, 20, true));
    if (!subtitle.isEmpty()) {
        titleRow->addSpacing(12);
        QLabel* sub = makeLabel(subtitle, 14, false, color::Gray);
        sub->setContentsMargins(0, 4, 0, 0);
        titleRow->addWidget(sub, 0, Qt::AlignBottom);
    }
    titleRow->addStretch(1);
    layout->addLayout(titleRow);
    if (!time.empty()) {
        QLabel* when = makeLabel(toQ(util::utcToLocal(time, "%Y-%m-%d %H:%M:%S")), 12, false, color::Gray);
        when->setContentsMargins(0, 4, 0, 0);
        layout->addWidget(when);
    }
    if (!info.isEmpty()) {
        QLabel* extra = makeLabel(info, 12, false, "#00008B");
        extra->setWordWrap(true);
        extra->setContentsMargins(0, 4, 0, 0);
        layout->addWidget(extra);
    }
    layout->addSpacing(8);
}

} // namespace

void BreakdownScreen::buildGeneric(const Transaction& transaction, QVBoxLayout* layout) {
    QString subtitle;
    QString info = toQ(transaction.notes());
    const auto addCash = [&](const QString& heading, const DenominationCount& count,
                             const QString& method = QString()) {
        if (count.entries().empty()) return;
        layout->addWidget(sectionTitle(heading));
        layout->addWidget(makeTable(count, false, 80, 120, method));
    };
    const auto addChips = [&](const QString& heading, const DenominationCount& count) {
        if (count.entries().empty()) return;
        layout->addWidget(sectionTitle(heading, 6));
        layout->addWidget(makeTable(count, true, 80, 120));
    };

    switch (transaction.type()) {
        case TransactionType::BuyIn:
        case TransactionType::Rebuy: {
            const auto& buyIn = static_cast<const BuyIn&>(transaction);
            subtitle = "Cash received + change given back";
            if (!buyIn.label().empty()) {
                info = QStringLiteral("%1: %2").arg(toQ(buyIn.label()), money(buyIn.buyInAmount()));
            }
            addHeader(layout, titleFor(transaction), subtitle, transaction.time(), info);
            addCash("Cash received", buyIn.cashReceived());
            addCash("Change given back", buyIn.changeGiven());
            break;
        }
        case TransactionType::FloatAdd: {
            addHeader(layout, titleFor(transaction), QString(), transaction.time(), QString());
            addCash("Cash received", static_cast<const FloatAdd&>(transaction).cash());
            break;
        }
        case TransactionType::CashTip: {
            addHeader(layout, titleFor(transaction), info, transaction.time(), QString());
            addCash("Cash received", static_cast<const CashTip&>(transaction).cash());
            break;
        }
        case TransactionType::LostChips: {
            addHeader(layout, titleFor(transaction), info, transaction.time(), QString());
            addChips("Chips turned in", static_cast<const LostChips&>(transaction).chips());
            break;
        }
        case TransactionType::Exchange: {
            const auto& exchange = static_cast<const Exchange&>(transaction);
            addHeader(layout, titleFor(transaction), "Cash given out + cash received",
                      transaction.time(), info);
            addCash("Cash given out", exchange.given());
            addCash("Cash received", exchange.received());
            break;
        }
        case TransactionType::FoodSale: {
            const auto& sale = static_cast<const FoodSale&>(transaction);
            addHeader(layout, titleFor(transaction), QString(), transaction.time(), QString());
            for (const SaleLine& line : sale.lines()) {
                QLabel* product = makeLabel(QStringLiteral("%1 x%2 @ %3 = %4")
                                                .arg(toQ(line.productName))
                                                .arg(line.qty)
                                                .arg(money(line.unitPrice), money(line.total())),
                                            16);
                product->setContentsMargins(0, 0, 0, 5);
                layout->addWidget(product);
            }
            if (!transaction.playerId().empty()) {
                layout->addWidget(makeLabel("Player: " + toQ(transaction.playerId()), 16));
            }
            const bool chips = sale.method() == PaymentMethod::Chips;
            if (!sale.payment().entries().empty()) {
                QLabel* heading = makeLabel("Payment Details:", 14, true);
                heading->setContentsMargins(0, 10, 0, 6);
                layout->addWidget(heading);
                layout->addWidget(makeTable(sale.payment(), false, 60, 100, chips ? "CHIP" : "CASH"));
            }
            if (!sale.change().entries().empty()) {
                QLabel* heading = makeLabel("Change Given:", 14, true);
                heading->setContentsMargins(0, 10, 0, 6);
                layout->addWidget(heading);
                layout->addWidget(makeTable(sale.change(), false, 80, 120));
            }
            break;
        }
        case TransactionType::CashOut:
            break;
    }
}

void BreakdownScreen::buildCashOut(const Transaction& transaction, QVBoxLayout* layout) {
    const auto& cashOut = static_cast<const CashOut&>(transaction);
    addHeader(layout, titleFor(transaction), "Transaction balance (GIVE = TAKE)", transaction.time(),
              QString());
    layout->addSpacing(4);

    const auto makeSide = [](const QString& border) {
        auto* panel = new QFrame;
        panel->setObjectName("side");
        panel->setStyleSheet(
            QStringLiteral("QFrame#side { border:2px solid %1; border-radius:5px; }").arg(border));
        auto* inner = new QVBoxLayout(panel);
        inner->setContentsMargins(10, 10, 10, 10);
        inner->setSpacing(0);
        return panel;
    };
    const auto boxed = [](const QString& border, const QString& heading, const QString& text) {
        auto* box = new QFrame;
        box->setObjectName("box");
        box->setStyleSheet(
            QStringLiteral("QFrame#box { border:2px solid %1; border-radius:3px; }").arg(border));
        auto* inner = new QVBoxLayout(box);
        inner->setContentsMargins(8, 8, 8, 8);
        inner->setSpacing(5);
        inner->addWidget(makeLabel(heading, 12, true, border));
        QLabel* body = makeLabel(text, 14);
        body->setWordWrap(true);
        inner->addWidget(body);
        return box;
    };
    const QString darkBlue = "#00008B";
    const QString darkGreen = color::DarkGreen;

    // TAKE
    QFrame* take = makeSide(darkBlue);
    auto* takeLayout = static_cast<QVBoxLayout*>(take->layout());
    takeLayout->addWidget(makeLabel("TAKE (What we receive)", 16, true, darkBlue));
    takeLayout->addSpacing(10);
    if (!cashOut.chipsReturned().entries().empty()) {
        takeLayout->addWidget(sectionTitle("Chips turned in:", 5));
        takeLayout->addWidget(makeTable(cashOut.chipsReturned(), true, 60, 100));
        QLabel* sum = makeLabel("Chips Sum: " + money(cashOut.chipValue()), 14, true, darkBlue,
                                Qt::AlignRight);
        sum->setContentsMargins(0, 5, 0, 10);
        takeLayout->addWidget(sum);
    }
    if (!cashOut.extraCash().entries().empty()) {
        takeLayout->addWidget(sectionTitle("Extra cash added by player:"));
        takeLayout->addWidget(makeTable(cashOut.extraCash(), false, 60, 100));
    }
    takeLayout->addSpacing(10);
    takeLayout->addWidget(separator());
    takeLayout->addSpacing(10);
    const Money taken = cashOut.chipValue() + cashOut.extraCash().total();
    takeLayout->addWidget(makeLabel("Total Taken: " + money(taken), 16, true, darkBlue, Qt::AlignRight));

    // GIVE
    QFrame* give = makeSide(darkGreen);
    auto* giveLayout = static_cast<QVBoxLayout*>(give->layout());
    giveLayout->addWidget(makeLabel("GIVE (What player receives)", 16, true, darkGreen));
    giveLayout->addSpacing(10);
    if (!cashOut.cashPaidOut().entries().empty()) {
        giveLayout->addWidget(sectionTitle("Cash paid out to player:", 5));
        giveLayout->addWidget(makeTable(cashOut.cashPaidOut(), false, 60, 100));
    }
    if (!cashOut.food().empty()) {
        giveLayout->addSpacing(10);
        giveLayout->addWidget(boxed(color::DarkOrange, "Food purchased:",
                                    money(cashOut.foodTotal()) + ": " +
                                        toQ(describeSaleLines(cashOut.food()))));
    }
    if (!cashOut.tip().isZero()) {
        giveLayout->addSpacing(10);
        giveLayout->addWidget(boxed(color::Purple, "Tips given:", money(cashOut.tip())));
    }
    giveLayout->addSpacing(10);
    giveLayout->addWidget(separator());
    giveLayout->addSpacing(10);
    const Money given = cashOut.cashPaidOut().total() + cashOut.foodTotal() + cashOut.tip();
    giveLayout->addWidget(makeLabel("Total Given: " + money(given), 16, true, darkGreen, Qt::AlignRight));

    auto* sides = new QHBoxLayout;
    sides->setSpacing(20);
    sides->addWidget(take, 1, Qt::AlignTop);
    sides->addWidget(give, 1, Qt::AlignTop);
    layout->addLayout(sides);
}

} // namespace pokersoc::ui::qt
