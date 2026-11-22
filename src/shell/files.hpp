// The only I/O boundary in Knot. Everything above this line is pure.
#pragma once

#include <string>
#include <vector>

#include "core/types.hpp"

namespace knot::shell {

// Recursively load C/C++ source and header files under `root`, sorted by path.
// Returns false when the root cannot be read.
bool load_sources(const std::string& root, std::vector<knot::core::SourceDoc>& out);

// A built-in fixture used by `knot demo`; no file system access.
std::vector<knot::core::SourceDoc> demo_docs();

bool write_stdout(const std::string& text);
bool write_stderr(const std::string& text);

}  // namespace knot::shell
