#include "diff.hpp"

#include <algorithm>
#include <set>

namespace knot::core {
namespace {

std::string kind_name(DiffEntry::Kind kind) {
    switch (kind) {
        case DiffEntry::Kind::CycleAdded: return "cycle-added";
        case DiffEntry::Kind::CycleRemoved: return "cycle-removed";
        case DiffEntry::Kind::CostUp: return "cost-up";
        case DiffEntry::Kind::CostDown: return "cost-down";
        case DiffEntry::Kind::HotspotUp: return "hotspot-up";
        case DiffEntry::Kind::HotspotDown: return "hotspot-down";
        case DiffEntry::Kind::RecAdded: return "rec-added";
        case DiffEntry::Kind::RecRemoved: return "rec-removed";
    }
    return "unknown";
}

std::string rec_key(const Recommendation& r) {
    std::string kind;
    switch (r.kind) {
        case Recommendation::Kind::BreakCycle: kind = "break-cycle"; break;
        case Recommendation::Kind::RemoveDuplicate: kind = "remove-duplicate"; break;
        case Recommendation::Kind::ForwardDeclare: kind = "forward-declare"; break;
    }
    return kind + "|" + r.file + "|" + r.header;
}

long long number_or(const JsonValue* value, long long fallback) {
    return value != nullptr && value->type == JsonType::Number ? value->number : fallback;
}

std::string string_or(const JsonValue* value, const std::string& fallback) {
    return value != nullptr && value->type == JsonType::String ? value->text : fallback;
}

bool read_long_array(const JsonValue* array, std::vector<std::string>& out) {
    if (array == nullptr || array->type != JsonType::Array) return false;
    for (const auto& item : array->array) {
        if (item.type != JsonType::String) return false;
        out.push_back(item.text);
    }
    return true;
}

}  // namespace

Snapshot snapshot_from_analysis(const Analysis& a) {
    Snapshot s;
    for (const auto& cycle : a.cycles) s.cycles.push_back(cycle.nodes);
    for (const auto& cost : a.costs) s.costs[cost.node] = static_cast<long long>(cost.transitive);
    for (const auto& hot : a.hotspots) s.hotspots[hot.header] = static_cast<long long>(hot.included_by);
    s.recommendations = a.recommendations;
    return s;
}

bool snapshot_from_json(const std::string& text, Snapshot& out, std::string& error) {
    JsonResult parsed = parse_json(text);
    if (!parsed.ok) {
        error = parsed.error;
        return false;
    }
    if (parsed.value.type != JsonType::Object) {
        error = "baseline must be a JSON object";
        return false;
    }
    const JsonValue* cycles = json_member(parsed.value, "cycles");
    if (cycles != nullptr && cycles->type == JsonType::Array) {
        for (const auto& item : cycles->array) {
            std::vector<std::string> nodes;
            if (!read_long_array(json_member(item, "nodes"), nodes)) {
                error = "baseline cycle has no string nodes";
                return false;
            }
            out.cycles.push_back(nodes);
        }
    }
    const JsonValue* costs = json_member(parsed.value, "costs");
    if (costs != nullptr && costs->type == JsonType::Array) {
        for (const auto& item : costs->array) {
            std::string node = string_or(json_member(item, "node"), "");
            if (node.empty()) continue;
            out.costs[node] = number_or(json_member(item, "transitive"), 0);
        }
    }
    const JsonValue* hotspots = json_member(parsed.value, "hotspots");
    if (hotspots != nullptr && hotspots->type == JsonType::Array) {
        for (const auto& item : hotspots->array) {
            std::string header = string_or(json_member(item, "header"), "");
            if (header.empty()) continue;
            out.hotspots[header] = number_or(json_member(item, "included_by"), 0);
        }
    }
    const JsonValue* recs = json_member(parsed.value, "recommendations");
    if (recs != nullptr && recs->type == JsonType::Array) {
        for (const auto& item : recs->array) {
            Recommendation r;
            std::string kind = string_or(json_member(item, "kind"), "");
            if (kind == "break-cycle") r.kind = Recommendation::Kind::BreakCycle;
            else if (kind == "forward-declare") r.kind = Recommendation::Kind::ForwardDeclare;
            else r.kind = Recommendation::Kind::RemoveDuplicate;
            r.file = string_or(json_member(item, "file"), "");
            r.header = string_or(json_member(item, "header"), "");
            r.reason = string_or(json_member(item, "reason"), "");
            out.recommendations.push_back(r);
        }
    }
    return true;
}

DiffReport diff_snapshots(const Snapshot& before, const Snapshot& after) {
    DiffReport report;
    std::set<std::vector<std::string>> old_cycles, new_cycles;
    for (const auto& c : before.cycles) old_cycles.insert(c);
    for (const auto& c : after.cycles) new_cycles.insert(c);

    for (const auto& c : new_cycles) {
        if (old_cycles.count(c)) continue;
        DiffEntry entry;
        entry.kind = DiffEntry::Kind::CycleAdded;
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (i) entry.subject += " -> ";
            entry.subject += c[i];
        }
        report.entries.push_back(entry);
    }
    for (const auto& c : old_cycles) {
        if (new_cycles.count(c)) continue;
        DiffEntry entry;
        entry.kind = DiffEntry::Kind::CycleRemoved;
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (i) entry.subject += " -> ";
            entry.subject += c[i];
        }
        report.entries.push_back(entry);
    }

    std::set<std::string> cost_keys;
    for (const auto& [key, value] : before.costs) {
        (void)value;
        cost_keys.insert(key);
    }
    for (const auto& [key, value] : after.costs) {
        (void)value;
        cost_keys.insert(key);
    }
    for (const auto& key : cost_keys) {
        long long b = 0;
        long long a = 0;
        auto itb = before.costs.find(key);
        auto ita = after.costs.find(key);
        if (itb != before.costs.end()) b = itb->second;
        if (ita != after.costs.end()) a = ita->second;
        if (a == b) continue;
        DiffEntry entry;
        entry.kind = a > b ? DiffEntry::Kind::CostUp : DiffEntry::Kind::CostDown;
        entry.subject = key;
        entry.before = b;
        entry.after = a;
        report.entries.push_back(entry);
    }

    std::set<std::string> hot_keys;
    for (const auto& [key, value] : before.hotspots) {
        (void)value;
        hot_keys.insert(key);
    }
    for (const auto& [key, value] : after.hotspots) {
        (void)value;
        hot_keys.insert(key);
    }
    for (const auto& key : hot_keys) {
        long long b = 0;
        long long a = 0;
        auto itb = before.hotspots.find(key);
        auto ita = after.hotspots.find(key);
        if (itb != before.hotspots.end()) b = itb->second;
        if (ita != after.hotspots.end()) a = ita->second;
        if (a == b) continue;
        DiffEntry entry;
        entry.kind = a > b ? DiffEntry::Kind::HotspotUp : DiffEntry::Kind::HotspotDown;
        entry.subject = key;
        entry.before = b;
        entry.after = a;
        report.entries.push_back(entry);
    }

    std::set<std::string> old_recs, new_recs;
    for (const auto& r : before.recommendations) old_recs.insert(rec_key(r));
    for (const auto& r : after.recommendations) new_recs.insert(rec_key(r));
    for (const auto& key : new_recs) {
        if (old_recs.count(key)) continue;
        DiffEntry entry;
        entry.kind = DiffEntry::Kind::RecAdded;
        entry.subject = key;
        report.entries.push_back(entry);
    }
    for (const auto& key : old_recs) {
        if (new_recs.count(key)) continue;
        DiffEntry entry;
        entry.kind = DiffEntry::Kind::RecRemoved;
        entry.subject = key;
        report.entries.push_back(entry);
    }

    for (const auto& entry : report.entries) {
        if (entry.kind == DiffEntry::Kind::CycleAdded ||
            entry.kind == DiffEntry::Kind::CostUp ||
            entry.kind == DiffEntry::Kind::HotspotUp) {
            report.regressions = true;
        }
    }
    std::sort(report.entries.begin(), report.entries.end(),
              [](const DiffEntry& a, const DiffEntry& b) {
                  std::string ka = kind_name(a.kind);
                  std::string kb = kind_name(b.kind);
                  if (ka != kb) return ka < kb;
                  if (a.subject != b.subject) return a.subject < b.subject;
                  if (a.before != b.before) return a.before < b.before;
                  return a.after < b.after;
              });
    return report;
}

std::string render_diff_text(const DiffReport& diff) {
    std::ostringstream os;
    os << "Knot diff: " << diff.entries.size() << " change(s), "
       << (diff.regressions ? "REGRESSION" : "no regression") << "\n";
    for (const auto& entry : diff.entries) {
        os << "  [" << kind_name(entry.kind) << "] " << entry.subject;
        if (entry.kind == DiffEntry::Kind::CostUp || entry.kind == DiffEntry::Kind::CostDown ||
            entry.kind == DiffEntry::Kind::HotspotUp || entry.kind == DiffEntry::Kind::HotspotDown) {
            os << " : " << entry.before << " -> " << entry.after;
        }
        os << "\n";
    }
    return os.str();
}

std::string render_diff_json(const DiffReport& diff) {
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"changes\": " << diff.entries.size()
       << ",\n  \"regression\": " << (diff.regressions ? "true" : "false")
       << ",\n  \"entries\": [";
    for (std::size_t i = 0; i < diff.entries.size(); ++i) {
        const auto& entry = diff.entries[i];
        if (i) os << ",";
        os << "\n    {\"kind\": \"" << kind_name(entry.kind) << "\", \"subject\": \"";
        for (char c : entry.subject) {
            if (c == '"' || c == '\\') os << '\\' << c;
            else os << c;
        }
        os << "\", \"before\": " << entry.before << ", \"after\": " << entry.after << "}";
    }
    if (!diff.entries.empty()) os << "\n  ";
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
