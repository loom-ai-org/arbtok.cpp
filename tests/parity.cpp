// Byte parity with the Python arbtok. Each golden record holds the inputs
// and Python's output of one call; this recomputes the record in C++ and
// compares the bytes.
//
// --divergences <file> names the records that are known not to match until
// a dependency fix lands, with the C++ line that each one gives. A listed
// record that gives that C++ line is counted apart. A listed record that gives
// another C++ line is a mismatch. A listed record that matches Python, or that
// is not in the goldens, fails the run, so that the list shrinks when the fix
// lands.
//
// A golden whose dump ran the model names the CPU class of the dump in its
// header (tests/cpu_class.hpp). On a CPU of another class the run stops with
// that reason before it reads a record.
//
// A record that depends on the GPL-2.0-derived stem lexicon holds
// {"sha256": <hex>} in place of each result item, where <hex> is the SHA-256
// of Python's line. The C++ line of such a record is compared by its SHA-256,
// and the golden header gives the count of those records.
//
// --shard k/n checks the records whose index modulo n is k, so that ctest can
// run the large modules in parallel processes. --mismatches <file> writes each
// golden line that mismatches, in full, to the file.
#include <algorithm>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <pthread.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cpu_class.hpp"
#include "json_line.hpp"
#include "sha256.hpp"
#include "parity/modules.hpp"

namespace {

struct Tally {
    long expected = 0, read = 0, checked = 0, mismatches = 0, divergences = 0, stale = 0, hashed = 0;
};

// The digest of a hashed record, whose line ends with {"sha256":"<64 hex digits>"}].
std::optional<std::string> record_digest(const std::string& line) {
    static const std::string open = "{\"sha256\":\"", close = "\"}]";
    const size_t n = open.size() + 64 + close.size();
    if (line.size() < n || line.compare(line.size() - n, open.size(), open) != 0 ||
        line.compare(line.size() - close.size(), close.size(), close) != 0)
        return std::nullopt;
    const std::string hex = line.substr(line.size() - 64 - close.size(), 64);
    if (hex.find_first_not_of("0123456789abcdef") != std::string::npos) return std::nullopt;
    return hex;
}

// The "cpu_class" of a golden header, empty when it has none: the golden then
// holds no model output.
std::string header_cpu_class(const std::string& header) {
    static const std::string key = "\"cpu_class\":\"";
    const size_t at = header.find(key);
    if (at == std::string::npos) return "";
    const size_t start = at + key.size();
    return header.substr(start, header.find('"', start) - start);
}

// The "sha256_records" of a golden header, 0 when it has none.
long header_hashed(const std::string& header) {
    const size_t at = header.rfind("\"sha256_records\":");
    return at == std::string::npos ? 0 : std::stol(header.substr(at + 17));
}

struct Divergence {
    std::string cpp;  // the line that the C++ gives
    std::string fix;  // the fix the record waits on
};

// (module, golden line) -> the expected C++ line and the fix.
using Divergences = std::map<std::pair<std::string, std::string>, Divergence>;

// One entry per line: module, the golden line, the C++ line, the fix it waits
// on, separated by tabs. A line that starts with '#' is a comment. A JSON line
// holds no tab, because JSON escapes it.
Divergences read_divergences(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read " + path);
    Divergences out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (std::count(line.begin(), line.end(), '\t') != 3)
            throw std::runtime_error("divergence line does not have four tab-separated fields: " + line);
        const size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        const auto key = std::make_pair(line.substr(0, a), line.substr(a + 1, b - a - 1));
        if (out.count(key)) throw std::runtime_error("divergence line is listed twice: " + line);
        out[key] = {line.substr(b + 1, c - b - 1), line.substr(c + 1)};
    }
    return out;
}

struct Shard {
    long index = 0, count = 1;
};

Tally run(const std::string& dir, const std::string& module, std::string (*record)(const json_line::Value&),
          const Divergences& divergences, long shown, const Shard& shard, std::ostream* mismatches) {
    const std::string path = dir + "/" + module + ".jsonl.gz";
    const auto lines = json_line::read_gz_lines(path);
    Tally t;
    if (lines.empty()) throw std::runtime_error("empty golden file " + path);
    t.expected = json_line::header_count(lines[0]);
    if (t.expected < 1) throw std::runtime_error("golden header gives no records: " + path);
    const long expected_hashed = header_hashed(lines[0]);
    if (const std::string golden_class = header_cpu_class(lines[0]); !golden_class.empty()) {
        const std::string why = cpu_class::mismatch(path, golden_class);
        if (!why.empty()) throw std::runtime_error(why);
    }
    std::map<std::string, long> by_function;
    // The listed lines the whole file holds: a listed divergence is stale only
    // when the file lacks it, whichever shard runs.
    std::set<std::string> seen;
    for (const auto& [key, divergence] : divergences)
        if (key.first == module) seen.insert(key.second);
    std::set<std::string> present;
    for (size_t i = 1; i < lines.size(); ++i)
        if (seen.count(lines[i])) present.insert(lines[i]);
    for (size_t i = 1; i < lines.size(); ++i) {
        ++t.read;
        const std::optional<std::string> digest = record_digest(lines[i]);
        if (digest) ++t.hashed;
        if (static_cast<long>(i - 1) % shard.count != shard.index) continue;
        std::string got;
        json_line::Value v;
        try {
            v = json_line::parse(lines[i]);
            got = record(v);
        } catch (const std::exception& e) {
            got = std::string("exception: ") + e.what();
        }
        ++t.checked;
        const std::string got_digest = digest ? sha256::hex(got) : std::string();
        const bool same = digest ? got_digest == *digest : got == lines[i];
        const auto listed = divergences.find({module, lines[i]});
        if (listed != divergences.end()) {
            const Divergence& d = listed->second;
            if (same) {
                ++t.stale;
                std::cerr << "LISTED DIVERGENCE NOW MATCHES " << module << " (" << d.fix
                          << "): remove it from the list\n  python: " << lines[i].substr(0, 3000) << "\n";
                continue;
            }
            if (got == d.cpp) {
                ++t.divergences;
                std::cerr << "EXPECTED DIVERGENCE " << module << " (" << d.fix << ")\n  python: "
                          << lines[i].substr(0, 3000) << "\n  c++:    " << got.substr(0, 3000) << "\n";
                continue;
            }
            std::cerr << "LISTED DIVERGENCE GIVES ANOTHER C++ LINE " << module << " (" << d.fix
                      << ")\n  python: " << lines[i].substr(0, 3000) << "\n  listed: " << d.cpp.substr(0, 3000)
                      << "\n  c++:    " << got.substr(0, 3000) << "\n";
        } else if (same) {
            continue;
        }
        if (mismatches != nullptr) *mismatches << lines[i] << "\n";
        ++by_function[v.size() ? v[0].str : "?"];
        if (++t.mismatches <= shown) {
            std::cerr << "MISMATCH " << module << "\n  python: " << lines[i].substr(0, 3000)
                      << "\n  c++:    " << got.substr(0, 3000) << "\n";
            if (digest)
                std::cerr << "  python sha256: " << *digest << "\n  c++ sha256:    " << got_digest << "\n";
        }
    }
    for (const auto& [key, divergence] : divergences) {
        if (key.first == module && !present.count(key.second)) {
            ++t.stale;
            std::cerr << "LISTED DIVERGENCE NOT IN THE GOLDENS " << module << " (" << divergence.fix
                      << ")\n  python: "
                      << key.second.substr(0, 3000) << "\n";
        }
    }
    if (t.hashed != expected_hashed) {
        ++t.mismatches;
        std::cerr << "HASHED RECORD COUNT " << module << ": the header gives " << expected_hashed << ", the file holds "
                  << t.hashed << "\n";
    }
    std::cout << module << ": " << t.expected << " in, " << t.checked << " checked, " << t.mismatches
              << " mismatches, " << t.divergences << " expected divergences, " << t.stale << " stale entries, "
              << t.hashed << " hashed records\n";
    for (const auto& [fn, n] : by_function) std::cout << "  " << fn << ": " << n << " mismatches\n";
    return t;
}

const std::vector<std::string>& module_names() {
#define ARBTOK_PARITY_NAME(name) #name,
    static const std::vector<std::string> names = {ARBTOK_PARITY_MODULES(ARBTOK_PARITY_NAME)};
#undef ARBTOK_PARITY_NAME
    return names;
}

struct Options {
    std::string dir;
    std::vector<std::string> modules;  // empty: every module
    Divergences divergences;
    long shown = 10;  // mismatches printed per module
    Shard shard;
    std::string mismatches;  // empty: do not write them
};

int run_all(const Options& opt) {
    for (const auto& [key, divergence] : opt.divergences) {
        const auto& names = module_names();
        if (std::find(names.begin(), names.end(), key.first) == names.end())
            throw std::runtime_error("divergence row names an unknown module \"" + key.first + "\" (" +
                                     divergence.fix + ")");
    }
    auto selected = [&](const std::string& name) {
        return opt.modules.empty() || std::find(opt.modules.begin(), opt.modules.end(), name) != opt.modules.end();
    };
    std::ofstream mismatch_file;
    if (!opt.mismatches.empty()) {
        mismatch_file.open(opt.mismatches);
        if (!mismatch_file) throw std::runtime_error("cannot write " + opt.mismatches);
    }
    std::ostream* mismatches = opt.mismatches.empty() ? nullptr : &mismatch_file;
    Tally total;
#define ARBTOK_PARITY_RUN(name)                                                  \
    if (selected(#name)) {                                                          \
        const Tally t = run(opt.dir, #name, parity::record_##name, opt.divergences, opt.shown, opt.shard, mismatches); \
        total.expected += t.expected;                                               \
        total.read += t.read;                                                       \
        total.checked += t.checked;                                                 \
        total.mismatches += t.mismatches;                                           \
        total.divergences += t.divergences;                                         \
        total.stale += t.stale;                                                     \
    }
    ARBTOK_PARITY_MODULES(ARBTOK_PARITY_RUN)
#undef ARBTOK_PARITY_RUN
    std::cout << "goldens: " << total.expected << " in, " << total.checked << " checked, " << total.mismatches
              << " mismatches, " << total.divergences << " expected divergences, " << total.stale << " stale entries\n";
    return (total.read == total.expected && total.mismatches == 0 && total.stale == 0) ? 0 : 1;
}

int run_guarded(const Options& opt) {
    try {
        return run_all(opt);
    } catch (const std::exception& e) {
        std::cerr << "arbtok_parity: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace

// usage: arbtok_parity <goldens dir> [--divergences <file>] [--stack-kb <n>] [--show <n>] [--shard <k>/<n>]
//        [--stride <n>] [--mismatches <file>] [module ...]
// --stack-kb runs the records on a thread with that stack size, to show that
// the matchers do not need a stack that grows with the subject. --show sets
// how many mismatches each module prints (10 by default). --stride <n> checks
// only every n-th record, for the slow sanitizer builds: it is --shard 0/<n>.
// A listed divergence of a record that is not checked is neither counted nor
// stale. The run reads every record and fails if the file holds fewer than
// its header count.
int main(int argc, char** argv) {
    Options opt;
    size_t stack_kb = 0;
    std::vector<std::string> rest;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if ((a == "--divergences" || a == "--stack-kb" || a == "--show" || a == "--shard" ||
                 a == "--stride" || a == "--mismatches") &&
                i + 1 < argc) {
                if (a == "--divergences") {
                    opt.divergences = read_divergences(argv[++i]);
                } else if (a == "--mismatches") {
                    opt.mismatches = argv[++i];
                } else if (a == "--stack-kb") {
                    stack_kb = std::stoul(argv[++i]);
                } else if (a == "--show") {
                    opt.shown = std::stol(argv[++i]);
                } else if (a == "--stride") {
                    opt.shard = {0, std::max(1L, std::stol(argv[++i]))};
                } else {
                    const std::string spec = argv[++i];
                    const size_t slash = spec.find('/');
                    if (slash == std::string::npos) throw std::runtime_error("--shard takes k/n");
                    opt.shard = {std::stol(spec.substr(0, slash)), std::stol(spec.substr(slash + 1))};
                    if (opt.shard.count < 1 || opt.shard.index < 0 || opt.shard.index >= opt.shard.count)
                        throw std::runtime_error("--shard takes k/n with 0 <= k < n");
                }
            } else {
                rest.push_back(a);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "arbtok_parity: " << e.what() << "\n";
        return 1;
    }
    if (rest.empty()) {
        std::cerr << "usage: arbtok_parity <goldens dir> [--divergences <file>] [--stack-kb <n>] [--show <n>] "
                     "[--shard <k>/<n>] [--stride <n>] [--mismatches <file>] [module ...]\n";
        return 2;
    }
    opt.dir = rest[0];
    opt.modules.assign(rest.begin() + 1, rest.end());
    for (const auto& m : opt.modules) {
        const auto& names = module_names();
        if (std::find(names.begin(), names.end(), m) == names.end()) {
            std::cerr << "arbtok_parity: unknown module \"" << m << "\"\n";
            return 2;
        }
    }
    if (stack_kb == 0) return run_guarded(opt);
    struct Job {
        const Options* opt;
        int status;
    } job{&opt, 1};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (pthread_attr_setstacksize(&attr, stack_kb * 1024) != 0) {
        std::cerr << "arbtok_parity: cannot set a stack of " << stack_kb << " KB\n";
        return 2;
    }
    pthread_t thread;
    auto body = [](void* p) -> void* {
        auto* j = static_cast<Job*>(p);
        j->status = run_guarded(*j->opt);
        return nullptr;
    };
    if (pthread_create(&thread, &attr, body, &job) != 0) {
        std::cerr << "arbtok_parity: cannot start the thread\n";
        return 2;
    }
    pthread_join(thread, nullptr);
    pthread_attr_destroy(&attr);
    return job.status;
}
