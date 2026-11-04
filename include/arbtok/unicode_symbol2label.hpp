// arbtok.unicode_symbol2label: the Arabic spoken label of a symbol or emoji.
// The table is data/unicode_symbol2label.json, in the order of the Python dict.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arbtok::unicode_symbol2label {

// UNICODE_SYMBOL_TO_LABEL.items(), in dict order.
const std::vector<std::pair<std::string, std::string>>& items();

// UNICODE_SYMBOL_TO_LABEL.get(symbol).
std::optional<std::string> get(const std::string& symbol);

}  // namespace arbtok::unicode_symbol2label
