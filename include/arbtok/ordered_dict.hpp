// A Python dict with str keys: lookup by key, iteration in insertion order.
#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arbtok {

template <class V>
class OrderedDict {
public:
    using Item = std::pair<std::string, V>;

    // dict.get(key): nullptr when the key is absent.
    const V* get(const std::string& key) const {
        const auto it = index_.find(key);
        return it == index_.end() ? nullptr : &items_[it->second].second;
    }
    bool contains(const std::string& key) const { return index_.count(key) != 0; }
    // dict.setdefault(key, value): the first value for a key wins.
    void setdefault(const std::string& key, V value) {
        if (index_.emplace(key, items_.size()).second) items_.emplace_back(key, std::move(value));
    }
    // d[key] = value: a key keeps its first position.
    void set(const std::string& key, V value) {
        const auto [it, added] = index_.emplace(key, items_.size());
        if (added) items_.emplace_back(key, std::move(value));
        else items_[it->second].second = std::move(value);
    }
    // dict.update(other).
    void update(const OrderedDict& other) {
        for (const auto& [k, v] : other.items_) set(k, v);
    }
    const std::vector<Item>& items() const { return items_; }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

private:
    std::vector<Item> items_;
    std::unordered_map<std::string, size_t> index_;
};

}  // namespace arbtok
