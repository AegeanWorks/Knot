#include "files.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>

#include "core/paths.hpp"

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
        if (name == ".git" || name == ".aikit") {
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

}  // namespace

bool file_exists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool write_text_file(const std::string& path, const std::string& text) {
    std::ofstream out(fs::path(path), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(out);
}

bool load_sources(const std::vector<std::string>& roots,
                  const knot::core::IgnoreRules& rules,
                  std::vector<knot::core::SourceDoc>& out) {
    if (roots.empty()) return false;
    const bool multiple = roots.size() > 1;
    std::vector<std::string> overrides(roots.size());
    if (multiple) {
        for (std::size_t i = 0; i < roots.size(); ++i) {
            for (std::size_t j = i + 1; j < roots.size(); ++j) {
                if (knot::core::qualify_path(roots[i], "", true) ==
                    knot::core::qualify_path(roots[j], "", true)) {
                    overrides[i] = roots[i];
                    overrides[j] = roots[j];
                }
            }
        }
        for (std::string& override : overrides) {
            for (char& c : override) if (c == '\\') c = '/';
            while (override.size() > 1 && override.back() == '/') override.pop_back();
        }
    }
    std::vector<std::pair<fs::path, std::string>> entries;

    for (std::size_t root_index = 0; root_index < roots.size(); ++root_index) {
        const std::string& root = roots[root_index];
        std::error_code ec;
        if (!fs::is_directory(root, ec)) return false;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec),
             end; it != end && !ec; it.increment(ec)) {
            if (is_skipped(it->path())) continue;
            if (!it->is_regular_file(ec)) continue;
            if (!is_source(it->path().string())) continue;
            std::string relative = fs::relative(it->path(), root, ec).generic_string();
            if (ec) return false;
            if (knot::core::is_ignored(rules, relative)) continue;
            entries.push_back({it->path(),
                               knot::core::qualify_path(root, relative, multiple,
                                                        overrides[root_index])});
        }
        if (ec) return false;
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    for (const auto& entry : entries) {
        std::string text;
        if (!read_file(entry.first, text)) return false;
        out.push_back(knot::core::SourceDoc{entry.second, text});
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
