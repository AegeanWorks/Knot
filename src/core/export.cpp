#include "export.hpp"

#include <sstream>

namespace knot::core {
namespace {

std::string dot_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string mermaid_escape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"') out += "#quot;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out.push_back(c);
    }
    return out;
}

}  // namespace

std::string render_dot(const Analysis& analysis) {
    std::ostringstream os;
    os << "digraph knot {\n  rankdir=LR;\n";
    for (const auto& id : analysis.graph.ids) {
        os << "  \"" << dot_escape(id) << "\";\n";
    }
    for (std::size_t i = 0; i < analysis.graph.ids.size(); ++i) {
        for (const auto& target : analysis.graph.adj[i]) {
            os << "  \"" << dot_escape(analysis.graph.ids[i]) << "\" -> \""
               << dot_escape(target) << "\";\n";
        }
    }
    os << "}\n";
    return os.str();
}

std::string render_mermaid(const Analysis& analysis) {
    std::ostringstream os;
    os << "graph TD\n";
    for (std::size_t i = 0; i < analysis.graph.ids.size(); ++i) {
        os << "  n" << i << "[\"" << mermaid_escape(analysis.graph.ids[i]) << "\"]\n";
    }
    for (std::size_t i = 0; i < analysis.graph.ids.size(); ++i) {
        for (const auto& target : analysis.graph.adj[i]) {
            auto target_it = analysis.graph.index.find(target);
            if (target_it == analysis.graph.index.end()) continue;
            os << "  n" << i << " --> n" << target_it->second << "\n";
        }
    }
    return os.str();
}

}  // namespace knot::core
