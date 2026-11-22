#include "csv.hpp"

#include <sstream>

namespace knot::core {
namespace {

std::string field(const std::string& value) {
    bool needs_quotes = false;
    for (char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return value;
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out += '"';
    return out;
}

const char* kind_name(Recommendation::Kind kind) {
    switch (kind) {
        case Recommendation::Kind::BreakCycle: return "break-cycle";
        case Recommendation::Kind::RemoveDuplicate: return "remove-duplicate";
        case Recommendation::Kind::ForwardDeclare: return "forward-declare";
        case Recommendation::Kind::AddInclude: return "add-include";
    }
    return "unknown";
}

}  // namespace

std::string render_costs_csv(const Analysis& analysis) {
    std::ostringstream os;
    os << "node,direct,transitive\n";
    for (const auto& cost : analysis.costs) {
        os << field(cost.node) << ',' << cost.direct << ',' << cost.transitive << '\n';
    }
    return os.str();
}

std::string render_hotspots_csv(const Analysis& analysis) {
    std::ostringstream os;
    os << "header,included_by\n";
    for (const auto& hot : analysis.hotspots) {
        os << field(hot.header) << ',' << hot.included_by << '\n';
    }
    return os.str();
}

std::string render_recommendations_csv(const Analysis& analysis) {
    std::ostringstream os;
    os << "kind,file,header,reason\n";
    for (const auto& rec : analysis.recommendations) {
        os << field(kind_name(rec.kind)) << ',' << field(rec.file) << ','
           << field(rec.header) << ',' << field(rec.reason) << '\n';
    }
    return os.str();
}

}  // namespace knot::core
