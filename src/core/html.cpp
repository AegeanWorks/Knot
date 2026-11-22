#include "html.hpp"

#include <sstream>

#include "export.hpp"
#include "stats.hpp"

namespace knot::core {
namespace {

std::string escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
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

std::string render_html(const Analysis& analysis) {
    const Stats stats = compute_stats(analysis);
    std::ostringstream os;
    os << "<!doctype html>\n<html lang=\"en\">\n<head>\n"
       << "<meta charset=\"utf-8\">\n<title>Knot report</title>\n"
       << "<style>body{font:14px system-ui,sans-serif;margin:2rem;color:#1b1b1b}"
       << "h1,h2{color:#204a6b}table{border-collapse:collapse;margin:.5rem 0 1.5rem}"
       << "td,th{border:1px solid #ccc;padding:.3rem .6rem;text-align:left}"
       << "th{background:#eef3f7}pre{background:#f5f5f5;padding:1rem;overflow:auto}"
       << ".ok{color:#17603a}.bad{color:#8b1e1e}</style>\n</head>\n<body>\n"
       << "<h1>Knot include-graph report</h1>\n"
       << "<p class=\"" << (analysis.cycles.empty() ? "ok" : "bad") << "\">"
       << analysis.cycles.size() << " include cycle(s)</p>\n"
       << "<h2>Project</h2>\n<table><tr><th>Files</th><th>Translation units</th>"
       << "<th>Headers</th><th>Internal edges</th><th>External edges</th><th>Max depth</th></tr><tr>"
       << "<td>" << stats.files << "</td><td>" << stats.translation_units
       << "</td><td>" << stats.headers << "</td><td>" << stats.internal_edges
       << "</td><td>" << stats.external_edges << "</td><td>" << stats.max_depth
       << "</td></tr></table>\n";

    os << "<h2>Cycles</h2>\n";
    if (analysis.cycles.empty()) {
        os << "<p class=\"ok\">None.</p>\n";
    } else {
        os << "<ul>\n";
        for (const auto& cycle : analysis.cycles) {
            os << "<li>";
            for (std::size_t i = 0; i < cycle.nodes.size(); ++i) {
                if (i) os << " &rarr; ";
                os << escape(cycle.nodes[i]);
            }
            os << "</li>\n";
        }
        os << "</ul>\n";
    }

    os << "<h2>Transitive cost</h2>\n<table><tr><th>Node</th><th>Direct</th>"
          "<th>Transitive</th></tr>\n";
    for (const auto& cost : analysis.costs) {
        os << "<tr><td>" << escape(cost.node) << "</td><td>" << cost.direct
           << "</td><td>" << cost.transitive << "</td></tr>\n";
    }
    os << "</table>\n";

    os << "<h2>Hotspots</h2>\n<table><tr><th>Header</th><th>Included by</th></tr>\n";
    for (const auto& hot : analysis.hotspots) {
        os << "<tr><td>" << escape(hot.header) << "</td><td>"
           << hot.included_by << "</td></tr>\n";
    }
    os << "</table>\n";

    os << "<h2>Recommendations</h2>\n<table><tr><th>Kind</th><th>File</th>"
          "<th>Header</th><th>Reason</th></tr>\n";
    for (const auto& rec : analysis.recommendations) {
        os << "<tr><td>" << kind_name(rec.kind) << "</td><td>" << escape(rec.file)
           << "</td><td>" << escape(rec.header) << "</td><td>"
           << escape(rec.reason) << "</td></tr>\n";
    }
    os << "</table>\n";

    os << "<h2>Graphviz source</h2>\n<pre>" << escape(render_dot(analysis))
       << "</pre>\n</body>\n</html>\n";
    return os.str();
}

}  // namespace knot::core
