#include "report.hpp"

#include <algorithm>
#include <sstream>

#include "graph.hpp"
#include "lexer.hpp"

namespace knot::core {
namespace {

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    static const char* hex = "0123456789abcdef";
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

std::string quoted(const std::string& value) { return "\"" + json_escape(value) + "\""; }

const char* kind_name(Recommendation::Kind kind) {
    switch (kind) {
        case Recommendation::Kind::BreakCycle: return "break-cycle";
        case Recommendation::Kind::RemoveDuplicate: return "remove-duplicate";
        case Recommendation::Kind::ForwardDeclare: return "forward-declare";
    }
    return "unknown";
}

}  // namespace

Analysis analyze(std::vector<SourceDoc> docs) {
    std::sort(docs.begin(), docs.end(), [](const SourceDoc& a, const SourceDoc& b) {
        return a.path < b.path;
    });
    Analysis a;
    a.docs = docs;
    for (const auto& doc : docs) {
        std::vector<IncludeEdge> found = scan_includes(doc.text, doc.path);
        a.edges.insert(a.edges.end(), found.begin(), found.end());
    }
    a.graph = build_graph(a.edges);
    a.cycles = find_cycles(a.graph);
    a.costs = transitive_costs(a.graph);
    a.hotspots = fan_in(a.graph);
    a.recommendations = recommendations(a.graph, a.edges, docs, a.cycles);
    return a;
}

std::string render_text(const Analysis& a, std::size_t top) {
    std::ostringstream os;
    os << "Knot: " << a.docs.size() << " file(s), " << a.edges.size() << " include(s), "
       << a.graph.ids.size() << " node(s)\n";

    os << "Cycles: " << a.cycles.size() << "\n";
    for (const auto& cycle : a.cycles) {
        for (std::size_t i = 0; i < cycle.nodes.size(); ++i) {
            if (i) os << " -> ";
            os << cycle.nodes[i];
        }
        os << "\n";
    }

    os << "Transitive cost (top " << top << "):\n";
    for (std::size_t i = 0; i < std::min(top, a.costs.size()); ++i) {
        os << "  " << a.costs[i].transitive << "\t" << a.costs[i].node
           << " (direct " << a.costs[i].direct << ")\n";
    }

    os << "Fan-in (top " << top << "):\n";
    for (std::size_t i = 0; i < std::min(top, a.hotspots.size()); ++i) {
        os << "  " << a.hotspots[i].included_by << "\t" << a.hotspots[i].header << "\n";
    }

    os << "Recommendations (" << a.recommendations.size() << "):\n";
    for (const auto& rec : a.recommendations) {
        os << "  [" << kind_name(rec.kind) << "] " << rec.file << " -> "
           << rec.header << " : " << rec.reason << "\n";
    }
    return os.str();
}

std::string render_json(const Analysis& a, std::size_t top) {
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"files\": " << a.docs.size()
       << ",\n  \"includes\": " << a.edges.size()
       << ",\n  \"nodes\": " << a.graph.ids.size() << ",\n  \"cycles\": [";
    for (std::size_t i = 0; i < a.cycles.size(); ++i) {
        if (i) os << ",";
        os << "\n    {\"nodes\": [";
        for (std::size_t j = 0; j < a.cycles[i].nodes.size(); ++j) {
            if (j) os << ", ";
            os << quoted(a.cycles[i].nodes[j]);
        }
        os << "]}";
    }
    if (!a.cycles.empty()) os << "\n  ";
    os << "],\n  \"costs\": [";
    for (std::size_t i = 0; i < std::min(top, a.costs.size()); ++i) {
        if (i) os << ",";
        os << "\n    {\"node\": " << quoted(a.costs[i].node)
           << ", \"direct\": " << a.costs[i].direct
           << ", \"transitive\": " << a.costs[i].transitive << "}";
    }
    if (!a.costs.empty() && top > 0) os << "\n  ";
    os << "],\n  \"hotspots\": [";
    for (std::size_t i = 0; i < std::min(top, a.hotspots.size()); ++i) {
        if (i) os << ",";
        os << "\n    {\"header\": " << quoted(a.hotspots[i].header)
           << ", \"included_by\": " << a.hotspots[i].included_by << "}";
    }
    if (!a.hotspots.empty() && top > 0) os << "\n  ";
    os << "],\n  \"recommendations\": [";
    for (std::size_t i = 0; i < a.recommendations.size(); ++i) {
        const auto& rec = a.recommendations[i];
        if (i) os << ",";
        os << "\n    {\"kind\": " << quoted(kind_name(rec.kind))
           << ", \"file\": " << quoted(rec.file)
           << ", \"header\": " << quoted(rec.header)
           << ", \"reason\": " << quoted(rec.reason) << "}";
    }
    if (!a.recommendations.empty()) os << "\n  ";
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
