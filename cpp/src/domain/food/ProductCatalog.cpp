#include "domain/food/ProductCatalog.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace pokersoc::domain {

namespace {

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

const Product& ProductCatalog::upsert(Product product) {
    const long long id = product.id;
    auto& stored = products_[id];
    stored = std::move(product);
    return stored;
}

bool ProductCatalog::remove(long long id) {
    if (products_.erase(id) == 0) {
        return false;
    }
    for (auto& slot : slots_) {
        if (slot == id) {
            slot = 0;
        }
    }
    return true;
}

void ProductCatalog::clear() {
    products_.clear();
    slots_.fill(0);
}

const Product* ProductCatalog::find(long long id) const {
    const auto it = products_.find(id);
    return it == products_.end() ? nullptr : &it->second;
}

const Product* ProductCatalog::findByName(const std::string& name) const {
    for (const auto& [id, product] : products_) {
        if (equalsIgnoreCase(product.name, name)) {
            return &product;
        }
    }
    return nullptr;
}

std::vector<const Product*> ProductCatalog::all() const {
    std::vector<const Product*> result;
    result.reserve(products_.size());
    for (const auto& [id, product] : products_) {
        result.push_back(&product);
    }
    std::sort(result.begin(), result.end(), [](const Product* a, const Product* b) {
        return lower(a->name) < lower(b->name);
    });
    return result;
}

void ProductCatalog::setSlot(int slot, long long productId) {
    if (slot < 1 || slot > kSlotCount) {
        return;
    }
    slots_[static_cast<std::size_t>(slot - 1)] = productId;
}

const Product* ProductCatalog::slot(int slot) const {
    const long long id = slotProductId(slot);
    return id == 0 ? nullptr : find(id);
}

long long ProductCatalog::slotProductId(int slot) const {
    if (slot < 1 || slot > kSlotCount) {
        return 0;
    }
    return slots_[static_cast<std::size_t>(slot - 1)];
}

} // namespace pokersoc::domain
