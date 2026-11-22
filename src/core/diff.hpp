// Pure golden-baseline snapshots and deterministic difference reports.
#pragma once

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "json.hpp"
#include "report.hpp"
#include "types.hpp"

namespace knot::core {

struct Snapshot {
    std::vector<std::vector<std::string>> cycles;
    std::map<std::string, long long> costs;
    std::map<std::string, long long> hotspots;
    std::vector<Recommendation> recommendations;
};

struct DiffEntry {
    enum class Kind {
        CycleAdded, CycleRemoved, CostUp, CostDown,
        HotspotUp, HotspotDown, RecAdded, RecRemoved
    };

    Kind kind = Kind::CostUp;
    std::string subject;
    long long before = 0;
    long long after = 0;
    std::string detail;
};

struct DiffReport {
    std::vector<DiffEntry> entries;
    bool regressions = false;
};

Snapshot snapshot_from_analysis(const Analysis& analysis);
// Pure: JSON text in, Snapshot out. Returns false with a reason on bad input.
bool snapshot_from_json(const std::string& text, Snapshot& out, std::string& error);
DiffReport diff_snapshots(const Snapshot& before, const Snapshot& after);

std::string render_diff_text(const DiffReport& diff);
std::string render_diff_json(const DiffReport& diff);

}  // namespace knot::core
