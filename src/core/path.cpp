#include "path.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace knot::core {
namespace {

bool is_translation_unit(const std::string& path) {
    static const char* suffixes[] = {".cpp", ".cc", ".cxx"};
    for (const char* suffix : suffixes) {
        std::string s(suffix);
        if (path.size() >= s.size() && path.compare(path.size() - s.size(), s.size(), s) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

PathState path_init(const IncludeGraph& g, const std::string& from) {
    PathState s;
    s.marks.assign(g.ids.size(), PathMark::Pending);
    auto it = g.index.find(from);
    if (it == g.index.end()) {
        s.done = true;
        return s;
    }
    s.marks[it->second] = PathMark::Seen;
    s.frontier.push_back(std::vector<std::string>{from});
    return s;
}

PathState path_step(PathState s, const IncludeGraph& g, const std::string& target) {
    if (s.done || s.frontier.empty()) {
        s.done = true;
        return s;
    }
    std::vector<std::string> path = s.frontier.front();
    s.frontier.erase(s.frontier.begin());
    if (path.back() == target) {
        s.found = path;
        s.done = true;
        return s;
    }
    std::size_t node = g.index.at(path.back());
    for (const auto& name : g.adj[node]) {
        std::size_t nb = g.index.at(name);
        if (s.marks[nb] == PathMark::Seen) continue;
        s.marks[nb] = PathMark::Seen;
        std::vector<std::string> next = path;
        next.push_back(name);
        s.frontier.push_back(next);
    }
    return s;
}

std::vector<std::string> shortest_path(const IncludeGraph& g, const std::string& from,
                                       const std::string& to) {
    if (g.index.find(from) == g.index.end() || g.index.find(to) == g.index.end()) return {};
    PathState s = path_init(g, from);
    int guard = 0;
    while (!s.done && guard++ < 100000) s = path_step(s, g, to);
    return s.found;
}

std::vector<std::string> best_path_to(const IncludeGraph& g, const std::string& target) {
    std::vector<std::string> best;
    for (const auto& id : g.ids) {
        if (!is_translation_unit(id)) continue;
        std::vector<std::string> candidate = shortest_path(g, id, target);
        if (candidate.empty()) continue;
        if (best.empty() || candidate.size() < best.size() ||
            (candidate.size() == best.size() && candidate < best)) {
            best = candidate;
        }
    }
    return best;
}

std::string resolve_node(const IncludeGraph& g, const std::string& name) {
    if (g.index.count(name)) return name;
    std::string folded = name;
    for (char& c : folded) {
        if (c == '\\') c = '/';
        else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    std::string match;
    for (const auto& id : g.ids) {
        std::string candidate = id;
        for (char& c : candidate) {
            if (c == '\\') c = '/';
            else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        bool equal = candidate == folded;
        bool suffix = candidate.size() > folded.size() &&
                      candidate[candidate.size() - folded.size() - 1] == '/' &&
                      candidate.compare(candidate.size() - folded.size(), folded.size(), folded) == 0;
        if (!equal && !suffix) continue;
        if (!match.empty()) return name;  // ambiguous
        match = id;
    }
    return match.empty() ? name : match;
}

std::string render_path_text(const std::string& target, const std::vector<std::string>& path) {
    std::ostringstream os;
    os << "Knot why " << target << ":\n";
    if (path.empty()) {
        os << "  no include path found\n";
        return os.str();
    }
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) os << " -> ";
        os << path[i];
    }
    os << "\n";
    return os.str();
}

std::string render_path_json(const std::string& target, const std::vector<std::string>& path) {
    std::ostringstream os;
    os << "{\n  \"schema\": 1,\n  \"target\": \"" << target << "\",\n  \"path\": [";
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) os << ", ";
        os << "\"" << path[i] << "\"";
    }
    os << "]\n}\n";
    return os.str();
}

}  // namespace knot::core
