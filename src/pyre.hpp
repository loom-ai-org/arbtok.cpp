// Python's re module for str patterns, as far as textnorm and util use it: a
// backtracking matcher over code points. For the syntax below and a pattern
// that CPython 3.12 compiles, a call gives the spans that CPython 3.12's sre
// gives. \d is str.isdecimal(), \w is str.isalnum() or "_", \s is
// str.isspace(), IGNORECASE compares the simple lowercase with re's extra case
// pairs, $ matches before a final "\n", a look-behind is fixed width, and an
// empty match next to the previous match is allowed in finditer, sub and split.
// As in sre, \b and \B fail when the string up to endpos is empty, and a
// repeat does not try an item past its minimum at the position where the
// previous item past its minimum started. As in sre, a group sets its start
// mark when the match enters it, and (?(group)yes|no) takes the yes branch only
// when the end mark of the group is not before its start mark. The goldens of
// tests/goldens/pyre hold every module pattern and the engine patterns of
// tools/goldens/pyre.py.
//
// Supported syntax: literals and escapes, classes, . ^ $ \A \Z \b \B, groups
// (capturing, (?:...), (?P<name>...), (?P=name), \N), look-ahead and
// look-behind, (?(group)yes|no), (?i:...), a leading (?i) or (?s), and the
// quantifiers * + ? {m,n} with their lazy forms. Any other syntax raises
// pyre::error, as an unsupported pattern must not match silently. The
// compiler does not check all that sre checks: a pattern with a back-reference
// to a group that is still open, or with a reference to a group defined later,
// compiles here, and sre raises re.error for it. The patterns that the port
// compiles at call time escape their data, so no such pattern reaches the
// compiler.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "arbtok/errors.hpp"
#include "detail.hpp"

namespace arbtok::pyre {

using detail::U32;
using detail::U32View;

// re.error.
struct error : Error {
    explicit error(const std::string& message) : Error("error", message) {}
};

enum Flag : int { IGNORECASE = 2, MULTILINE = 8, DOTALL = 16 };

inline constexpr size_t npos = static_cast<size_t>(-1);

struct Program;

class Match {
public:
    // The subject string the match was made in; the caller keeps it alive.
    U32View string() const { return subject_; }
    size_t start(int g = 0) const { return static_cast<size_t>(spans_[2 * g]); }
    size_t end(int g = 0) const { return static_cast<size_t>(spans_[2 * g + 1]); }
    // Whether the group took part in the match.
    bool matched(int g) const { return spans_[2 * g] >= 0; }
    // m.group(g); nullopt for a group that did not take part.
    std::optional<U32> group(int g = 0) const;
    std::optional<U32> group(std::string_view name) const;
    // m.group(g) or "" for a group that did not take part.
    U32 str(int g = 0) const;
    U32 str(std::string_view name) const;
    bool matched(std::string_view name) const;
    int index(std::string_view name) const;
    // m.lastgroup: the name of the last group that closed, or "".
    std::string lastgroup() const;
    int lastindex() const { return lastindex_; }
    int groups() const;
    // The raw spans, two per group, -1 for a group that did not take part.
    const std::vector<long>& spans() const { return spans_; }

private:
    friend class Pattern;
    U32View subject_;
    std::vector<long> spans_;
    int lastindex_ = -1;
    std::shared_ptr<const Program> program_;
};

class Pattern {
public:
    Pattern(U32View pattern, int flags = 0);
    Pattern(std::string_view pattern, int flags = 0);
    Pattern(const char* pattern, int flags = 0) : Pattern(std::string_view(pattern), flags) {}

    const U32& pattern() const;
    int flags() const;
    int groups() const;

    // pattern.search / match / fullmatch(string, pos, endpos).
    std::optional<Match> search(U32View s, size_t pos = 0, size_t endpos = npos) const;
    std::optional<Match> match(U32View s, size_t pos = 0, size_t endpos = npos) const;
    std::optional<Match> fullmatch(U32View s, size_t pos = 0, size_t endpos = npos) const;
    // list(pattern.finditer(string, pos, endpos)).
    std::vector<Match> finditer(U32View s, size_t pos = 0, size_t endpos = npos) const;
    // pattern.findall(string) for a pattern with no group or one group.
    std::vector<U32> findall(U32View s) const;
    // pattern.sub(repl, string) with a template ("\1", "\g<name>") or a function.
    U32 sub(U32View repl, U32View s) const;
    U32 sub(const std::function<U32(const Match&)>& repl, U32View s) const;
    // pattern.split(string): the pieces and the groups between them, a group
    // that did not take part as nullopt.
    std::vector<std::optional<U32>> split(U32View s) const;

    bool fullmatches(U32View s) const { return fullmatch(s).has_value(); }

private:
    std::optional<Match> run(U32View s, size_t pos, size_t endpos, bool anchored, bool full,
                             bool must_advance) const;
    std::shared_ptr<const Program> program_;
};

// re.compile(pattern, flags), cached by pattern and flags as re's cache is.
const Pattern& compiled(U32View pattern, int flags = 0);
const Pattern& compiled(std::string_view pattern, int flags = 0);

// re.escape(text).
U32 escape(U32View text);

// The simple lowercase _sre.unicode_tolower gives.
char32_t sre_lower(char32_t c);
// re's \d, \w.
bool is_decimal(char32_t c);
bool is_word(char32_t c);

}  // namespace arbtok::pyre
