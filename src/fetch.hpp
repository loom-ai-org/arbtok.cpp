// The body of an http(s) URL, fetched with curl as orthography2ipa.cpp
// fetches its remote lexicons. OSError when curl fails.
#pragma once

#include <string>

namespace arbtok::detail {

std::string fetch_url(const std::string& url);

// Writes data to path through a temporary file in the same directory.
void write_file_atomic(const std::string& path, const std::string& data);

}  // namespace arbtok::detail
