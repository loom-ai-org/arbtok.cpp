#include "arbtok/lexicon.hpp"

#include <sys/stat.h>

#include <cctype>
#include <filesystem>
#include <map>
#include <mutex>
#include <tuple>

#include "arbtok/tokenizer.hpp"
#include "detail.hpp"
#include "fetch.hpp"
#include "json.hpp"
#include "pyio.hpp"
#include "unicode_norm.hpp"

namespace arbtok::lexicon {

namespace fs = std::filesystem;
using tokenizer::normalize_unicode;

namespace {

bool starts_with(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t i = s.find(sep, start);
        if (i == std::string::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, i - start));
        start = i + 1;
    }
}

std::vector<std::string> tsv_lines(const std::string& text) {
    std::vector<std::string> out;
    for (const auto& line : detail::splitlines(detail::u(text))) out.push_back(detail::s8(line));
    return out;
}

bool exists(const std::string& path) {
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0;
}

// os.path.expandvars on POSIX: $name and ${name} of a set variable.
std::string expandvars(const std::string& path) {
    std::string out;
    size_t i = 0;
    while (i < path.size()) {
        if (path[i] != '$') {
            out += path[i++];
            continue;
        }
        size_t end = i + 1;
        std::string name;
        if (end < path.size() && path[end] == '{') {
            const size_t close = path.find('}', end + 1);
            if (close == std::string::npos) {
                out += path[i++];
                continue;
            }
            name = path.substr(end + 1, close - end - 1);
            end = close + 1;
        } else {
            while (end < path.size() && (std::isalnum(static_cast<unsigned char>(path[end])) || path[end] == '_')) ++end;
            name = path.substr(i + 1, end - i - 1);
            if (name.empty()) {
                out += path[i++];
                continue;
            }
        }
        if (const auto value = detail::getenv(name.c_str())) {
            out += *value;
        } else {
            out += path.substr(i, end - i);
        }
        i = end;
    }
    return out;
}

std::string getenv_or(const char* name, const std::string& fallback) {
    const auto v = detail::getenv(name);
    return v ? *v : fallback;
}

// huggingface_hub.constants.HF_HUB_OFFLINE.
bool hub_offline() {
    auto v = detail::getenv("HF_HUB_OFFLINE");
    if (!v || v->empty()) v = detail::getenv("TRANSFORMERS_OFFLINE");
    if (!v) return false;
    std::string up;
    for (char c : *v) up.push_back(static_cast<char>(c >= 'a' && c <= 'z' ? c - 32 : c));
    return up == "1" || up == "ON" || up == "YES" || up == "TRUE";
}

// huggingface_hub.constants.HF_HUB_CACHE.
std::string hub_cache() {
    const std::string default_home = detail::path_join(detail::expanduser("~"), ".cache");
    const std::string hf_home = expandvars(detail::expanduser(
        getenv_or("HF_HOME", detail::path_join(getenv_or("XDG_CACHE_HOME", default_home), "huggingface"))));
    const std::string legacy = getenv_or("HUGGINGFACE_HUB_CACHE", detail::path_join(hf_home, "hub"));
    return expandvars(detail::expanduser(getenv_or("HF_HUB_CACHE", legacy)));
}

// huggingface_hub.utils.validate_repo_id for a "namespace/name" id.
void validate_repo_id(const std::string& repo) {
    auto part_ok = [](const std::string& part) {
        if (part.empty() || part.size() > 96) return false;
        for (unsigned char c : part)
            if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.' || c >= 0x80)) return false;
        return true;
    };
    const size_t slash = repo.find('/');
    const std::string ns = repo.substr(0, slash), name = repo.substr(slash + 1);
    const bool ends_ok = !name.empty() && name.front() != '-' && name.front() != '.' && name.back() != '-' &&
                         name.back() != '.' && ns.front() != '-' && ns.front() != '.';
    if (!part_ok(ns) || !part_ok(name) || !ends_ok || repo.find("--") != std::string::npos ||
        repo.find("..") != std::string::npos || (name.size() >= 4 && name.compare(name.size() - 4, 4, ".git") == 0))
        throw Error("HFValidationError", "Repo id must use alphanumeric chars, '-', '_' or '.'.");
}

// hf_hub_download(repo, filename, repo_type="dataset") at revision main.
std::string hf_dataset_file(const std::string& repo, const std::string& filename) {
    std::string folder_name = "datasets--";
    for (char c : repo) folder_name += c == '/' ? std::string("--") : std::string(1, c);
    const std::string folder = detail::path_join(hub_cache(), folder_name);
    const std::string ref = detail::path_join(folder, "refs/main");
    for (const auto& part : split(filename, '/'))
        if (part == "..") throw ValueError("Invalid filename: " + filename);
    if (!filename.empty() && filename.front() == '/') throw ValueError("Invalid filename: " + filename);
    validate_repo_id(repo);
    if (exists(ref)) {
        const std::string commit = detail::read_text(ref);
        const std::string pointer = detail::path_join(detail::path_join(folder, "snapshots/" + commit), filename);
        if (exists(pointer)) return pointer;
    }
    if (hub_offline())
        throw Error("LocalEntryNotFoundError",
                    "An error happened while trying to locate the file on the Hub and we cannot find the requested "
                    "files in the local cache. Please check your connection and try again or make sure your Internet "
                    "connection is on.");
    const json::Value info = json::loads(detail::fetch_url("https://huggingface.co/api/datasets/" + repo + "/revision/main"));
    const std::string commit = info.at("sha").text;
    const std::string pointer = detail::path_join(detail::path_join(folder, "snapshots/" + commit), filename);
    fs::create_directories(fs::path(pointer).parent_path());
    detail::write_file_atomic(pointer, detail::fetch_url("https://huggingface.co/datasets/" + repo + "/resolve/" +
                                                         commit + "/" + filename));
    fs::create_directories(fs::path(ref).parent_path());
    detail::write_file_atomic(ref, commit);
    return pointer;
}

}  // namespace

std::string cache_dir() {
    const auto xdg = detail::getenv("XDG_CACHE_HOME");
    const std::string base = xdg ? detail::path_str(*xdg) : detail::path_join(detail::expanduser("~"), ".cache");
    return detail::path_join(base, "arbtok/lexicons");
}

std::string resolve_source(const std::string& source) {
    if (starts_with(source, "hf://")) {
        const std::vector<std::string> parts = split(source.substr(5), '/');
        if (parts.size() < 2) throw ValueError("not enough values to unpack (expected at least 2, got 1)");
        std::string file;
        for (size_t i = 2; i < parts.size(); ++i) file += (i > 2 ? "/" : "") + parts[i];
        return detail::path_str(hf_dataset_file(parts[0] + "/" + parts[1], file));
    }
    if (starts_with(source, "http://") || starts_with(source, "https://")) {
        const std::string dir = cache_dir();
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) throw detail::os_error(ec.value(), dir);
        const std::string target = detail::path_join(dir, source.substr(source.rfind('/') + 1));
        if (!exists(target)) detail::write_file_atomic(target, detail::fetch_url(source));
        return target;
    }
    return detail::expanduser(source);
}

OrderedDict<std::string> parse(const std::string& text) {
    OrderedDict<std::string> entries;
    for (const auto& line : tsv_lines(text)) {
        const auto parts = split(line, '\t');
        if (parts.size() < 2 || parts[0] == "key") continue;
        const std::string key = normalize_unicode(parts[0]), stem = normalize_unicode(parts[1]);
        if (!key.empty() && !stem.empty()) entries.setdefault(key, stem);
    }
    return entries;
}

OrderedDict<double> parse_shares(const std::string& text) {
    OrderedDict<double> shares;
    for (const auto& line : tsv_lines(text)) {
        const auto parts = split(line, '\t');
        if (parts.size() < 4 || parts[0] == "key") continue;
        const std::string key = normalize_unicode(parts[0]);
        const auto top = detail::py_int(detail::u(parts[2]));
        const auto runner_up = detail::py_int(detail::u(parts[3]));
        if (!top || !runner_up) continue;
        if (key.empty() || shares.contains(key)) continue;
        long long total = 0;
        if (!__builtin_add_overflow(*top, *runner_up, &total)) {
            if (total > 0) shares.setdefault(key, detail::py_true_divide(*runner_up, total));
        } else if (*top > 0) {
            // Two positive counts whose sum leaves long long.
            const long double sum = static_cast<long double>(*top) + static_cast<long double>(*runner_up);
            shares.setdefault(key, static_cast<double>(static_cast<long double>(*runner_up) / sum));
        }
    }
    return shares;
}

struct StemLexicon::Data {
    OrderedDict<std::string> entries;
    OrderedDict<double> shares;
};

StemLexicon::StemLexicon(std::string source, std::optional<double> ambiguous_share)
    : source_(std::move(source)), ambiguous_share_(ambiguous_share) {}

void StemLexicon::load() const {
    // The default lexicon is 5 MB, and every diacritizer of every lect loads it.
    using Key = std::tuple<std::string, long long, long long, long long>;
    static std::mutex mu;
    static std::map<Key, std::weak_ptr<const Data>> shared;
    std::string path, text;
    try {
        path = resolve_source(source_);
        text = detail::read_text(path);
    } catch (const std::exception& e) {
        const auto* error = dynamic_cast<const Error*>(&e);
        throw LexiconUnavailable("the stem lexicon " + detail::py_str_repr(detail::u(source_)) +
                                 " could not be read: " + (error ? error->message() : std::string(e.what())) +
                                 "\n\nPass lexicon=None to run the diacritizer without one.");
    }
    struct stat st {};
    ::stat(path.c_str(), &st);
    const Key key{path, static_cast<long long>(st.st_size), static_cast<long long>(st.st_mtim.tv_sec),
                  static_cast<long long>(st.st_mtim.tv_nsec)};
    std::lock_guard<std::mutex> lock(mu);
    if (auto cached = shared[key].lock()) {
        data_ = cached;
        return;
    }
    auto data = std::make_shared<Data>();
    data->entries = parse(text);
    data->shares = parse_shares(text);
    shared[key] = data;
    data_ = std::move(data);
}

const OrderedDict<std::string>& StemLexicon::entries() const {
    if (!data_) load();
    return data_->entries;
}

const OrderedDict<double>& StemLexicon::shares() const {
    static const OrderedDict<double> none;
    return data_ ? data_->shares : none;
}

std::optional<std::string> StemLexicon::get(const std::string& word, std::optional<long> context_words) const {
    const std::string key = normalize_unicode(word);
    const std::string* stem = entries().get(key);
    if (stem == nullptr) return std::nullopt;
    if (context_words && *context_words >= MIN_CONTEXT_WORDS && ambiguous(key)) return std::nullopt;
    return *stem;
}

bool StemLexicon::ambiguous(const std::string& word) const {
    if (!ambiguous_share_) return false;
    if (!data_) load();
    const double* share = data_->shares.get(normalize_unicode(word));
    return (share ? *share : 0.0) >= *ambiguous_share_;
}

size_t StemLexicon::size() const { return entries().size(); }

bool StemLexicon::contains(const std::string& word) const { return get(word).has_value(); }

}  // namespace arbtok::lexicon
