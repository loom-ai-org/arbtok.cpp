// A small JSON reader that keeps the order of object members, and the string
// quoting of Python's json.dumps(value, ensure_ascii=False).
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arbtok::numbers::json {

struct Value {
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    bool boolean = false;
    std::string str;  // the text of a number, or the UTF-8 of a string
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;

    bool is_null() const { return kind == Kind::Null; }
    size_t size() const { return items.size(); }
    const Value& operator[](size_t i) const { return items.at(i); }
    // The member named key; std::out_of_range when there is none.
    const Value& at(std::string_view key) const;
    long long integer() const { return std::stoll(str); }
};

// The JSON text; std::runtime_error on malformed input.
Value parse(std::string_view text);

// The JSON string literal of a UTF-8 string, as json.dumps writes it with
// ensure_ascii=False.
std::string quote(std::string_view s);

// The JSON text of a value, with the separators "," and ":".
std::string dump(const Value& v);

}  // namespace arbtok::numbers::json
