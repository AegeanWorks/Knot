#include "stats.hpp"

#include <algorithm>
#include <set>
#include <sstream>

#include "path.hpp"

namespace knot::core {
namespace {

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

bool is_translation_unit(const std::string& path) {
    return ends_with(path, ".cpp") || ends_with(path, ".cc") || ends_with(path, ".cxx");
}

bool is_header(const std::string& path) {
    return ends_with(path, ".h") || ends_with(path, ".hpp") ||
           ends_with(path, ".hh") || ends_with(path, ".hxx");
}

}  // namespace

Stats compute_stats(const Analysis& analysis) {
    Stats stats;
    stats.files = analysis.docs.size();
    std::set<std::string> document_paths;
    for (const auto& doc : analysis.docs) {
        document_paths.insert(doc.path);
        if (is_translation_unit(doc.path)) ++stats.translation_units;
        if (is_header(doc.path)) ++stats.headers;
    }
    stats.edges = analysis.edges.size();
    for (const auto& edge : analysis.edges) {
        if (document_paths.count(edge.header)) ++stats.internal_edges;
        else ++stats.external_edges;
    }

    // Eccentricity via the existing bounded BFS. Ties break by the
    // lexicographically smallest chain so the verdict is reproducible.
    for (const auto& source : analysis.graph.ids) {
        for (const auto& target : analysis.graph.ids) {
            if (source == target) continue;
            std::vector<std::string> path = shortest_path(analysis.graph, source, target);
            if (path.empty()) continue;
            std::size_t depth = path.size() - 1;
            if (depth > stats.max_depth) {
                stats.max_depth = depth;
                stats.deepest_chain = path;
            } else if (depth == stats.max_depth &&
                       (stats.deepest_chain.empty() || path < stats.deepest_chain)) {
                stats.deepest_chain = path;
            }
        }
    }
    return stats;
}

std::string render_stats_text(const Stats& stats) {
    std::ostringstream os;
    os << "Knot stats: " << stats.files << " file(s), " << stats.translation_units
       << " translation unit(s), " << stats.headers << " header(s)\n";
    os << "Edges: " << stats.internal_edges << " internal, " << stats.external_edges
       << " external (" << stats.edges << " total)\n";
    os << "Deepest include chain: " << stats.max_depth << " hop(s)\n";
    if (!stats.deepest_chain.empty()) {
        os << "  ";
        for (std::size_t i = 0; i < stats.deepest_chain.size(); ++i) {
            if (i) os << " -> ";
            os << stats.deepest_chain[i];
        }
        os << "\n";
    }
    return os.str();
}

std::string render_stats_json(const Stats& stats) {
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"files\": " << stats.files
       << ",\n  \"translation_units\": " << stats.translation_units
       << ",\n  \"headers\": " << stats.headers
       << ",\n  \"edges\": " << stats.edges
       << ",\n  \"internal_edges\": " << stats.internal_edges
       << ",\n  \"external_edges\": " << stats.external_edges
       << ",\n  \"max_depth\": " << stats.max_depth
       << ",\n  \"deepest_chain\": [";
    for (std::size_t i = 0; i < stats.deepest_chain.size(); ++i) {
        if (i) os << ", ";
        os << "\"" << json_escape(stats.deepest_chain[i]) << "\"";
    }
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
