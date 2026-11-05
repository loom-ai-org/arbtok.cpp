#include "json.hpp"

#include <cstdio>
#include <stdexcept>

namespace arbtok::numbers::json {

namespace {

class Parser {
public:
    explicit Parser(std::string_view text) : s_(text) {}

    Value document() {
        Value v = value();
        space();
        if (i_ != s_.size()) fail("trailing text");
        return v;
    }

private:
    std::string_view s_;
    size_t i_ = 0;

    [[noreturn]] void fail(const char* what) const {
        throw std::runtime_error(std::string("json: ") + what + " at " + std::to_string(i_));
    }
    void space() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
    }
    bool literal(std::string_view word) {
        if (s_.substr(i_, word.size()) != word) return false;
        i_ += word.size();
        return true;
    }
    static void append_utf8(std::string& out, unsigned cp) {
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
    unsigned hex4() {
        if (i_ + 4 > s_.size()) fail("short escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad escape");
        }
        return v;
    }
    std::string string() {
        ++i_;  // the opening quote
        std::string out;
        while (true) {
            if (i_ >= s_.size()) fail("open string");
            const char c = s_[i_++];
            if (c == '"') return out;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i_ >= s_.size()) fail("open escape");
            const char e = s_[i_++];
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
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp < 0xDC00 && s_.substr(i_, 2) == "\\u") {
                        i_ += 2;
                        const unsigned lo = hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("bad escape");
            }
        }
    }
    Value value() {
        space();
        if (i_ >= s_.size()) fail("no value");
        Value v;
        const char c = s_[i_];
        if (c == '{') {
            v.kind = Value::Kind::Object;
            ++i_;
            space();
            if (i_ < s_.size() && s_[i_] == '}') {
                ++i_;
                return v;
            }
            while (true) {
                space();
                if (i_ >= s_.size() || s_[i_] != '"') fail("no key");
                std::string key = string();
                space();
                if (i_ >= s_.size() || s_[i_++] != ':') fail("no colon");
                v.members.emplace_back(std::move(key), value());
                space();
                if (i_ < s_.size() && s_[i_] == ',') {
                    ++i_;
                    continue;
                }
                if (i_ < s_.size() && s_[i_] == '}') {
                    ++i_;
                    return v;
                }
                fail("bad object");
            }
        }
        if (c == '[') {
            v.kind = Value::Kind::Array;
            ++i_;
            space();
            if (i_ < s_.size() && s_[i_] == ']') {
                ++i_;
                return v;
            }
            while (true) {
                v.items.push_back(value());
                space();
                if (i_ < s_.size() && s_[i_] == ',') {
                    ++i_;
                    continue;
                }
                if (i_ < s_.size() && s_[i_] == ']') {
                    ++i_;
                    return v;
                }
                fail("bad array");
            }
        }
        if (c == '"') {
            v.kind = Value::Kind::String;
            v.str = string();
            return v;
        }
        if (literal("null")) return v;
        if (literal("true")) {
            v.kind = Value::Kind::Bool;
            v.boolean = true;
            return v;
        }
        if (literal("false")) {
            v.kind = Value::Kind::Bool;
            return v;
        }
        const size_t start = i_;
        while (i_ < s_.size() && (std::string_view("+-.eE0123456789").find(s_[i_]) != std::string_view::npos)) ++i_;
        if (i_ == start) fail("bad value");
        v.kind = Value::Kind::Number;
        v.str = std::string(s_.substr(start, i_ - start));
        return v;
    }
};

}  // namespace

const Value& Value::at(std::string_view key) const {
    for (const auto& [k, v] : members) {
        if (k == key) return v;
    }
    throw std::out_of_range("json: no member " + std::string(key));
}

Value parse(std::string_view text) { return Parser(text).document(); }

std::string quote(std::string_view s) {
    std::string out = "\"";
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(ch);
                }
        }
    }
    return out + "\"";
}

std::string dump(const Value& v) {
    switch (v.kind) {
        case Value::Kind::Null: return "null";
        case Value::Kind::Bool: return v.boolean ? "true" : "false";
        case Value::Kind::Number: return v.str;
        case Value::Kind::String: return quote(v.str);
        case Value::Kind::Array: {
            std::string out = "[";
            for (size_t i = 0; i < v.items.size(); ++i) out += (i ? "," : "") + dump(v.items[i]);
            return out + "]";
        }
        case Value::Kind::Object: {
            std::string out = "{";
            for (size_t i = 0; i < v.members.size(); ++i) {
                out += (i ? "," : "") + quote(v.members[i].first) + ":" + dump(v.members[i].second);
            }
            return out + "}";
        }
    }
    return "null";
}

}  // namespace arbtok::numbers::json
