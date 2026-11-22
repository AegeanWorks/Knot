#include "impact.hpp"

#include <algorithm>
#include <sstream>

namespace knot::core {

std::vector<std::vector<std::string>> reverse_graph(const IncludeGraph& g) {
    std::vector<std::vector<std::string>> reverse(g.ids.size());
    for (std::size_t i = 0; i < g.ids.size(); ++i) {
        for (const auto& name : g.adj[i]) {
            auto it = g.index.find(name);
            if (it != g.index.end()) reverse[it->second].push_back(g.ids[i]);
        }
    }
    for (auto& list : reverse) std::sort(list.begin(), list.end());
    return reverse;
}

ImpactState impact_init(const IncludeGraph& g, const std::string& target) {
    ImpactState s;
    s.marks.assign(g.ids.size(), ImpactMark::Pending);
    auto it = g.index.find(target);
    if (it == g.index.end()) {
        s.done = true;
        return s;
    }
    s.marks[it->second] = ImpactMark::Seen;
    s.frontier.push_back(target);
    return s;
}

ImpactState impact_step(ImpactState s, const IncludeGraph& g,
                        const std::vector<std::vector<std::string>>& reverse) {
    if (s.done || s.frontier.empty()) {
        s.done = true;
        return s;
    }
    std::string node = s.frontier.front();
    s.frontier.erase(s.frontier.begin());
    std::size_t index = g.index.at(node);
    for (const auto& parent : reverse[index]) {
        std::size_t p = g.index.at(parent);
        if (s.marks[p] == ImpactMark::Seen) continue;
        s.marks[p] = ImpactMark::Seen;
        s.found.push_back(parent);
        s.frontier.push_back(parent);
    }
    return s;
}

std::vector<std::string> impact_set(const IncludeGraph& g, const std::string& target) {
    if (g.index.find(target) == g.index.end()) return {};
    ImpactState s = impact_init(g, target);
    std::vector<std::vector<std::string>> reverse = reverse_graph(g);
    int guard = 0;
    while (!s.done && guard++ < 100000) s = impact_step(s, g, reverse);
    std::sort(s.found.begin(), s.found.end());
    s.found.erase(std::unique(s.found.begin(), s.found.end()), s.found.end());
    return s.found;
}

std::string render_impact_text(const std::string& target,
                               const std::vector<std::string>& impacted) {
    std::ostringstream os;
    os << "Knot impact " << target << ": " << impacted.size() << " affected node(s)\n";
    for (const auto& node : impacted) os << "  " << node << "\n";
    return os.str();
}

std::string render_impact_json(const std::string& target,
                               const std::vector<std::string>& impacted) {
    auto escape = [](const std::string& value) {
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
    };
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"target\": \"" << escape(target) << "\",\n  \"impact\": [";
    for (std::size_t i = 0; i < impacted.size(); ++i) {
        if (i) os << ", ";
        os << "\"" << escape(impacted[i]) << "\"";
    }
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
