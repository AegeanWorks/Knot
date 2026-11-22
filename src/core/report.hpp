// Pure composition and deterministic rendering of an analysis.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "types.hpp"

namespace knot::core {

struct Analysis {
    std::vector<SourceDoc> docs;
    std::vector<IncludeEdge> edges;
    IncludeGraph graph;
    std::vector<Cycle> cycles;
    std::vector<Cost> costs;
    std::vector<Hotspot> hotspots;
    std::vector<Recommendation> recommendations;
};

// Pure: documents in, immutable analysis out.
Analysis analyze(std::vector<SourceDoc> docs);

std::string render_text(const Analysis& a, std::size_t top);
std::string render_json(const Analysis& a, std::size_t top);

// A complete snapshot: no top-N truncation, safe to use as a CI baseline.
std::string render_baseline_json(const Analysis& a);

}  // namespace knot::core
