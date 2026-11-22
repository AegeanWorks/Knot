#include "files.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace knot::shell {
namespace {

bool is_source(const std::string& path) {
    static const char* suffixes[] = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh", ".hxx"};
    for (const char* suffix : suffixes) {
        if (path.size() >= std::string(suffix).size() &&
            path.compare(path.size() - std::string(suffix).size(),
                         std::string(suffix).size(), suffix) == 0) {
            return true;
        }
    }
    return false;
}

bool is_skipped(const fs::path& path) {
    for (const auto& part : path) {
        std::string name = part.string();
        if (name == ".git" || name == "build" || name == "node_modules" ||
            name == ".aikit") {
            return true;
        }
    }
    return false;
}

bool read_file(const fs::path& path, std::string& text) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    text = buffer.str();
    return true;
}

std::string normalise(fs::path path) {
    std::string value = path.generic_string();
    while (value.size() > 2 && value.compare(0, 2, "./") == 0) value = value.substr(2);
    return value;
}

}  // namespace

bool load_sources(const std::string& root, std::vector<knot::core::SourceDoc>& out) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return false;

    std::vector<fs::path> paths;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec),
         end; it != end && !ec; it.increment(ec)) {
        if (is_skipped(it->path())) continue;
        if (!it->is_regular_file(ec)) continue;
        if (is_source(it->path().string())) paths.push_back(it->path());
    }
    if (ec) return false;
    std::sort(paths.begin(), paths.end());

    for (const auto& path : paths) {
        std::string text;
        if (!read_file(path, text)) return false;
        out.push_back(knot::core::SourceDoc{normalise(path), text});
    }
    return true;
}

bool read_text_file(const std::string& path, std::string& text) {
    return read_file(fs::path(path), text);
}

std::vector<knot::core::SourceDoc> demo_docs() {
    return {
        {"app.cpp",
         "#include \"widget.hpp\"\n"
         "#include \"widget.hpp\"  // duplicate\n"
         "#include <vector>\n"
         "\n"
         "int main() { Widget* w = nullptr; (void)w; }\n"},
        {"widget.hpp",
         "#pragma once\n"
         "#include \"theme.hpp\"\n"
         "struct Widget { void paint(); };\n"},
        {"theme.hpp",
         "#pragma once\n"
         "#include \"widget.hpp\"  // cycle back to widget\n"
         "// Theme text: \"struct Widget\" inside a string must be ignored.\n"
         "struct Theme { int level; };\n"},
    };
}

bool write_stdout(const std::string& text) {
    std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(std::cout);
}

bool write_stderr(const std::string& text) {
    std::cerr.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(std::cerr);
}

}  // namespace knot::shell
