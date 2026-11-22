#include "graph.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace knot::core {
namespace {

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_alnum(char c) {
    return is_alpha(c) || (c >= '0' && c <= '9');
}

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool is_translation_unit(const std::string& path) {
    return path.size() > 4 && (path.substr(path.size() - 4) == ".cpp" ||
                               path.substr(path.size() - 3) == ".cc" ||
                               path.substr(path.size() - 4) == ".cxx");
}

bool is_header(const std::string& path) {
    return path.size() > 2 && (path.substr(path.size() - 2) == ".h" ||
                               path.substr(path.size() - 4) == ".hpp" ||
                               path.substr(path.size() - 3) == ".hh" ||
                               path.substr(path.size() - 4) == ".hxx");
}

}  // namespace

IncludeGraph build_graph(const std::vector<IncludeEdge>& edges) {
    IncludeGraph g;
    std::set<std::string> ids;
    for (const auto& e : edges) {
        if (!e.from.empty()) ids.insert(e.from);
        if (!e.header.empty()) ids.insert(e.header);
    }
    g.ids.assign(ids.begin(), ids.end());
    for (std::size_t i = 0; i < g.ids.size(); ++i) g.index[g.ids[i]] = i;
    g.adj.assign(g.ids.size(), {});

    std::vector<std::set<std::string>> unique_adj(g.ids.size());
    for (const auto& e : edges) {
        auto from = g.index.find(e.from);
        auto to = g.index.find(e.header);
        if (from == g.index.end() || to == g.index.end()) continue;
        unique_adj[from->second].insert(e.header);
    }
    for (std::size_t i = 0; i < g.adj.size(); ++i) {
        g.adj[i].assign(unique_adj[i].begin(), unique_adj[i].end());
    }
    return g;
}

DfsState dfs_init(const IncludeGraph& g) {
    DfsState s;
    s.marks.assign(g.ids.size(), Mark::White);
    return s;
}

DfsState dfs_step(DfsState s, const IncludeGraph& g) {
    s.found_this_step = false;
    if (s.done) return s;

    if (s.stack.empty()) {
        while (s.next_root < s.marks.size() &&
               s.marks[s.next_root] != Mark::White) {
            ++s.next_root;
        }
        if (s.next_root >= s.marks.size()) {
            s.done = true;
            return s;
        }
        std::size_t root = s.next_root++;
        s.marks[root] = Mark::Gray;
        s.stack.push_back(DfsFrame{root, 0});
        s.path.push_back(g.ids[root]);
        return s;
    }

    DfsFrame frame = s.stack.back();
    const auto& neighbours = g.adj[frame.node];
    if (frame.next >= neighbours.size()) {
        s.marks[frame.node] = Mark::Black;
        s.stack.pop_back();
        s.path.pop_back();
        return s;
    }

    std::size_t nb_index = g.index.at(neighbours[frame.next]);
    s.stack.back().next += 1;
    if (s.marks[nb_index] == Mark::Gray) {
        auto it = std::find(s.path.begin(), s.path.end(), neighbours[frame.next]);
        std::size_t pos = static_cast<std::size_t>(it - s.path.begin());
        Cycle cycle;
        for (std::size_t i = pos; i < s.path.size(); ++i) cycle.nodes.push_back(s.path[i]);
        cycle.nodes.push_back(neighbours[frame.next]);
        s.cycles.push_back(cycle);
        s.found_this_step = true;
        return s;
    }
    if (s.marks[nb_index] == Mark::White) {
        s.marks[nb_index] = Mark::Gray;
        s.stack.push_back(DfsFrame{nb_index, 0});
        s.path.push_back(g.ids[nb_index]);
    }
    return s;
}

std::vector<Cycle> find_cycles(const IncludeGraph& g) {
    DfsState s = dfs_init(g);
    std::vector<Cycle> unique;
    while (!s.done) {
        s = dfs_step(s, g);
        if (!s.found_this_step) continue;
        // Canonicalise by rotating to the lexicographically smallest node so
        // equivalent cycles discovered from different roots compare equal.
        const auto& nodes = s.cycles.back().nodes;
        if (nodes.size() < 2) continue;
        std::size_t best = 0;
        for (std::size_t i = 1; i + 1 < nodes.size(); ++i) {
            if (nodes[i] < nodes[best]) best = i;
        }
        std::vector<std::string> canonical;
        for (std::size_t k = 0; k + 1 < nodes.size(); ++k) {
            canonical.push_back(nodes[(best + k) % (nodes.size() - 1)]);
        }
        canonical.push_back(canonical.front());
        bool seen = false;
        for (const auto& existing : unique) {
            if (existing.nodes == canonical) {
                seen = true;
                break;
            }
        }
        if (!seen) unique.push_back(Cycle{canonical});
    }
    std::sort(unique.begin(), unique.end(), [](const Cycle& a, const Cycle& b) {
        return a.nodes < b.nodes;
    });
    return unique;
}

std::vector<Cost> transitive_costs(const IncludeGraph& g) {
    std::vector<Cost> costs;
    costs.reserve(g.ids.size());
    for (std::size_t root = 0; root < g.ids.size(); ++root) {
        // Explicit worklist state machine: Pending -> Seen, no recursion.
        std::vector<char> seen(g.ids.size(), 0);
        std::vector<std::size_t> work;
        work.push_back(root);
        seen[root] = 1;
        std::size_t count = 0;
        while (!work.empty()) {
            std::size_t node = work.back();
            work.pop_back();
            for (const auto& name : g.adj[node]) {
                std::size_t nb = g.index.at(name);
                if (seen[nb]) continue;
                seen[nb] = 1;
                ++count;
                work.push_back(nb);
            }
        }
        costs.push_back(Cost{g.ids[root], g.adj[root].size(), count});
    }
    std::sort(costs.begin(), costs.end(), [](const Cost& a, const Cost& b) {
        if (a.transitive != b.transitive) return a.transitive > b.transitive;
        return a.node < b.node;
    });
    return costs;
}

std::vector<Hotspot> fan_in(const IncludeGraph& g) {
    std::map<std::string, std::set<std::string>> sources;
    for (std::size_t i = 0; i < g.ids.size(); ++i) {
        for (const auto& name : g.adj[i]) sources[name].insert(g.ids[i]);
    }
    std::vector<Hotspot> out;
    for (const auto& [header, froms] : sources) {
        out.push_back(Hotspot{header, froms.size()});
    }
    std::sort(out.begin(), out.end(), [](const Hotspot& a, const Hotspot& b) {
        if (a.included_by != b.included_by) return a.included_by > b.included_by;
        return a.header < b.header;
    });
    return out;
}

ScanStep ident_step(IdentState s, char c) {
    switch (s.state) {
        case TokenState::Seeking:
            if (is_alpha(c)) {
                s.state = TokenState::Identifier;
                s.current.clear();
                s.current.push_back(c);
            } else if (c == '/') {
                s.state = TokenState::SlashPending;
            } else if (c == '"') {
                s.state = TokenState::StringLit;
            } else if (c == '\'') {
                s.state = TokenState::CharLit;
            }
            return ScanStep{s, false, Token{}};

        case TokenState::Identifier:
            if (is_alnum(c)) {
                s.current.push_back(c);
                return ScanStep{s, false, Token{}};
            }
            if (is_space(c)) {
                s.state = TokenState::AfterIdentifier;
                return ScanStep{s, false, Token{}};
            }
            {
                Token token{s.current, c};
                s.current.clear();
                s.state = c == '/' ? TokenState::SlashPending
                          : c == '"' ? TokenState::StringLit
                          : c == '\'' ? TokenState::CharLit
                                      : TokenState::Seeking;
                return ScanStep{s, true, token};
            }

        case TokenState::AfterIdentifier:
            if (is_space(c)) return ScanStep{s, false, Token{}};
            {
                Token token{s.current, c};
                s.current.clear();
                // The current character is significant: if it starts another
                // identifier it must begin the next token, not be discarded.
                if (is_alpha(c)) {
                    s.state = TokenState::Identifier;
                    s.current.push_back(c);
                } else if (c == '/') {
                    s.state = TokenState::SlashPending;
                } else if (c == '"') {
                    s.state = TokenState::StringLit;
                } else if (c == '\'') {
                    s.state = TokenState::CharLit;
                } else {
                    s.state = TokenState::Seeking;
                }
                return ScanStep{s, true, token};
            }

        case TokenState::LineComment:
            if (c == '\n') s.state = TokenState::Seeking;
            return ScanStep{s, false, Token{}};

        case TokenState::BlockComment:
            if (c == '*') s.state = TokenState::BlockStar;
            return ScanStep{s, false, Token{}};

        case TokenState::BlockStar:
            if (c == '/') s.state = TokenState::Seeking;
            else if (c != '*') s.state = TokenState::BlockComment;
            return ScanStep{s, false, Token{}};

        case TokenState::StringLit:
            if (c == '\\') s.state = TokenState::StringEscape;
            else if (c == '"') s.state = TokenState::Seeking;
            return ScanStep{s, false, Token{}};

        case TokenState::StringEscape:
            s.state = TokenState::StringLit;
            return ScanStep{s, false, Token{}};

        case TokenState::CharLit:
            if (c == '\\') s.state = TokenState::CharEscape;
            else if (c == '\'') s.state = TokenState::Seeking;
            return ScanStep{s, false, Token{}};

        case TokenState::CharEscape:
            s.state = TokenState::CharLit;
            return ScanStep{s, false, Token{}};

        case TokenState::SlashPending:
            if (c == '/') s.state = TokenState::LineComment;
            else if (c == '*') s.state = TokenState::BlockComment;
            else s.state = TokenState::Seeking;
            return ScanStep{s, false, Token{}};
    }
    return ScanStep{s, false, Token{}};
}

std::vector<Token> scan_identifiers(const std::string& text) {
    IdentState state;
    std::vector<Token> out;
    for (char c : text) {
        ScanStep step = ident_step(state, c);
        state = step.state;
        if (step.emitted) out.push_back(step.token);
    }
    if (state.state == TokenState::Identifier ||
        state.state == TokenState::AfterIdentifier) {
        out.push_back(Token{state.current, '\0'});
    }
    return out;
}

bool can_forward_declare(const std::string& text, const std::string& type_name) {
    if (type_name.empty()) return false;
    std::size_t hits = 0;
    for (const auto& token : scan_identifiers(text)) {
        if (token.text != type_name) continue;
        ++hits;
        if (token.next_significant != '*' && token.next_significant != '&') {
            return false;
        }
    }
    return hits > 0;
}

std::string type_name_from_header(const std::string& header) {
    std::size_t slash = header.find_last_of("/\\");
    std::string base = slash == std::string::npos ? header : header.substr(slash + 1);
    std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    if (base.empty()) return base;
    if (base[0] >= 'a' && base[0] <= 'z') base[0] = static_cast<char>(base[0] - 'a' + 'A');
    return base;
}

std::vector<Recommendation> recommendations(
    const IncludeGraph& g,
    const std::vector<IncludeEdge>& edges,
    const std::vector<SourceDoc>& docs,
    const std::vector<Cycle>& cycles) {
    (void)g;  // kept in the signature: the graph is reserved for future rules
    std::vector<Recommendation> out;

    // Duplicate directives in the same translation unit.
    std::map<std::pair<std::string, std::string>, int> counts;
    for (const auto& e : edges) counts[{e.from, e.header}] += 1;
    for (const auto& [key, count] : counts) {
        if (count < 2) continue;
        out.push_back(Recommendation{Recommendation::Kind::RemoveDuplicate,
                                     key.first, key.second,
                                     "included " + std::to_string(count) + " times"});
    }

    // Closing edge of each cycle.
    for (const auto& cycle : cycles) {
        if (cycle.nodes.size() < 3) continue;
        const std::string& from = cycle.nodes[cycle.nodes.size() - 2];
        const std::string& to = cycle.nodes.back();
        out.push_back(Recommendation{Recommendation::Kind::BreakCycle, from, to,
                                     "closes an include cycle"});
    }

    // Pointer/reference-only use in a .cpp: a forward declaration may replace
    // the header include. Only internal quoted headers are considered.
    std::map<std::string, const SourceDoc*> by_path;
    for (const auto& doc : docs) by_path[doc.path] = &doc;
    std::set<std::pair<std::string, std::string>> already_advised;
    for (const auto& e : edges) {
        if (!is_translation_unit(e.from) || !is_header(e.header)) continue;
        auto it = by_path.find(e.from);
        if (it == by_path.end()) continue;
        std::string type = type_name_from_header(e.header);
        if (!can_forward_declare(it->second->text, type)) continue;
        auto key = std::make_pair(e.from, e.header);
        if (!already_advised.insert(key).second) continue;  // duplicate directive
        out.push_back(Recommendation{Recommendation::Kind::ForwardDeclare,
                                     e.from, e.header,
                                     type + " is used only as pointer or reference"});
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
