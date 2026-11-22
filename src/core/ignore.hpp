// Pure glob matching and .knotignore parsing. Explicit backtracking state machine.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace knot::core {

struct IgnoreRule {
    std::string pattern;
    bool negated = false;
    bool anchored = false;
    bool directory = false;
};

using IgnoreRules = std::vector<IgnoreRule>;

struct GlobState {
    std::size_t p = 0;
    std::size_t t = 0;
    long long star_p = -1;
    std::size_t star_t = 0;
    bool star_double = false;
    bool matched = false;
    bool failed = false;
};

// One pure transition of the glob matcher. `**` crosses '/', `*` and `?` do not.
GlobState glob_step(GlobState s, const std::string& pattern, const std::string& text);
bool glob_match(const std::string& pattern, const std::string& text);

IgnoreRules parse_ignore(const std::string& text);
// Last matching rule wins; a negated rule re-includes a path.
bool is_ignored(const IgnoreRules& rules, const std::string& relative_path);

}  // namespace knot::core
