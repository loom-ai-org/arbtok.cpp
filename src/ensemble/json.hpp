// A JSON reader for the vocabulary and the golden files. An object keeps the
// order in which each key first appears and the last value given for it, as
// json.loads builds a dict.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arbtok::ensemble::detail::json {

struct Value {
    enum class Type { Null, Bool, Int, Float, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    std::int64_t i = 0;
    double f = 0;
    std::string s;
    std::vector<Value> items;
    std::vector<std::pair<std::string, Value>> members;

    bool is_null() const { return type == Type::Null; }

    const Value* find(std::string_view key) const {
        for (const auto& m : members)
            if (m.first == key) return &m.second;
        return nullptr;
    }

    const Value& at(std::string_view key) const {
        const Value* v = find(key);
        if (!v) throw std::runtime_error("JSON key not found: " + std::string(key));
        return *v;
    }
};

class Parser {
public:
    explicit Parser(std::string_view text) : t_(text) {}

    Value parse() {
        Value v = value();
        ws();
        if (p_ != t_.size()) fail("trailing data");
        return v;
    }

private:
    std::string_view t_;
    std::size_t p_ = 0;

    [[noreturn]] void fail(const char* what) const {
        throw std::runtime_error(std::string("JSON parse error: ") + what + " at offset " + std::to_string(p_));
    }

    void ws() {
        while (p_ < t_.size() && (t_[p_] == ' ' || t_[p_] == '\t' || t_[p_] == '\n' || t_[p_] == '\r')) ++p_;
    }

    bool consume(std::string_view lit) {
        if (t_.substr(p_, lit.size()) == lit) {
            p_ += lit.size();
            return true;
        }
        return false;
    }

    Value value() {
        ws();
        if (p_ >= t_.size()) fail("unexpected end");
        Value v;
        const char c = t_[p_];
        if (c == '{') {
            v.type = Value::Type::Object;
            ++p_;
            ws();
            if (p_ < t_.size() && t_[p_] == '}') {
                ++p_;
                return v;
            }
            std::map<std::string, std::size_t> index;
            for (;;) {
                ws();
                if (p_ >= t_.size() || t_[p_] != '"') fail("expected key");
                std::string key = string();
                ws();
                if (p_ >= t_.size() || t_[p_] != ':') fail("expected ':'");
                ++p_;
                Value member = value();
                auto it = index.find(key);
                if (it == index.end()) {
                    index.emplace(key, v.members.size());
                    v.members.emplace_back(std::move(key), std::move(member));
                } else {
                    v.members[it->second].second = std::move(member);
                }
                ws();
                if (p_ < t_.size() && t_[p_] == ',') {
                    ++p_;
                    continue;
                }
                if (p_ < t_.size() && t_[p_] == '}') {
                    ++p_;
                    return v;
                }
                fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            v.type = Value::Type::Array;
            ++p_;
            ws();
            if (p_ < t_.size() && t_[p_] == ']') {
                ++p_;
                return v;
            }
            for (;;) {
                v.items.push_back(value());
                ws();
                if (p_ < t_.size() && t_[p_] == ',') {
                    ++p_;
                    continue;
                }
                if (p_ < t_.size() && t_[p_] == ']') {
                    ++p_;
                    return v;
                }
                fail("expected ',' or ']'");
            }
        }
        if (c == '"') {
            v.type = Value::Type::String;
            v.s = string();
            return v;
        }
        if (consume("null")) return v;
        if (consume("true")) {
            v.type = Value::Type::Bool;
            v.b = true;
            return v;
        }
        if (consume("false")) {
            v.type = Value::Type::Bool;
            return v;
        }
        return number();
    }

    Value number() {
        const std::size_t start = p_;
        bool is_float = false;
        if (p_ < t_.size() && t_[p_] == '-') ++p_;
        while (p_ < t_.size()) {
            const char c = t_[p_];
            if (c >= '0' && c <= '9') {
                ++p_;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                is_float = true;
                ++p_;
            } else {
                break;
            }
        }
        if (p_ == start) fail("unexpected character");
        const std::string num(t_.substr(start, p_ - start));
        Value v;
        if (is_float) {
            v.type = Value::Type::Float;
            v.f = std::stod(num);
        } else {
            v.type = Value::Type::Int;
            v.i = std::stoll(num);
        }
        return v;
    }

    unsigned hex4() {
        if (p_ + 4 > t_.size()) fail("short \\u escape");
        unsigned u = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = t_[p_++];
            u <<= 4;
            if (c >= '0' && c <= '9') u |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') u |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') u |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad \\u escape");
        }
        return u;
    }

    static void put_utf8(std::string& out, char32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::string string() {
        ++p_;
        std::string out;
        for (;;) {
            if (p_ >= t_.size()) fail("unterminated string");
            const char c = t_[p_++];
            if (c == '"') return out;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (p_ >= t_.size()) fail("unterminated escape");
            const char e = t_[p_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    char32_t cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (!consume("\\u")) fail("lone surrogate");
                        const unsigned lo = hex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) fail("lone surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        fail("lone surrogate");
                    }
                    put_utf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
    }
};

inline Value parse(std::string_view text) { return Parser(text).parse(); }

}  // namespace arbtok::ensemble::detail::json
