#include "pyre.hpp"

#include <algorithm>
#include <map>
#include <mutex>

namespace arbtok::pyre {

namespace {
#include "pyre_tables.inc"

template <size_t N>
bool in_ranges(const char32_t (&table)[N][2], char32_t c) {
    size_t lo = 0, hi = N;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (c < table[mid][0]) {
            hi = mid;
        } else if (c > table[mid][1]) {
            lo = mid + 1;
        } else {
            return true;
        }
    }
    return false;
}

const ExtraCase* extra_case(char32_t lo) {
    const auto* end = std::end(kExtraCases);
    const auto* it = std::lower_bound(std::begin(kExtraCases), end, lo,
                                      [](const ExtraCase& e, char32_t v) { return e.lo < v; });
    return (it != end && it->lo == lo) ? it : nullptr;
}

bool is_cased(char32_t c) { return in_ranges(kIsCased, c); }

}  // namespace

char32_t sre_lower(char32_t c) {
    const auto* end = std::end(kSimpleLower);
    const auto* it = std::lower_bound(std::begin(kSimpleLower), end, c,
                                      [](const char32_t (&e)[2], char32_t v) { return e[0] < v; });
    return (it != end && (*it)[0] == c) ? (*it)[1] : c;
}

bool is_decimal(char32_t c) { return in_ranges(kDecimal, c); }
bool is_word(char32_t c) { return c == U'_' || detail::is_alnum(c); }

namespace {

using Ranges = std::vector<std::pair<char32_t, char32_t>>;

Ranges merged(Ranges r) {
    std::sort(r.begin(), r.end());
    Ranges out;
    for (const auto& [lo, hi] : r) {
        if (!out.empty() && lo <= out.back().second + 1) {
            out.back().second = std::max(out.back().second, hi);
        } else {
            out.emplace_back(lo, hi);
        }
    }
    return out;
}

bool in(const Ranges& r, char32_t c) {
    auto it = std::upper_bound(r.begin(), r.end(), c,
                               [](char32_t v, const std::pair<char32_t, char32_t>& e) { return v < e.first; });
    if (it == r.begin()) return false;
    --it;
    return c <= it->second;
}

bool category(char cat, char32_t c) {
    switch (cat) {
        case 'd': return is_decimal(c);
        case 'D': return !is_decimal(c);
        case 'w': return is_word(c);
        case 'W': return !is_word(c);
        case 's': return detail::is_space(c);
        case 'S': return !detail::is_space(c);
        default: return false;
    }
}

struct CharClass {
    Ranges ranges;
    std::string cats;
    bool negate = false;
    // Under IGNORECASE with a cased member: the members are lowercased, and a
    // character is tested by its lowercase.
    bool lowered = false;

    bool contains(char32_t c) const {
        if (lowered) c = sre_lower(c);
        bool hit = in(ranges, c);
        for (size_t i = 0; !hit && i < cats.size(); ++i) hit = category(cats[i], c);
        return hit != negate;
    }
};

struct Node {
    enum Kind : unsigned char {
        Lit, LitIgnore, Any, AnyAll, Set, Bol, Eol, BolM, EolM, StrStart, StrEnd, WordB, NotWordB,
        Group, Alt, Repeat, Look, Backref, Cond
    } kind = Lit;
    char32_t ch = 0;
    std::vector<char32_t> lits;  // LitIgnore: the lowercase forms that match
    std::shared_ptr<CharClass> cls;
    int group = 0;
    bool icase = false;
    std::vector<std::vector<Node>> alts;
    long min = 0, max = -1;  // Repeat; max -1 is unbounded
    bool greedy = true;
    bool behind = false, negate = false;
    long width = 0;
};

using Seq = std::vector<Node>;

bool char_node(const Node& n) {
    return n.kind == Node::Lit || n.kind == Node::LitIgnore || n.kind == Node::Any || n.kind == Node::AnyAll ||
           n.kind == Node::Set;
}

bool test1(const Node& n, char32_t c) {
    switch (n.kind) {
        case Node::Lit: return c == n.ch;
        case Node::LitIgnore: {
            const char32_t lo = sre_lower(c);
            return std::find(n.lits.begin(), n.lits.end(), lo) != n.lits.end();
        }
        case Node::Any: return c != U'\n';
        case Node::AnyAll: return true;
        case Node::Set: return n.cls->contains(c);
        default: return false;
    }
}

}  // namespace

struct Program {
    U32 pattern;
    int flags = 0;
    int groups = 0;
    std::map<std::string, int> names;
    std::vector<std::string> group_names;  // index -> name or ""
    std::vector<Seq> top;                  // the top-level alternatives
    const Node* first = nullptr;           // a character every match starts with, when known
};

namespace {

// ── Parser ──────────────────────────────────────────────────────────────────

class Parser {
public:
    Parser(U32View p, int flags, Program& prog) : p_(p), prog_(prog), flags_(flags) {}

    void parse() {
        global_flags();
        prog_.top = alternatives(flags_);
        if (i_ < p_.size()) fail(p_[i_] == U')' ? "unbalanced parenthesis" : "unexpected character");
        prog_.flags = flags_;
    }

private:
    [[noreturn]] void fail(const std::string& why) const {
        throw error(why + " at position " + std::to_string(i_) + " in " + detail::to_utf8(p_));
    }

    bool more() const { return i_ < p_.size(); }
    char32_t peek(size_t k = 0) const { return i_ + k < p_.size() ? p_[i_ + k] : 0; }

    static int flag_of(char32_t c) {
        switch (c) {
            case U'i': return IGNORECASE;
            case U'm': return MULTILINE;
            case U's': return DOTALL;
            case U'u': return 0;
            default: return -1;
        }
    }

    // Global flags at the start: (?i), (?s), (?is).
    void global_flags() {
        while (peek() == U'(' && peek(1) == U'?' && flag_of(peek(2)) >= 0) {
            size_t j = i_ + 2;
            int f = 0;
            while (j < p_.size() && flag_of(p_[j]) >= 0) f |= flag_of(p_[j++]);
            if (j >= p_.size() || p_[j] != U')') return;  // a scoped group, parsed later
            flags_ |= f;
            i_ = j + 1;
        }
    }

    std::vector<Seq> alternatives(int flags) {
        std::vector<Seq> alts;
        alts.push_back(sequence(flags));
        while (peek() == U'|') {
            ++i_;
            alts.push_back(sequence(flags));
        }
        return alts;
    }

    Seq sequence(int flags) {
        Seq seq;
        while (more() && peek() != U'|' && peek() != U')') {
            Node atom;
            const bool repeatable = parse_atom(flags, atom);
            if (repeatable && more() && is_quantifier()) {
                Node rep;
                rep.kind = Node::Repeat;
                parse_quantifier(rep);
                rep.alts.push_back(Seq{std::move(atom)});
                seq.push_back(std::move(rep));
            } else {
                if (!repeatable && more() && is_quantifier()) fail("nothing to repeat");
                seq.push_back(std::move(atom));
            }
        }
        return seq;
    }

    bool is_quantifier() const {
        const char32_t c = peek();
        if (c == U'*' || c == U'+' || c == U'?') return true;
        if (c != U'{') return false;
        size_t j = i_ + 1;
        if (j < p_.size() && p_[j] == U'}') return false;
        while (j < p_.size() && p_[j] >= U'0' && p_[j] <= U'9') ++j;
        if (j < p_.size() && p_[j] == U',') {
            ++j;
            while (j < p_.size() && p_[j] >= U'0' && p_[j] <= U'9') ++j;
        }
        return j < p_.size() && p_[j] == U'}';
    }

    void parse_quantifier(Node& rep) {
        const char32_t c = p_[i_++];
        if (c == U'*') {
            rep.min = 0, rep.max = -1;
        } else if (c == U'+') {
            rep.min = 1, rep.max = -1;
        } else if (c == U'?') {
            rep.min = 0, rep.max = 1;
        } else {
            std::string lo, hi;
            while (peek() >= U'0' && peek() <= U'9') lo += static_cast<char>(p_[i_++]);
            bool comma = false;
            if (peek() == U',') {
                comma = true;
                ++i_;
                while (peek() >= U'0' && peek() <= U'9') hi += static_cast<char>(p_[i_++]);
            } else {
                hi = lo;
            }
            ++i_;  // '}'
            rep.min = lo.empty() ? 0 : std::stol(lo);
            rep.max = (comma && hi.empty()) ? -1 : std::stol(hi);
            if (rep.max >= 0 && rep.max < rep.min) fail("min repeat greater than max repeat");
        }
        if (peek() == U'?') {
            rep.greedy = false;
            ++i_;
        } else if (peek() == U'+') {
            fail("possessive quantifiers are not supported");
        }
    }

    Node literal(char32_t c, int flags) {
        Node n;
        if ((flags & IGNORECASE) && is_cased(c)) {
            n.kind = Node::LitIgnore;
            const char32_t lo = sre_lower(c);
            n.lits.push_back(lo);
            if (const auto* e = extra_case(lo))
                for (unsigned k = 0; k < e->n; ++k) n.lits.push_back(e->others[k]);
        } else {
            n.kind = Node::Lit;
            n.ch = c;
        }
        return n;
    }

    unsigned hex(size_t digits) {
        unsigned v = 0;
        for (size_t k = 0; k < digits; ++k) {
            const char32_t c = peek();
            int d;
            if (c >= U'0' && c <= U'9') {
                d = static_cast<int>(c - U'0');
            } else if (c >= U'a' && c <= U'f') {
                d = static_cast<int>(c - U'a') + 10;
            } else if (c >= U'A' && c <= U'F') {
                d = static_cast<int>(c - U'A') + 10;
            } else {
                fail("incomplete escape");
            }
            v = v * 16 + static_cast<unsigned>(d);
            ++i_;
        }
        if (v > 0x10FFFF) fail("bad escape");
        return v;
    }

    // An escape that stands for one code point, after the backslash; -1 when
    // the escape is not one.
    long char_escape(char32_t c) {
        switch (c) {
            case U'a': return 7;
            case U'f': return 12;
            case U'n': return 10;
            case U'r': return 13;
            case U't': return 9;
            case U'v': return 11;
            case U'\\': return U'\\';
            case U'x': return hex(2);
            case U'u': return hex(4);
            case U'U': return hex(8);
            default: return -1;
        }
    }

    static bool ascii_letter(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }

    // Returns whether the atom can take a quantifier.
    bool parse_atom(int flags, Node& out) {
        const char32_t c = p_[i_++];
        switch (c) {
            case U'.':
                out.kind = (flags & DOTALL) ? Node::AnyAll : Node::Any;
                return true;
            case U'^':
                out.kind = (flags & MULTILINE) ? Node::BolM : Node::Bol;
                return false;
            case U'$':
                out.kind = (flags & MULTILINE) ? Node::EolM : Node::Eol;
                return false;
            case U'[':
                parse_class(flags, out);
                return true;
            case U'(':
                return parse_group(flags, out);
            case U'\\':
                return parse_escape(flags, out);
            case U'*':
            case U'+':
            case U'?':
                fail("nothing to repeat");
            case U'{':
                if (peek() == U'}') {
                    out = literal(c, flags);
                    return true;
                }
                out = literal(c, flags);
                return true;
            default:
                out = literal(c, flags);
                return true;
        }
    }

    bool parse_escape(int flags, Node& out) {
        if (!more()) fail("bad escape (end of pattern)");
        const char32_t c = p_[i_++];
        switch (c) {
            case U'A': out.kind = Node::StrStart; return false;
            case U'Z': out.kind = Node::StrEnd; return false;
            case U'b': out.kind = Node::WordB; return false;
            case U'B': out.kind = Node::NotWordB; return false;
            case U'd': case U'D': case U'w': case U'W': case U's': case U'S': {
                out.kind = Node::Set;
                out.cls = std::make_shared<CharClass>();
                out.cls->cats.push_back(static_cast<char>(c));
                return true;
            }
            default: break;
        }
        if (c >= U'1' && c <= U'9') {
            std::string digits(1, static_cast<char>(c));
            if (peek() >= U'0' && peek() <= U'9') digits += static_cast<char>(p_[i_++]);
            const int g = std::stoi(digits);
            if (g > prog_.groups) fail("invalid group reference " + digits);
            out.kind = Node::Backref;
            out.group = g;
            out.icase = flags & IGNORECASE;
            return true;
        }
        if (c == U'0') {
            unsigned v = 0;
            for (int k = 0; k < 2 && peek() >= U'0' && peek() <= U'7'; ++k) v = v * 8 + (p_[i_++] - U'0');
            out = literal(v, flags);
            return true;
        }
        const long v = char_escape(c);
        if (v >= 0) {
            out = literal(static_cast<char32_t>(v), flags);
            return true;
        }
        if (ascii_letter(c) || (c >= U'0' && c <= U'9')) fail("bad escape");
        out = literal(c, flags);
        return true;
    }

    // One class member after '[': a code point, or a category in *cat.
    char32_t class_item(char* cat) {
        *cat = 0;
        const char32_t c = p_[i_++];
        if (c != U'\\') return c;
        if (!more()) fail("bad escape (end of pattern)");
        const char32_t e = p_[i_++];
        switch (e) {
            case U'd': case U'D': case U'w': case U'W': case U's': case U'S':
                *cat = static_cast<char>(e);
                return 0;
            case U'b': return 8;
            default: break;
        }
        if (e >= U'0' && e <= U'7') {
            unsigned v = e - U'0';
            for (int k = 0; k < 2 && peek() >= U'0' && peek() <= U'7'; ++k) v = v * 8 + (p_[i_++] - U'0');
            return v;
        }
        const long v = char_escape(e);
        if (v >= 0) return static_cast<char32_t>(v);
        if (ascii_letter(e) || e == U'8' || e == U'9') fail("bad escape");
        return e;
    }

    void parse_class(int flags, Node& out) {
        auto cls = std::make_shared<CharClass>();
        if (peek() == U'^') {
            cls->negate = true;
            ++i_;
        }
        Ranges ranges;
        bool first = true;
        while (true) {
            if (!more()) fail("unterminated character set");
            if (peek() == U']' && !first) {
                ++i_;
                break;
            }
            first = false;
            char cat;
            const char32_t lo = class_item(&cat);
            if (cat) {
                if (peek() == U'-' && peek(1) != U']' && peek(1) != 0) fail("bad character range");
                cls->cats.push_back(cat);
                continue;
            }
            if (peek() == U'-' && peek(1) != U']' && peek(1) != 0) {
                ++i_;
                char cat2;
                const char32_t hi = class_item(&cat2);
                if (cat2 || hi < lo) fail("bad character range");
                ranges.emplace_back(lo, hi);
            } else {
                ranges.emplace_back(lo, lo);
            }
        }
        if (flags & IGNORECASE) {
            bool cased = false;
            for (const auto& [lo, hi] : ranges) {
                for (char32_t c = lo; c <= hi && !cased; ++c) cased = is_cased(c);
                if (cased) break;
            }
            if (cased) {
                Ranges lowered;
                for (const auto& [lo, hi] : ranges) {
                    for (char32_t c = lo; c <= hi; ++c) {
                        const char32_t l = sre_lower(c);
                        lowered.emplace_back(l, l);
                        if (const auto* e = extra_case(l))
                            for (unsigned k = 0; k < e->n; ++k) lowered.emplace_back(e->others[k], e->others[k]);
                    }
                }
                ranges = std::move(lowered);
                cls->lowered = true;
            }
        }
        cls->ranges = merged(std::move(ranges));
        out.kind = Node::Set;
        out.cls = std::move(cls);
    }

    std::string name_until(char32_t close) {
        std::string name;
        while (more() && peek() != close) name += detail::to_utf8(U32(1, p_[i_++]));
        if (!more()) fail("missing " + detail::to_utf8(U32(1, close)));
        ++i_;
        if (name.empty()) fail("missing group name");
        return name;
    }

    int group_ref(const std::string& name) {
        if (!name.empty() && std::all_of(name.begin(), name.end(), [](char ch) { return ch >= '0' && ch <= '9'; }))
            return std::stoi(name);
        const auto it = prog_.names.find(name);
        if (it == prog_.names.end()) fail("unknown group name '" + name + "'");
        return it->second;
    }

    int new_group(const std::string& name) {
        const int g = ++prog_.groups;
        prog_.group_names.resize(g + 1);
        prog_.group_names[g] = name;
        if (!name.empty()) {
            if (prog_.names.count(name)) fail("redefinition of group name '" + name + "'");
            prog_.names[name] = g;
        }
        return g;
    }

    void close_paren() {
        if (peek() != U')') fail("missing ), unterminated subpattern");
        ++i_;
    }

    bool parse_group(int flags, Node& out) {
        if (peek() != U'?') {
            out.kind = Node::Group;
            out.group = new_group("");
            out.alts = alternatives(flags);
            close_paren();
            return true;
        }
        ++i_;
        const char32_t c = more() ? p_[i_++] : 0;
        if (c == U':') {
            out.kind = Node::Alt;
            out.alts = alternatives(flags);
            close_paren();
            return true;
        }
        if (c == U'P') {
            const char32_t d = more() ? p_[i_++] : 0;
            if (d == U'<') {
                const std::string name = name_until(U'>');
                out.kind = Node::Group;
                out.group = new_group(name);
                out.alts = alternatives(flags);
                close_paren();
                return true;
            }
            if (d == U'=') {
                const std::string name = name_until(U')');
                out.kind = Node::Backref;
                out.group = group_ref(name);
                out.icase = flags & IGNORECASE;
                return true;
            }
            fail("unknown extension ?P");
        }
        if (c == U'=' || c == U'!') {
            out.kind = Node::Look;
            out.negate = c == U'!';
            out.alts = alternatives(flags);
            close_paren();
            return false;
        }
        if (c == U'<' && (peek() == U'=' || peek() == U'!')) {
            out.kind = Node::Look;
            out.behind = true;
            out.negate = p_[i_++] == U'!';
            out.alts = alternatives(flags);
            close_paren();
            long lo = 0, hi = 0;
            widths(out.alts, lo, hi);
            if (lo != hi) fail("look-behind requires fixed-width pattern");
            out.width = lo;
            return false;
        }
        if (c == U'(') {
            const std::string name = name_until(U')');
            out.kind = Node::Cond;
            out.group = group_ref(name);
            Seq yes = sequence(flags), no;
            if (peek() == U'|') {
                ++i_;
                no = sequence(flags);
                if (peek() == U'|') fail("conditional backref with more than two branches");
            }
            close_paren();
            out.alts.push_back(std::move(yes));
            out.alts.push_back(std::move(no));
            return true;
        }
        if (c == U'#') {
            while (more() && peek() != U')') ++i_;
            close_paren();
            out.kind = Node::Alt;
            out.alts.emplace_back();
            return false;
        }
        // Scoped flags: (?i:...), (?-i:...), (?i-s:...).
        --i_;
        int on = 0, off = 0;
        while (flag_of(peek()) >= 0) on |= flag_of(p_[i_++]);
        if (peek() == U'-') {
            ++i_;
            while (flag_of(peek()) >= 0) off |= flag_of(p_[i_++]);
        }
        if (peek() != U':') fail("unknown extension or flags in the middle of the pattern");
        ++i_;
        out.kind = Node::Alt;
        out.alts = alternatives((flags | on) & ~off);
        close_paren();
        return true;
    }

    static void widths(const std::vector<Seq>& alts, long& lo, long& hi) {
        bool first = true;
        for (const auto& seq : alts) {
            long a = 0, b = 0;
            for (const auto& n : seq) {
                long x = 0, y = 0;
                node_width(n, x, y);
                a += x;
                b = (b < 0 || y < 0) ? -1 : b + y;
            }
            if (first) {
                lo = a, hi = b, first = false;
            } else {
                lo = std::min(lo, a);
                hi = (hi < 0 || b < 0) ? -1 : std::max(hi, b);
            }
        }
    }

    static void node_width(const Node& n, long& lo, long& hi) {
        if (char_node(n)) {
            lo = hi = 1;
            return;
        }
        switch (n.kind) {
            case Node::Group:
            case Node::Alt:
                widths(n.alts, lo, hi);
                return;
            case Node::Cond: {
                widths(n.alts, lo, hi);
                return;
            }
            case Node::Repeat: {
                long a = 0, b = 0;
                widths(n.alts, a, b);
                lo = a * n.min;
                hi = (n.max < 0 || b < 0) ? -1 : b * n.max;
                return;
            }
            case Node::Backref:
                lo = 0, hi = -1;
                return;
            default:
                lo = hi = 0;
                return;
        }
    }

    U32View p_;
    Program& prog_;
    int flags_;
    size_t i_ = 0;
};

const Node* first_char(const std::vector<Seq>& alts) {
    if (alts.size() != 1) return nullptr;
    for (const auto& n : alts[0]) {
        if (n.kind == Node::Look || n.kind == Node::WordB || n.kind == Node::NotWordB) continue;
        if (n.kind == Node::Lit || n.kind == Node::LitIgnore || n.kind == Node::Set) return &n;
        if (n.kind == Node::Group || (n.kind == Node::Alt && n.alts.size() == 1)) return first_char(n.alts);
        if (n.kind == Node::Repeat && n.min >= 1) return first_char(n.alts);
        return nullptr;
    }
    return nullptr;
}

// ── Matcher ─────────────────────────────────────────────────────────────────

struct Cont {
    enum Type : unsigned char { Done, Next, Close, Rep, LookDone } type = Done;
    const Seq* seq = nullptr;
    size_t idx = 0;
    int group = 0;
    size_t open = 0;
    const Node* rep = nullptr;
    long count = 0;
    size_t last = 0;
    const Cont* next = nullptr;
};

struct Matcher {
    U32View s;
    size_t end = 0;
    size_t start = 0;
    bool full = false;
    bool must_advance = false;
    std::vector<long> marks;
    int lastindex = -1;
    size_t match_end = 0;

    bool word_at(size_t pos) const { return pos < end && is_word(s[pos]); }
    bool word_before(size_t pos) const { return pos > 0 && is_word(s[pos - 1]); }

    bool zero_width(const Node& n, size_t pos) const {
        switch (n.kind) {
            case Node::Bol: return pos == 0;
            case Node::BolM: return pos == 0 || s[pos - 1] == U'\n';
            case Node::Eol: return pos == end || (pos + 1 == end && s[pos] == U'\n');
            case Node::EolM: return pos == end || s[pos] == U'\n';
            case Node::StrStart: return pos == 0;
            case Node::StrEnd: return pos == end;
            case Node::WordB: return word_before(pos) != word_at(pos);
            case Node::NotWordB: return word_before(pos) == word_at(pos);
            default: return false;
        }
    }

    static bool simple_zero_width(const Node& n) {
        return n.kind == Node::Bol || n.kind == Node::BolM || n.kind == Node::Eol || n.kind == Node::EolM ||
               n.kind == Node::StrStart || n.kind == Node::StrEnd || n.kind == Node::WordB ||
               n.kind == Node::NotWordB;
    }

    bool resume(const Cont* k, size_t pos) {
        switch (k->type) {
            case Cont::Done:
                if (full && pos != end) return false;
                if (must_advance && pos == start) return false;
                match_end = pos;
                return true;
            case Cont::LookDone:
                return true;
            case Cont::Next:
                return seq(*k->seq, k->idx, pos, k->next);
            case Cont::Close: {
                const long a = marks[2 * k->group], b = marks[2 * k->group + 1];
                const int last = lastindex;
                marks[2 * k->group] = static_cast<long>(k->open);
                marks[2 * k->group + 1] = static_cast<long>(pos);
                lastindex = k->group;
                if (resume(k->next, pos)) return true;
                marks[2 * k->group] = a;
                marks[2 * k->group + 1] = b;
                lastindex = last;
                return false;
            }
            case Cont::Rep:
                return rep(*k->rep, k->count, pos, k->last, k->next);
        }
        return false;
    }

    bool alts(const std::vector<Seq>& options, size_t pos, const Cont* k) {
        for (const auto& option : options)
            if (seq(option, 0, pos, k)) return true;
        return false;
    }

    bool seq(const Seq& q, size_t i, size_t pos, const Cont* k) {
        while (i < q.size()) {
            const Node& n = q[i];
            if (char_node(n)) {
                if (pos < end && test1(n, s[pos])) {
                    ++pos, ++i;
                    continue;
                }
                return false;
            }
            if (simple_zero_width(n)) {
                if (!zero_width(n, pos)) return false;
                ++i;
                continue;
            }
            if (i + 1 == q.size()) return node(n, pos, k);
            Cont c;
            c.type = Cont::Next;
            c.seq = &q;
            c.idx = i + 1;
            c.next = k;
            return node(n, pos, &c);
        }
        return resume(k, pos);
    }

    bool rep(const Node& n, long count, size_t pos, size_t last, const Cont* k) {
        const bool more = n.max < 0 || count < n.max;
        auto item = [&] {
            Cont c;
            c.type = Cont::Rep;
            c.rep = &n;
            c.count = count + 1;
            c.last = pos;
            c.next = k;
            return seq(n.alts[0], 0, pos, &c);
        };
        if (count < n.min) return item();
        if (n.greedy) {
            if (more && pos != last && item()) return true;
            return resume(k, pos);
        }
        if (resume(k, pos)) return true;
        return more && pos != last && item();
    }

    bool node(const Node& n, size_t pos, const Cont* k) {
        switch (n.kind) {
            case Node::Group: {
                for (const auto& option : n.alts) {
                    Cont c;
                    c.type = Cont::Close;
                    c.group = n.group;
                    c.open = pos;
                    c.next = k;
                    if (seq(option, 0, pos, &c)) return true;
                }
                return false;
            }
            case Node::Alt:
                return alts(n.alts, pos, k);
            case Node::Repeat: {
                const Seq& body = n.alts[0];
                if (body.size() == 1 && char_node(body[0])) {
                    const Node& c = body[0];
                    const size_t room = end - pos;
                    const size_t limit = n.max < 0 ? room : std::min(room, static_cast<size_t>(n.max));
                    size_t got = 0;
                    while (got < limit && test1(c, s[pos + got])) ++got;
                    const size_t lo = static_cast<size_t>(n.min);
                    if (got < lo) return false;
                    if (n.greedy) {
                        for (size_t m = got;; --m) {
                            if (resume(k, pos + m)) return true;
                            if (m == lo) break;
                        }
                        return false;
                    }
                    for (size_t m = lo; m <= got; ++m)
                        if (resume(k, pos + m)) return true;
                    return false;
                }
                return rep(n, 0, pos, npos, k);
            }
            case Node::Look: {
                const std::vector<long> saved = marks;
                const int saved_last = lastindex;
                Cont done;
                done.type = Cont::LookDone;
                bool ok = false;
                if (!n.behind) {
                    ok = alts(n.alts, pos, &done);
                } else if (pos >= static_cast<size_t>(n.width)) {
                    ok = alts(n.alts, pos - static_cast<size_t>(n.width), &done);
                }
                if (n.negate) {
                    marks = saved;
                    lastindex = saved_last;
                    return !ok && resume(k, pos);
                }
                if (ok && resume(k, pos)) return true;
                marks = saved;
                lastindex = saved_last;
                return false;
            }
            case Node::Backref: {
                const long a = marks[2 * n.group], b = marks[2 * n.group + 1];
                if (a < 0 || b < 0) return false;
                const size_t len = static_cast<size_t>(b - a);
                if (pos + len > end) return false;
                for (size_t j = 0; j < len; ++j) {
                    const char32_t x = s[static_cast<size_t>(a) + j], y = s[pos + j];
                    if (x != y && !(n.icase && sre_lower(x) == sre_lower(y))) return false;
                }
                return resume(k, pos + len);
            }
            case Node::Cond: {
                const bool set = marks[2 * n.group] >= 0 && marks[2 * n.group + 1] >= 0;
                return seq(n.alts[set ? 0 : 1], 0, pos, k);
            }
            default:
                if (char_node(n)) return pos < end && test1(n, s[pos]) && resume(k, pos + 1);
                return zero_width(n, pos) && resume(k, pos);
        }
    }
};

}  // namespace

// ── Match ───────────────────────────────────────────────────────────────────

std::optional<U32> Match::group(int g) const {
    if (g < 0 || 2 * static_cast<size_t>(g) + 1 >= spans_.size()) throw IndexError("no such group");
    if (spans_[2 * g] < 0) return std::nullopt;
    return U32(subject_.substr(start(g), end(g) - start(g)));
}

int Match::index(std::string_view name) const {
    const auto it = program_->names.find(std::string(name));
    if (it == program_->names.end()) throw IndexError("no such group");
    return it->second;
}

std::optional<U32> Match::group(std::string_view name) const { return group(index(name)); }
U32 Match::str(int g) const { return group(g).value_or(U32()); }
U32 Match::str(std::string_view name) const { return group(name).value_or(U32()); }
bool Match::matched(std::string_view name) const { return matched(index(name)); }
int Match::groups() const { return program_->groups; }

std::string Match::lastgroup() const {
    if (lastindex_ < 0) return "";
    return program_->group_names[lastindex_];
}

// ── Pattern ─────────────────────────────────────────────────────────────────

Pattern::Pattern(U32View pattern, int flags) {
    auto prog = std::make_shared<Program>();
    prog->pattern = U32(pattern);
    prog->group_names.resize(1);
    Parser(prog->pattern, flags, *prog).parse();
    prog->first = first_char(prog->top);
    program_ = std::move(prog);
}

Pattern::Pattern(std::string_view pattern, int flags) : Pattern(detail::from_utf8(pattern), flags) {}

const U32& Pattern::pattern() const { return program_->pattern; }
int Pattern::flags() const { return program_->flags; }
int Pattern::groups() const { return program_->groups; }

std::optional<Match> Pattern::run(U32View s, size_t pos, size_t endpos, bool anchored, bool full,
                                  bool must_advance) const {
    const Program& prog = *program_;
    Matcher m;
    m.s = s;
    m.end = std::min(endpos, s.size());
    m.full = full;
    if (pos > s.size()) pos = s.size();
    if (pos > m.end) return std::nullopt;
    Cont done;
    done.type = Cont::Done;
    for (size_t p = pos; p <= m.end; ++p) {
        if (!anchored && prog.first != nullptr && (p == m.end || !test1(*prog.first, s[p]))) continue;
        m.marks.assign(2 * (static_cast<size_t>(prog.groups) + 1), -1);
        m.lastindex = -1;
        m.start = p;
        m.must_advance = must_advance && p == pos;
        if (m.alts(prog.top, p, &done)) {
            Match out;
            out.subject_ = s;
            out.spans_ = std::move(m.marks);
            out.spans_[0] = static_cast<long>(p);
            out.spans_[1] = static_cast<long>(m.match_end);
            out.lastindex_ = m.lastindex;
            out.program_ = program_;
            return out;
        }
        if (anchored) break;
    }
    return std::nullopt;
}

std::optional<Match> Pattern::search(U32View s, size_t pos, size_t endpos) const {
    return run(s, pos, endpos, false, false, false);
}

std::optional<Match> Pattern::match(U32View s, size_t pos, size_t endpos) const {
    return run(s, pos, endpos, true, false, false);
}

std::optional<Match> Pattern::fullmatch(U32View s, size_t pos, size_t endpos) const {
    return run(s, pos, endpos, true, true, false);
}

std::vector<Match> Pattern::finditer(U32View s, size_t pos, size_t endpos) const {
    std::vector<Match> out;
    const size_t end = std::min(endpos, s.size());
    bool must = false;
    while (pos <= end) {
        auto m = run(s, pos, end, false, false, must);
        if (!m) break;
        must = m->end() == m->start();
        pos = m->end();
        out.push_back(std::move(*m));
    }
    return out;
}

std::vector<U32> Pattern::findall(U32View s) const {
    std::vector<U32> out;
    for (const auto& m : finditer(s)) out.push_back(groups() == 0 ? m.str(0) : m.str(1));
    return out;
}

namespace {

struct TemplatePart {
    U32 text;
    int group = -1;
};

std::vector<TemplatePart> parse_template(U32View repl, const Program& prog) {
    std::vector<TemplatePart> parts;
    U32 literal;
    auto flush = [&] {
        if (!literal.empty()) parts.push_back({literal, -1});
        literal.clear();
    };
    for (size_t i = 0; i < repl.size(); ++i) {
        const char32_t c = repl[i];
        if (c != U'\\' || i + 1 >= repl.size()) {
            literal += c;
            continue;
        }
        const char32_t e = repl[++i];
        if (e == U'g' && i + 1 < repl.size() && repl[i + 1] == U'<') {
            const size_t close = repl.find(U'>', i + 2);
            if (close == U32View::npos) throw error("missing >, unterminated name");
            const std::string name = detail::to_utf8(repl.substr(i + 2, close - i - 2));
            int g;
            if (!name.empty() && std::all_of(name.begin(), name.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) {
                g = std::stoi(name);
            } else {
                const auto it = prog.names.find(name);
                if (it == prog.names.end()) throw IndexError("unknown group name '" + name + "'");
                g = it->second;
            }
            if (g > prog.groups) throw error("invalid group reference " + std::to_string(g));
            flush();
            parts.push_back({U32(), g});
            i = close;
        } else if (e >= U'1' && e <= U'9') {
            int g = static_cast<int>(e - U'0');
            if (i + 1 < repl.size() && repl[i + 1] >= U'0' && repl[i + 1] <= U'9') {
                g = g * 10 + static_cast<int>(repl[++i] - U'0');
            }
            if (g > prog.groups) throw error("invalid group reference " + std::to_string(g));
            flush();
            parts.push_back({U32(), g});
        } else {
            switch (e) {
                case U'n': literal += U'\n'; break;
                case U't': literal += U'\t'; break;
                case U'r': literal += U'\r'; break;
                case U'f': literal += U'\f'; break;
                case U'v': literal += U'\v'; break;
                case U'a': literal += U'\a'; break;
                case U'\\': literal += U'\\'; break;
                default:
                    if ((e >= U'a' && e <= U'z') || (e >= U'A' && e <= U'Z')) throw error("bad escape in template");
                    literal += U'\\';
                    literal += e;
            }
        }
    }
    flush();
    return parts;
}

}  // namespace

U32 Pattern::sub(U32View repl, U32View s) const {
    const auto parts = parse_template(repl, *program_);
    return sub(
        [&](const Match& m) {
            U32 out;
            for (const auto& p : parts) out += p.group < 0 ? p.text : m.str(p.group);
            return out;
        },
        s);
}

U32 Pattern::sub(const std::function<U32(const Match&)>& repl, U32View s) const {
    U32 out;
    size_t i = 0, pos = 0;
    bool must = false;
    while (pos <= s.size()) {
        auto m = run(s, pos, s.size(), false, false, must);
        if (!m) break;
        out.append(s.substr(i, m->start() - i));
        out += repl(*m);
        i = m->end();
        must = m->end() == m->start();
        pos = m->end();
    }
    out.append(s.substr(i));
    return out;
}

std::vector<std::optional<U32>> Pattern::split(U32View s) const {
    std::vector<std::optional<U32>> out;
    size_t last = 0, pos = 0;
    bool must = false;
    while (pos <= s.size()) {
        auto m = run(s, pos, s.size(), false, false, must);
        if (!m) break;
        out.emplace_back(U32(s.substr(last, m->start() - last)));
        for (int g = 1; g <= groups(); ++g) out.push_back(m->group(g));
        must = m->end() == m->start();
        last = pos = m->end();
    }
    out.emplace_back(U32(s.substr(last)));
    return out;
}

const Pattern& compiled(U32View pattern, int flags) {
    static std::mutex lock;
    static std::map<std::pair<U32, int>, std::unique_ptr<Pattern>> cache;
    std::lock_guard<std::mutex> guard(lock);
    auto key = std::make_pair(U32(pattern), flags);
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(std::move(key), std::make_unique<Pattern>(pattern, flags)).first;
    return *it->second;
}

const Pattern& compiled(std::string_view pattern, int flags) { return compiled(detail::from_utf8(pattern), flags); }

U32 escape(U32View text) {
    static const U32 special = U"()[]{}?*+-|^$\\.&~# \t\n\r\v\f";
    U32 out;
    for (const char32_t c : text) {
        if (special.find(c) != U32::npos) out += U'\\';
        out += c;
    }
    return out;
}

}  // namespace arbtok::pyre
