// Pure CSV renderers for the analysis tables.
#pragma once

#include <string>

#include "report.hpp"

namespace knot::core {

std::string render_costs_csv(const Analysis& analysis);
std::string render_hotspots_csv(const Analysis& analysis);
std::string render_recommendations_csv(const Analysis& analysis);

}  // namespace knot::core
