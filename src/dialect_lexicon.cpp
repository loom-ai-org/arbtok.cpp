#include "arbtok/dialect_lexicon.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <map>
#include <set>

#include <orthography2ipa/orthography2ipa.hpp>

#include "arbtok/tokenizer.hpp"
#include "detail.hpp"
#include "pyio.hpp"

namespace arbtok::dialect_lexicon {

namespace o2i = orthography2ipa;
using tokenizer::normalize_unicode;

namespace {

// Path.glob("*.tsv") of base, in sorted order. A missing directory has none.
std::vector<std::string> tsv_names(const std::string& base) {
    std::vector<std::string> names;
    DIR* dir = ::opendir(base.c_str());
    if (dir == nullptr) return names;
    while (const dirent* entry = ::readdir(dir)) {
        const std::string name = entry->d_name;
        if (name != "." && name != ".." && name.size() >= 4 && name.compare(name.size() - 4, 4, ".tsv") == 0)
            names.push_back(name);
    }
    ::closedir(dir);
    std::sort(names.begin(), names.end());
    return names;
}

// Path(name).stem for a name that ends with ".tsv".
std::string stem(const std::string& name) { return name == ".tsv" ? name : name.substr(0, name.size() - 4); }

// _lexicon_files(base): resolved lect -> file. A later name wins a collision.
std::map<std::string, std::string> lexicon_files(const std::string& base) {
    std::map<std::string, std::string> files;
    for (const auto& name : tsv_names(base)) files[o2i::resolve(stem(name))] = detail::path_join(base, name);
    return files;
}

}  // namespace

std::string lexicon_dir() { return detail::path_join(data_dir(), "lexicons"); }

std::optional<std::string> lexicon_file(const std::string& lect) {
    const auto files = lexicon_files(lexicon_dir());
    const auto it = files.find(o2i::resolve(lect));
    if (it == files.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> available_lects() {
    const std::string dir = lexicon_dir();
    struct stat st {};
    if (::stat(dir.c_str(), &st) != 0) return {};
    std::vector<std::string> out;
    for (const auto& [lect, path] : lexicon_files(dir)) out.push_back(lect);
    return out;
}

OrderedDict<std::string> parse(const std::string& text) {
    OrderedDict<std::string> entries;
    for (const auto& line32 : detail::splitlines(detail::u(text))) {
        const std::string line = detail::s8(line32);
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t tab; (tab = line.find('\t', start)) != std::string::npos; start = tab + 1)
            parts.push_back(line.substr(start, tab - start));
        parts.push_back(line.substr(start));
        if (parts.size() < 2 || parts[0] == "key") continue;
        const std::string key = normalize_unicode(parts[0]), voc = normalize_unicode(parts[1]);
        if (!key.empty() && !voc.empty()) entries.setdefault(key, voc);
    }
    return entries;
}

std::vector<std::string> parent_chain(const std::string& lang) {
    std::vector<std::string> chain{lang};
    std::set<std::string> seen{lang};
    std::string cur = lang;
    try {
        for (;;) {
            const std::string parent = o2i::get(cur).parent;
            if (parent.empty() || parent == "null" || seen.count(parent)) break;
            chain.push_back(parent);
            seen.insert(parent);
            cur = parent;
        }
    } catch (const std::exception&) {
    }
    return chain;
}

DialectLexicon::DialectLexicon(std::string lang, std::optional<std::string> data_dir)
    : lang_(std::move(lang)), data_dir_(std::move(data_dir)) {}

const OrderedDict<std::string>& DialectLexicon::entries() const {
    if (!entries_) {
        const auto files = lexicon_files(data_dir_ ? *data_dir_ : lexicon_dir());
        const auto chain = parent_chain(lang_);
        OrderedDict<std::string> merged;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const auto file = files.find(o2i::resolve(*it));
            if (file != files.end()) merged.update(parse(detail::read_text(file->second)));
        }
        entries_ = std::move(merged);
    }
    return *entries_;
}

std::optional<std::string> DialectLexicon::get(const std::string& word) const {
    const std::string* voc = entries().get(normalize_unicode(word));
    if (voc == nullptr) return std::nullopt;
    return *voc;
}

}  // namespace arbtok::dialect_lexicon
