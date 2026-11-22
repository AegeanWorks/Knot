// Pure CI policy model: parse a small key=value file and evaluate violations.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "diff.hpp"
#include "report.hpp"

namespace knot::core {

struct Policy {
    bool fail_on_cycle = true;
    std::size_t max_cost = 0;     // 0 means unlimited
    std::size_t max_hotspot = 0;  // 0 means unlimited
    std::string baseline;         // relative to the scanned directory
};

enum class ViolationKind { Cycle, Cost, Hotspot, Regression };

struct Violation {
    ViolationKind kind = ViolationKind::Cycle;
    std::string subject;
    long long value = 0;
    long long limit = 0;
};

Policy parse_policy(const std::string& text);
std::vector<Violation> evaluate_policy(const Analysis& analysis, const Policy& policy,
                                       const DiffReport& diff);
std::string render_violations_text(const std::vector<Violation>& violations);
std::string render_violations_json(const std::vector<Violation>& violations);

}  // namespace knot::core
