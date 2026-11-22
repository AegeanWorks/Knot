// Advisory missing-include detection. Pure, reusing the identifier scanner.
#pragma once

#include <vector>

#include "report.hpp"
#include "types.hpp"

namespace knot::core {

std::vector<Recommendation> missing_includes(const Analysis& analysis);

}  // namespace knot::core
