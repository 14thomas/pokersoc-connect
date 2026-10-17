#include <iomanip>
#include <iostream>

#include "application/FoodService.h"
#include "domain/food/ProductCatalog.h"
#include "domain/money/DenominationTable.h"
#include "domain/player/Player.h"
#include "ui/console/ConsoleApp.h"
#include "ui/console/ConsoleIO.h"

namespace pokersoc::ui {

using domain::Money;
using domain::Product;
using domain::SaleLine;

namespace {

Money roundUpToFiveCents(Money amount) {
    return Money::fromCents(((amount.cents() + 4) / 5) * 5);
}

void printProducts(const std::vector<const Product*>& products) {
    if (products.empty()) {
        std::cout << "    (no products yet)\n";
        return;
    }
    for (std::size_t i = 0; i < products.size(); ++i) {
        std::cout << "    " << std::setw(2) << (i + 1) << ") " << std::left << std::setw(24)
                  << products[i]->name << std::right << products[i]->price.toString() << '\n';
    }
}

/// Parse "3", "3x2", "s1", "s1x2" -> (product, qty).
const Product* parsePick(const std::string& text, const std::vector<const Product*>& products,
                         const domain::ProductCatalog& catalog, int& qty) {
    std::string item = text;
    qty = 1;
    const auto split = text.find_first_of("xX*");
    if (split != std::string::npos) {
        item = text.substr(0, split);
        try {
            qty = std::stoi(text.substr(split + 1));
        } catch (...) {
            return nullptr;
        }
        if (qty <= 0) return nullptr;
    }
    try {
        if (!item.empty() && (item.front() == 's' || item.front() == 'S')) {
            return catalog.slot(std::stoi(item.substr(1)));
        }
        const int index = std::stoi(item);
        if (index >= 1 && index <= static_cast<int>(products.size())) {
            return products[static_cast<std::size_t>(index - 1)];
        }
    } catch (...) {
    }
    return nullptr;
}

} // namespace

void ConsoleApp::foodMenu() {
    while (!inputClosed()) {
        std::cout << "\n--- Food & drink ---\n"
                  << "  1) Sell\n"
                  << "  2) Manage products\n"
                  << "  3) Quick slots\n"
                  << "  0) Back\n";
        const std::string choice = ask("Choice: ", "0");
        if (choice == "1") sellFood();
        else if (choice == "2") manageCatalog();
        else if (choice == "3") manageSlots();
        else if (choice == "0") return;
    }
}

std::optional<std::vector<SaleLine>> ConsoleApp::pickFood(const std::string& title) {
    const auto products = catalog_.all();
    std::vector<SaleLine> basket;

    std::cout << "\n  --- " << title << " ---\n";
    std::string slots;
    for (int slot = 1; slot <= domain::ProductCatalog::kSlotCount; ++slot) {
        if (const Product* product = catalog_.slot(slot)) {
            slots += "  s" + std::to_string(slot) + ") " + product->name;
        }
    }
    if (!slots.empty()) std::cout << "  Quick:" << slots << '\n';
    printProducts(products);
    std::cout << "  Enter a number (or s1-s6), optionally x qty (e.g. 3x2).\n"
              << "  d = done, u = undo last, x = cancel.\n";

    while (true) {
        const std::string input = ask("  Add: ", "d");
        if (inputClosed() && input == "d" && basket.empty()) return std::nullopt;
        if (input == "x" || input == "X") return std::nullopt;
        if (input == "d" || input == "D") break;
        if (input == "u" || input == "U") {
            if (!basket.empty()) basket.pop_back();
        } else {
            int qty = 1;
            const Product* product = parsePick(input, products, catalog_, qty);
            if (!product) {
                std::cout << "  Not a product.\n";
                continue;
            }
            bool merged = false;
            for (auto& line : basket) {
                if (line.productName == product->name && line.unitPrice == product->price) {
                    line.qty += qty;
                    merged = true;
                }
            }
            if (!merged) basket.push_back(SaleLine{product->name, product->price, qty});
        }
        std::cout << "  Basket: " << (basket.empty() ? "(empty)" : domain::describeSaleLines(basket))
                  << "  = " << domain::saleLinesTotal(basket).toString() << '\n';
    }
    return basket;
}

void ConsoleApp::sellFood() {
    if (catalog_.empty()) {
        std::cout << "  No products yet; add some under Manage products.\n";
        return;
    }
    const auto basket = pickFood("Sell");
    if (!basket || basket->empty()) return;
    const Money total = domain::saleLinesTotal(*basket);
    std::cout << "  Total " << total.toString() << '\n';

    std::string playerId;
    if (const domain::Player* player = currentPlayer()) {
        playerId = player->id();
    }

    bool chips = false;
    if (food_->chipPaymentAllowed()) {
        chips = askInt("  Pay with 1) Cash 2) Chips [1]: ", 1) == 2;
    }

    const auto& denominations = chips ? table_.chips() : cashbox_.currency();
    const Money exact = roundUpToFiveCents(total);
    const auto paid = askCount(std::string("  ") + (chips ? "Chips" : "Cash") + " received [exact " +
                                   exact.toString() + "]: ",
                               denominations);
    if (!paid) return;
    const domain::DenominationCount payment = paid->empty() ? breakdown(exact, denominations) : *paid;

    const auto result = chips ? food_->sellForChips(playerId, *basket, payment)
                              : food_->sellForCash(playerId, *basket, payment);
    std::cout << "  " << result.message << '\n';
    if (result.success && !result.change.empty()) {
        std::cout << "  Change:\n";
        printCount(result.change);
    }
}

void ConsoleApp::manageCatalog() {
    while (!inputClosed()) {
        const auto products = catalog_.all();
        std::cout << "\n  --- Products ---\n";
        printProducts(products);
        std::cout << "  1) Add  2) Edit  3) Delete  0) Back\n";
        const std::string choice = ask("Choice: ", "0");

        if (choice == "1") {
            Product product;
            product.name = ask("  Name: ");
            if (product.name.empty()) continue;
            if (catalog_.findByName(product.name)) {
                std::cout << "  A product with that name already exists.\n";
                continue;
            }
            const auto price = askMoney("  Price: ");
            if (!price) continue;
            product.price = *price;
            const auto& saved = food_->saveProduct(std::move(product));
            std::cout << "  Added " << saved.name << " at " << saved.price.toString() << ".\n";
        } else if (choice == "2" || choice == "3") {
            const int index = askInt("  Product number: ", 0);
            if (index < 1 || index > static_cast<int>(products.size())) continue;
            const Product chosen = *products[static_cast<std::size_t>(index - 1)];
            if (choice == "3") {
                if (askYesNo("  Delete " + chosen.name + "? (y/n) [n]: ", false)) {
                    food_->removeProduct(chosen.id);
                    std::cout << "  Deleted.\n";
                }
                continue;
            }
            Product edited = chosen;
            edited.name = ask("  Name [" + chosen.name + "]: ", chosen.name);
            edited.price = askMoney("  Price [" + chosen.price.toString() + "]: ", chosen.price)
                               .value_or(chosen.price);
            food_->saveProduct(std::move(edited));
            std::cout << "  Saved.\n";
        } else if (choice == "0") {
            return;
        }
    }
}

void ConsoleApp::manageSlots() {
    while (!inputClosed()) {
        std::cout << "\n  --- Quick slots ---\n";
        for (int slot = 1; slot <= domain::ProductCatalog::kSlotCount; ++slot) {
            const Product* product = catalog_.slot(slot);
            std::cout << "    s" << slot << ") " << (product ? product->name : "(empty)") << '\n';
        }
        const int slot = askInt("  Slot to change (1-6, 0 = back): ", 0);
        if (slot < 1 || slot > domain::ProductCatalog::kSlotCount) return;

        const auto products = catalog_.all();
        printProducts(products);
        const int index = askInt("  Product number (0 = empty the slot): ", 0);
        if (index < 0 || index > static_cast<int>(products.size())) continue;
        food_->setSlot(slot, index == 0 ? 0 : products[static_cast<std::size_t>(index - 1)]->id);
    }
}

} // namespace pokersoc::ui
