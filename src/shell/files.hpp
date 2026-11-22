// The only I/O boundary in Knot. Everything above this line is pure.
#pragma once

#include <string>
#include <vector>

#include "core/ignore.hpp"
#include "core/types.hpp"

namespace knot::shell {

// Recursively load C/C++ source and header files under `root`, sorted by path.
// Returns false when the root cannot be read.
bool load_sources(const std::string& root, const knot::core::IgnoreRules& rules,
                  std::vector<knot::core::SourceDoc>& out);

// Read a baseline report from disk. The only file read outside load_sources.
bool read_text_file(const std::string& path, std::string& text);
bool file_exists(const std::string& path);

// A built-in fixture used by `knot demo`; no file system access.
std::vector<knot::core::SourceDoc> demo_docs();

bool write_stdout(const std::string& text);
bool write_stderr(const std::string& text);

}  // namespace knot::shell
