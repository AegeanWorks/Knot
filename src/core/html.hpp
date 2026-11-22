// Pure self-contained HTML report renderer.
#pragma once

#include <string>

#include "report.hpp"

namespace knot::core {

std::string render_html(const Analysis& analysis);

}  // namespace knot::core
