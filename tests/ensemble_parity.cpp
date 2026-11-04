// Compare the ensemble port with goldens dumped from the Python package.
//
//   ensemble_parity <golden.jsonl.gz> [--show N] [--tolerance X]
//
// A golden file is either the ensemble goldens of tools/goldens/ensemble.py
// or the Unicode goldens of tools/goldens/ensemble_unicode.py; its header
// record names which. Exit status 0 means every record matched and the
// record count equals the count in the header.

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "arbtok/diacritize.hpp"
#include "arbtok/ensemble.hpp"
#include "detail.hpp"
#include "json.hpp"
#include "unicode_norm.hpp"

namespace {

namespace json = arbtok::json;
namespace uni = arbtok::detail;

const std::string& S(const json::Value& v) { return v.text; }
std::int64_t I(const json::Value& v) { return std::stoll(v.text); }
bool is_null(const json::Value& v) { return v.kind == json::Value::Null; }

struct Sha256 {
    std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<unsigned char> buf;
    std::uint64_t len = 0;

    static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void block(const unsigned char* p) {
        static const std::uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (std::uint32_t(p[4 * i]) << 24) | (std::uint32_t(p[4 * i + 1]) << 16) |
                   (std::uint32_t(p[4 * i + 2]) << 8) | std::uint32_t(p[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        auto v = h;
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(v[4], 6) ^ rotr(v[4], 11) ^ rotr(v[4], 25);
            const std::uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
            const std::uint32_t t1 = v[7] + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(v[0], 2) ^ rotr(v[0], 13) ^ rotr(v[0], 22);
            const std::uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
            const std::uint32_t t2 = s0 + maj;
            v = {t1 + t2, v[0], v[1], v[2], v[3] + t1, v[4], v[5], v[6]};
        }
        for (int i = 0; i < 8; ++i) h[i] += v[i];
    }

    static std::string hex(const void* data, std::size_t n) {
        Sha256 s;
        const auto* p = static_cast<const unsigned char*>(data);
        std::size_t i = 0;
        for (; i + 64 <= n; i += 64) s.block(p + i);
        std::vector<unsigned char> tail(p + i, p + n);
        const std::uint64_t bits = std::uint64_t(n) * 8;
        tail.push_back(0x80);
        while (tail.size() % 64 != 56) tail.push_back(0);
        for (int k = 7; k >= 0; --k) tail.push_back(static_cast<unsigned char>(bits >> (8 * k)));
        for (std::size_t j = 0; j < tail.size(); j += 64) s.block(tail.data() + j);
        std::string out;
        char b[9];
        for (auto x : s.h) {
            std::snprintf(b, sizeof b, "%08x", x);
            out += b;
        }
        return out;
    }
};

std::vector<unsigned char> base64_decode(const std::string& in) {
    static int table[256];
    static bool init = false;
    if (!init) {
        std::fill(std::begin(table), std::end(table), -1);
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(a[i])] = i;
        init = true;
    }
    std::vector<unsigned char> out;
    unsigned acc = 0;
    int bits = 0;
    for (char c : in) {
        const int v = table[static_cast<unsigned char>(c)];
        if (v < 0) continue;
        acc = (acc << 6) | static_cast<unsigned>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

std::string from_hex(const std::string& h) {
    std::string out;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) out.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return out;
}

bool read_line(gzFile f, std::string& line) {
    line.clear();
    char buf[1 << 16];
    while (gzgets(f, buf, sizeof buf)) {
        line += buf;
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
            return true;
        }
    }
    return !line.empty();
}

std::vector<std::int64_t> int_list(const json::Value& v) {
    std::vector<std::int64_t> out;
    for (const auto& x : v.items) out.push_back(I(x));
    return out;
}

std::string show_list(const std::vector<std::int64_t>& v) {
    std::ostringstream ss;
    ss << '[';
    for (std::size_t i = 0; i < v.size() && i < 40; ++i) ss << (i ? "," : "") << v[i];
    if (v.size() > 40) ss << ",...";
    ss << ']';
    return ss.str();
}

std::string show(const std::string& s) { return s.size() > 300 ? "\"" + s.substr(0, 300) + "\"..." : "\"" + s + "\""; }

struct Report {
    long show_limit = 20;
    long mismatches = 0;
    long records_bad = 0;
    bool record_bad = false;

    void fail(long index, const std::string& field, const std::string& want, const std::string& got) {
        ++mismatches;
        record_bad = true;
        if (mismatches <= show_limit)
            std::cerr << "MISMATCH record " << index << " " << field << "\n  want " << want << "\n  got  " << got << "\n";
    }

    void check(long index, const std::string& field, const std::string& want, const std::string& got) {
        if (want != got) fail(index, field, show(want), show(got));
    }

    void check(long index, const std::string& field, const std::vector<std::int64_t>& want,
               const std::vector<std::int64_t>& got) {
        if (want != got) fail(index, field, show_list(want), show_list(got));
    }

    void check(long index, const std::string& field, bool want, bool got) {
        if (want != got) fail(index, field, want ? "true" : "false", got ? "true" : "false");
    }
};

// The categories golden holds the category of every code point; the port
// reads three predicates of it: L*, Mn and So.
void check_unicode(long index, const json::Value& r, Report& rep) {
    if (const auto* cats = r.find("categories")) {
        const std::string& s = S(*cats);
        long bad = 0;
        for (char32_t cp = 0; cp <= 0x10FFFF; ++cp) {
            const std::string want = s.substr(2 * cp, 2);
            const bool ok = uni::is_alpha(cp) == (want[0] == 'L') && uni::is_nonspacing_mark(cp) == (want == "Mn") &&
                            uni::is_other_symbol(cp) == (want == "So");
            if (!ok && ++bad <= 5) rep.fail(index, "category U+" + std::to_string(cp), want, "another L*, Mn or So answer");
        }
        const std::string& ccc = S(r.at("combining"));
        for (char32_t cp = 0; cp <= 0x10FFFF; ++cp) {
            const int want = std::stoi(ccc.substr(2 * cp, 2), nullptr, 16);
            if (want != uni::combining(cp) && ++bad <= 10)
                rep.fail(index, "combining U+" + std::to_string(cp), std::to_string(want),
                         std::to_string(uni::combining(cp)));
        }
        return;
    }
    const uni::U32 text = uni::from_utf8(S(r.at("text")));
    rep.check(index, "nfd", S(r.at("nfd")), uni::to_utf8(uni::nfd(text)));
    rep.check(index, "nfc", S(r.at("nfc")), uni::to_utf8(uni::nfc(text)));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: ensemble_parity <golden.jsonl.gz> [--show N] [--tolerance X]\n";
        return 2;
    }
    Report rep;
    double tolerance = 1e-6;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--show")) rep.show_limit = std::stol(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--tolerance")) tolerance = std::stod(argv[i + 1]);
        else {
            std::cerr << "unknown option " << argv[i] << "\n";
            return 2;
        }
    }
    gzFile f = gzopen(argv[1], "rb");
    if (!f) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 2;
    }
    std::string line;
    if (!read_line(f, line)) {
        std::cerr << "empty golden file\n";
        return 2;
    }
    const json::Value header = json::loads(line);
    const std::string kind = S(header.at("kind"));
    const long expected = static_cast<long>(I(header.at("records")));

    const arbtok::ensemble::EnsembleDiacritizer* ens = nullptr;
    if (kind == "header") {
        ens = &arbtok::ensemble::get_ensemble();
        std::cout << "reference arbtok " << S(header.at("arbtok")) << ", onnxruntime " << S(header.at("onnxruntime"))
                  << ", numpy " << S(header.at("numpy")) << "; C++ onnxruntime "
                  << arbtok::ensemble::onnxruntime_version() << "\n";
        std::vector<std::string> want;
        for (const auto& c : header.at("classes").items) want.push_back(S(c));
        if (want != ens->classes()) rep.fail(0, "classes", "header classes", "a different class list");
        if (I(header.at("unk")) != ens->unk()) rep.fail(0, "unk", std::to_string(I(header.at("unk"))), std::to_string(ens->unk()));
    } else if (kind == "unicode-header") {
        if (S(header.at("unidata")) != uni::unidata_version())
            rep.fail(0, "unidata", S(header.at("unidata")), uni::unidata_version());
    } else {
        std::cerr << "unknown golden header kind " << kind << "\n";
        return 2;
    }

    long n = 0, logit_records = 0, identical = 0, hashed = 0;
    std::size_t logit_rows = 0;
    double max_diff = 0;
    while (read_line(f, line)) {
        ++n;
        rep.record_bad = false;
        const json::Value r = json::loads(line);
        if (kind == "unicode-header") {
            check_unicode(n, r, rep);
            if (rep.record_bad) ++rep.records_bad;
            continue;
        }
        const std::string& text = S(r.at("text"));
        std::string input = text;
        if (const auto* hex = r.find("bytes_hex")) {
            input = from_hex(S(*hex));
            rep.check(n, "decode_utf8", text, uni::to_utf8(uni::from_utf8(input)));
        }
        using arbtok::ensemble::EnsembleDiacritizer;
        rep.check(n, "normalize", S(r.at("normalized")), EnsembleDiacritizer::normalize(input));
        rep.check(n, "bare", S(r.at("bare")), EnsembleDiacritizer::bare(input));
        const std::string& bare = S(r.at("bare"));
        rep.check(n, "ids", int_list(r.at("ids")), ens->encode(bare));

        const arbtok::ensemble::Logits lg = ens->logits(input);
        if (is_null(r.at("argmax"))) {
            if (!lg.empty()) rep.fail(n, "logits", "None", std::to_string(lg.rows) + " rows");
        } else {
            const std::size_t want_rows = r.at("ids").items.size();
            if (lg.rows != want_rows || lg.cols != ens->classes().size()) {
                rep.fail(n, "logits shape", std::to_string(want_rows), std::to_string(lg.rows));
            } else {
                ++hashed;
                if (Sha256::hex(lg.values.data(), lg.values.size() * sizeof(float)) == S(r.at("logits_sha256")))
                    ++identical;
                if (const auto* b64 = r.find("logits"); b64 && !is_null(*b64)) {
                    const std::vector<unsigned char> raw = base64_decode(S(*b64));
                    if (raw.size() != lg.values.size() * sizeof(float)) {
                        rep.fail(n, "logits size", std::to_string(raw.size()), std::to_string(lg.values.size() * 4));
                    } else {
                        ++logit_records;
                        logit_rows += lg.rows;
                        double rec_max = 0;
                        for (std::size_t k = 0; k < lg.values.size(); ++k) {
                            float want;
                            std::memcpy(&want, raw.data() + 4 * k, 4);
                            const double d = std::fabs(static_cast<double>(want) - static_cast<double>(lg.values[k]));
                            if (!(d <= rec_max)) rec_max = d;
                        }
                        max_diff = std::max(max_diff, rec_max);
                        if (!(rec_max <= tolerance)) rep.fail(n, "logits", "max abs diff <= tolerance", std::to_string(rec_max));
                    }
                }
                const std::vector<std::int64_t> am = arbtok::ensemble::argmax(lg);
                rep.check(n, "argmax", int_list(r.at("argmax")), am);
                rep.check(n, "constrained", int_list(r.at("constrained")), ens->constrained_argmax(input, lg.bare, lg));
                rep.check(n, "decoded", S(r.at("decoded")), ens->decode(lg.bare, am));
            }
        }
        rep.check(n, "diacritized", S(r.at("diacritized")), ens->diacritize(input));
        rep.check(n, "has_arabic", r.at("has_arabic").boolean, arbtok::diacritize::has_arabic(input));
        rep.check(n, "strip_marks", S(r.at("strip_marks")), arbtok::diacritize::strip_marks(input));
        for (const auto& g : r.at("guards").items) {
            const std::string& p = S(g.items.at(0));
            rep.check(n, "skeleton_is_preserved " + show(p), g.items.at(1).boolean, arbtok::diacritize::skeleton_is_preserved(input, p));
            const std::optional<std::string> got = arbtok::diacritize::repair_skeleton(input, p);
            const json::Value& want = g.items.at(2);
            if (is_null(want) != !got.has_value() || (got && *got != S(want)))
                rep.fail(n, "repair_skeleton " + show(p), is_null(want) ? "None" : show(S(want)), got ? show(*got) : "None");
        }
        if (rep.record_bad) ++rep.records_bad;
    }
    gzclose(f);

    std::cout << "N in " << expected << ", N out " << n << ", records with a mismatch " << rep.records_bad
              << ", mismatches " << rep.mismatches << "\n";
    if (kind == "header") {
        std::cout << "logits compared: " << logit_records << " records, " << logit_rows << " rows, max abs diff "
                  << max_diff << " (tolerance " << tolerance << ")\n"
                  << "logits bit-identical (SHA-256): " << identical << " of " << hashed << " records\n";
    }
    if (n != expected) {
        std::cerr << "record count " << n << " differs from the header count " << expected << "\n";
        return 1;
    }
    return rep.mismatches ? 1 : 0;
}
