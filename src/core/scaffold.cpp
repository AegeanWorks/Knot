#include "scaffold.hpp"

namespace knot::core {

std::string default_ignore_text() {
    return "# Knot ignore rules. Patterns: * (no /), ** (crosses /), ?, ! to re-include.\n"
           "build/\n"
           "third_party/**\n"
           "generated/\n"
           "*.pb.h\n";
}

std::string default_policy_text() {
    return "# Knot CI policy for this repository.\n"
           "fail_on_cycle = true\n"
           "max_cost = 0\n"
           "max_hotspot = 0\n"
           "# baseline = knot-baseline.json\n";
}

}  // namespace knot::core
