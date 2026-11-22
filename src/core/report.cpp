#include "report.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "graph.hpp"
#include "lexer.hpp"

namespace knot::core {
namespace {

std::string folded(std::string value) {
    for (char& c : value) {
        if (c == '\\') c = '/';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool ends_with_segment(const std::string& path, const std::string& suffix) {
    return path.size() > suffix.size() &&
           path[path.size() - suffix.size() - 1] == '/' &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Map a header as written to a scanned document when possible. Absolute roots
// otherwise create two nodes for one file (the source path and the include
// string), which would hide cycles and break path queries. Pure and total.
std::string resolve_header(const std::string& from, const std::string& header,
                           const std::vector<std::string>& paths) {
    std::string wanted = folded(header);
    std::size_t slash = folded(from).find_last_of('/');
    std::string sibling = slash == std::string::npos ? wanted : folded(from).substr(0, slash + 1) + wanted;
    for (const auto& path : paths) {
        if (folded(path) == sibling) return path;
    }
    std::string unique;
    for (const auto& path : paths) {
        std::string candidate = folded(path);
        if (candidate == wanted || ends_with_segment(candidate, wanted)) {
            if (!unique.empty()) return header;  // ambiguous: keep the literal include
            unique = path;
        }
    }
    return unique.empty() ? header : unique;
}

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
    std::vector<std::string> paths;
    for (const auto& doc : docs) paths.push_back(doc.path);
    for (const auto& doc : docs) {
        std::vector<IncludeEdge> found = scan_includes(doc.text, doc.path);
        for (auto& edge : found) edge.header = resolve_header(edge.from, edge.header, paths);
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
