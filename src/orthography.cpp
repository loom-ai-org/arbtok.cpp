#include "arbtok/orthography.hpp"

#include "detail.hpp"

namespace arbtok::orthography {

namespace {

MarkSet marks_of(const std::string& klass) {
    MarkSet out;
    for (char32_t c : detail::u(klass)) out.insert(detail::s8(c));
    return out;
}

MarkSet orthographic(const MarkSet& marks) {
    MarkSet out;
    for (const auto& m : marks)
        if (ORTHOGRAPHIC_MARKS.count(m)) out.insert(m);
    return out;
}

}  // namespace

std::pair<std::string, std::vector<MarkSet>> source_marks(
    const std::string& text, const std::function<std::string(const std::string&)>& normalize) {
    detail::U32 bases;
    std::vector<MarkSet> marks;
    for (char32_t ch : detail::u(normalize(text))) {
        if (detail::is_nonspacing_mark(ch)) {
            if (!marks.empty()) marks.back().insert(detail::s8(ch));
        } else {
            bases.push_back(ch);
            marks.emplace_back();
        }
    }
    return {detail::s8(bases), marks};
}

std::vector<int> allowed_classes(const std::vector<std::string>& classes, const MarkSet& marks_here,
                                 bool allow_restoration) {
    const MarkSet here = orthographic(marks_here);
    std::vector<int> out;
    for (size_t i = 0; i < classes.size(); ++i) {
        const MarkSet marks = marks_of(classes[i]);
        bool keep;
        if (here.count(MADDA)) {
            keep = marks == MarkSet{MADDA};
        } else if (!here.empty()) {
            keep = orthographic(marks) == here;
        } else {
            keep = allow_restoration || orthographic(marks).empty();
        }
        if (keep) out.push_back(static_cast<int>(i));
    }
    return out;
}

int pinned_class(const std::vector<std::string>& classes, const MarkSet& marks_here) {
    int best = -1;
    size_t best_len = 0;
    for (size_t i = 0; i < classes.size(); ++i) {
        if (marks_of(classes[i]) != marks_here) continue;
        const size_t len = detail::u(classes[i]).size();
        if (best < 0 || len < best_len) {
            best = static_cast<int>(i);
            best_len = len;
        }
    }
    return best;
}

int project_class(const std::vector<std::string>& classes, int predicted, const MarkSet& marks_here,
                  bool allow_restoration) {
    const MarkSet here = orthographic(marks_here);
    const MarkSet predicted_marks = marks_of(classes.at(static_cast<size_t>(predicted)));
    const MarkSet predicted_ortho = orthographic(predicted_marks);
    MarkSet want;
    if (here.count(MADDA)) {
        want = {MADDA};
    } else if (predicted_ortho == here) {
        return predicted;
    } else if (here.empty() && allow_restoration) {
        return predicted;
    } else {
        for (const auto& m : predicted_marks)
            if (!predicted_ortho.count(m)) want.insert(m);
        want.insert(here.begin(), here.end());
    }
    const int projected = pinned_class(classes, want);
    return projected >= 0 ? projected : predicted;
}

}  // namespace arbtok::orthography
