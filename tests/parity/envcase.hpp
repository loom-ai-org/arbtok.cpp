// The env records of tools/goldens/envcase.py: a call run under a fresh
// root directory with its own environment variables, files and directories.
// "<ROOT>" stands for the root in the case and in the result, and HOME is
// <ROOT>/home. The call runs in this process with the working directory set
// to the root, and the environment and directory are restored after it.
#pragma once

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "common.hpp"

namespace parity {

inline std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    for (size_t at = 0; (at = s.find(from, at)) != std::string::npos; at += to.size()) s.replace(at, from.size(), to);
    return s;
}

inline const Value* member(const Value& object, const std::string& key) {
    for (const auto& [k, v] : object.members)
        if (k == key) return &v;
    return nullptr;
}

// run_env_case(case, call): call receives the root and returns the result
// JSON with the root in it; the result comes back with "<ROOT>" in its place.
inline std::string run_env_case(const Value& c, const std::function<std::string(const std::string&)>& call) {
    namespace fs = std::filesystem;
    std::string tmpl = (fs::temp_directory_path() / "arbtok-env-XXXXXX").string();
    if (::mkdtemp(tmpl.data()) == nullptr) throw std::runtime_error("mkdtemp failed");
    const std::string root = fs::canonical(tmpl).string();
    fs::create_directories(root + "/home");
    if (const Value* dirs = member(c, "dirs"))
        for (const auto& d : dirs->items) fs::create_directories(root + "/" + d.str);
    if (const Value* files = member(c, "files"))
        for (const auto& [rel, hexdata] : files->members) {
            const fs::path path = root + "/" + rel;
            fs::create_directories(path.parent_path());
            std::string bytes;
            for (size_t i = 0; i + 1 < hexdata.str.size(); i += 2)
                bytes.push_back(static_cast<char>(std::stoi(hexdata.str.substr(i, 2), nullptr, 16)));
            std::ofstream(path, std::ios::binary) << bytes;
        }
    static const char* cleared[] = {"XDG_CACHE_HOME", "HF_HOME", "HF_HUB_CACHE", "HUGGINGFACE_HUB_CACHE",
                                    "HF_HUB_OFFLINE"};
    std::map<std::string, std::optional<std::string>> saved;
    auto save = [&](const std::string& name) {
        if (saved.count(name)) return;
        const char* v = std::getenv(name.c_str());
        saved[name] = v ? std::optional<std::string>(v) : std::nullopt;
    };
    for (const char* name : cleared) {
        save(name);
        ::unsetenv(name);
    }
    save("HOME");
    ::setenv("HOME", (root + "/home").c_str(), 1);
    if (const Value* env = member(c, "env"))
        for (const auto& [k, v] : env->members) {
            save(k);
            ::setenv(k.c_str(), replace_all(v.str, "<ROOT>", root).c_str(), 1);
        }
    const fs::path cwd = fs::current_path();
    fs::current_path(root);
    std::string result;
    try {
        result = replace_all(call(root), root, "<ROOT>");
    } catch (const arbtok::Error& e) {
        result = "{\"err\":" + J(e.python_type());
        const Value* message = member(c, "message");
        if (message && message->boolean) result += ",\"message\":" + J(replace_all(e.message(), root, "<ROOT>"));
        result += "}";
    }
    fs::current_path(cwd);
    for (const auto& [name, value] : saved) {
        if (value) ::setenv(name.c_str(), value->c_str(), 1);
        else ::unsetenv(name.c_str());
    }
    fs::remove_all(root);
    return result;
}

}  // namespace parity
