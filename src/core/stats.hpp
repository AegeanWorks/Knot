// Pure project-level metrics, including a deterministic deepest include chain.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "report.hpp"

namespace knot::core {

struct Stats {
    std::size_t files = 0;
    std::size_t translation_units = 0;
    std::size_t headers = 0;
    std::size_t edges = 0;
    std::size_t internal_edges = 0;
    std::size_t external_edges = 0;
    std::size_t max_depth = 0;
    std::vector<std::string> deepest_chain;
};

Stats compute_stats(const Analysis& analysis);
std::string render_stats_text(const Stats& stats);
std::string render_stats_json(const Stats& stats);

}  // namespace knot::core
