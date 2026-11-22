#include "missing.hpp"

#include <algorithm>
#include <set>

#include "graph.hpp"
#include "path.hpp"

namespace knot::core {
namespace {

bool ends_with(const std::string& path, const std::string& suffix) {
    return path.size() >= suffix.size() &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_translation_unit(const std::string& path) {
    return ends_with(path, ".cpp") || ends_with(path, ".cc") || ends_with(path, ".cxx");
}

bool is_header(const std::string& path) {
    return ends_with(path, ".h") || ends_with(path, ".hpp") ||
           ends_with(path, ".hh") || ends_with(path, ".hxx");
}

}  // namespace

std::vector<Recommendation> missing_includes(const Analysis& analysis) {
    std::vector<Recommendation> out;
    std::vector<std::string> headers;
    for (const auto& doc : analysis.docs) {
        if (is_header(doc.path)) headers.push_back(doc.path);
    }
    if (headers.empty()) return out;

    for (const auto& doc : analysis.docs) {
        if (!is_translation_unit(doc.path)) continue;
        std::set<std::string> used;
        for (const auto& token : scan_identifiers(doc.text)) used.insert(token.text);
        if (used.empty()) continue;
        for (const auto& header : headers) {
            if (header == doc.path) continue;
            std::string type = type_name_from_header(header);
            if (type.empty() || !used.count(type)) continue;
            if (!shortest_path(analysis.graph, doc.path, header).empty()) continue;
            Recommendation rec;
            rec.kind = Recommendation::Kind::AddInclude;
            rec.file = doc.path;
            rec.header = header;
            rec.reason = "uses " + type + " but its header is not reachable";
            out.push_back(rec);
        }
    }
    std::sort(out.begin(), out.end(), [](const Recommendation& a, const Recommendation& b) {
        if (a.kind != b.kind) return a.kind < b.kind;
        if (a.file != b.file) return a.file < b.file;
        if (a.header != b.header) return a.header < b.header;
        return a.reason < b.reason;
    });
    return out;
}

}  // namespace knot::core
