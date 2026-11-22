#include "policy.hpp"

#include <algorithm>

namespace knot::core {
namespace {

const char* kind_name(ViolationKind kind) {
    switch (kind) {
        case ViolationKind::Cycle: return "cycle";
        case ViolationKind::Cost: return "cost";
        case ViolationKind::Hotspot: return "hotspot";
        case ViolationKind::Regression: return "regression";
    }
    return "unknown";
}

bool parse_bool(const std::string& text, bool& out) {
    if (text == "true") {
        out = true;
        return true;
    }
    if (text == "false") {
        out = false;
        return true;
    }
    return false;
}

bool parse_size(const std::string& text, std::size_t& out) {
    if (text.empty()) return false;
    std::size_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<std::size_t>(c - '0');
        if (value > 100000) return false;
    }
    out = value;
    return true;
}

}  // namespace

Policy parse_policy(const std::string& text) {
    Policy policy;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos
                                                                       : end - start);
        start = end == std::string::npos ? text.size() + 1 : end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t')) ++first;
        std::size_t last = line.size();
        while (last > first && (line[last - 1] == ' ' || line[last - 1] == '\t')) --last;
        line = line.substr(first, last - first);
        if (line.empty() || line[0] == '#') continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!value.empty() && value[0] == ' ') value.erase(0, 1);
        if (key == "fail_on_cycle") {
            bool parsed = false;
            if (parse_bool(value, parsed)) policy.fail_on_cycle = parsed;
        } else if (key == "max_cost") {
            std::size_t parsed = 0;
            if (parse_size(value, parsed)) policy.max_cost = parsed;
        } else if (key == "max_hotspot") {
            std::size_t parsed = 0;
            if (parse_size(value, parsed)) policy.max_hotspot = parsed;
        } else if (key == "baseline") {
            policy.baseline = value;
        }
    }
    return policy;
}

std::vector<Violation> evaluate_policy(const Analysis& analysis, const Policy& policy,
                                       const DiffReport& diff) {
    std::vector<Violation> violations;
    if (policy.fail_on_cycle) {
        for (const auto& cycle : analysis.cycles) {
            Violation violation;
            violation.kind = ViolationKind::Cycle;
            for (std::size_t i = 0; i < cycle.nodes.size(); ++i) {
                if (i) violation.subject += " -> ";
                violation.subject += cycle.nodes[i];
            }
            violations.push_back(violation);
        }
    }
    if (policy.max_cost != 0) {
        for (const auto& cost : analysis.costs) {
            if (cost.transitive <= policy.max_cost) continue;
            Violation violation;
            violation.kind = ViolationKind::Cost;
            violation.subject = cost.node;
            violation.value = static_cast<long long>(cost.transitive);
            violation.limit = static_cast<long long>(policy.max_cost);
            violations.push_back(violation);
        }
    }
    if (policy.max_hotspot != 0) {
        for (const auto& hot : analysis.hotspots) {
            if (hot.included_by <= policy.max_hotspot) continue;
            Violation violation;
            violation.kind = ViolationKind::Hotspot;
            violation.subject = hot.header;
            violation.value = static_cast<long long>(hot.included_by);
            violation.limit = static_cast<long long>(policy.max_hotspot);
            violations.push_back(violation);
        }
    }
    if (diff.regressions) {
        std::size_t count = 0;
        for (const auto& entry : diff.entries) {
            if (entry.kind == DiffEntry::Kind::CycleAdded ||
                entry.kind == DiffEntry::Kind::CostUp ||
                entry.kind == DiffEntry::Kind::HotspotUp) {
                ++count;
            }
        }
        Violation violation;
        violation.kind = ViolationKind::Regression;
        violation.subject = "baseline regression";
        violation.value = static_cast<long long>(count);
        violations.push_back(violation);
    }
    std::sort(violations.begin(), violations.end(),
              [](const Violation& a, const Violation& b) {
                  std::string ka = kind_name(a.kind);
                  std::string kb = kind_name(b.kind);
                  if (ka != kb) return ka < kb;
                  return a.subject < b.subject;
              });
    return violations;
}

std::string render_violations_text(const std::vector<Violation>& violations) {
    std::ostringstream os;
    os << "Knot check: " << violations.size() << " violation(s)\n";
    for (const auto& violation : violations) {
        os << "  [" << kind_name(violation.kind) << "] " << violation.subject;
        if (violation.kind == ViolationKind::Cost ||
            violation.kind == ViolationKind::Hotspot) {
            os << " : " << violation.value << " > " << violation.limit;
        }
        os << "\n";
    }
    return os.str();
}

std::string render_violations_json(const std::vector<Violation>& violations) {
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"violations\": " << violations.size()
       << ",\n  \"entries\": [";
    for (std::size_t i = 0; i < violations.size(); ++i) {
        const auto& violation = violations[i];
        if (i) os << ",";
        os << "\n    {\"kind\": \"" << kind_name(violation.kind) << "\", \"subject\": \"";
        for (char c : violation.subject) {
            if (c == '"' || c == '\\') os << '\\' << c;
            else os << c;
        }
        os << "\", \"value\": " << violation.value << ", \"limit\": " << violation.limit << "}";
    }
    if (!violations.empty()) os << "\n  ";
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
