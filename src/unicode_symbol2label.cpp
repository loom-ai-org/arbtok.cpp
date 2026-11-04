#include "arbtok/unicode_symbol2label.hpp"

#include <unordered_map>

#include "detail.hpp"
#include "json.hpp"

namespace arbtok::unicode_symbol2label {

namespace {

struct Table {
    std::vector<std::pair<std::string, std::string>> items;
    std::unordered_map<std::string, size_t> index;
};

const Table& table() {
    static const Table t = [] {
        Table out;
        const json::Value root = json::loads(detail::read_file(detail::data_path("unicode_symbol2label.json")));
        if (root.kind != json::Value::Object) throw ValueError("unicode_symbol2label.json is not an object");
        for (const auto& [key, value] : root.members) {
            if (value.kind != json::Value::String) throw ValueError("unicode_symbol2label.json: a label is not a string");
            out.index.emplace(key, out.items.size());
            out.items.emplace_back(key, value.text);
        }
        return out;
    }();
    return t;
}

}  // namespace

const std::vector<std::pair<std::string, std::string>>& items() { return table().items; }

std::optional<std::string> get(const std::string& symbol) {
    const auto& t = table();
    auto it = t.index.find(symbol);
    if (it == t.index.end()) return std::nullopt;
    return t.items[it->second].second;
}

}  // namespace arbtok::unicode_symbol2label
