// Pure graph export to reviewable, deterministic text formats.
#pragma once

#include <string>

#include "report.hpp"

namespace knot::core {

std::string render_dot(const Analysis& analysis);
std::string render_mermaid(const Analysis& analysis);

}  // namespace knot::core
