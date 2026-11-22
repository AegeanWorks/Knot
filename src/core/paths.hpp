// Pure path qualification for multi-root scans.
#pragma once

#include <string>

namespace knot::core {

// With one root the relative path is unchanged; with several roots it is
// prefixed by the root's directory name so two `src/a.cpp` files stay distinct.
std::string qualify_path(const std::string& root, const std::string& relative,
                         bool multiple_roots, const std::string& prefix_override = "");

}  // namespace knot::core
