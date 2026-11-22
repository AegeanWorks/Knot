#include "ignore.hpp"

namespace knot::core {
namespace {

char at(const std::string& value, std::size_t i) {
    return i < value.size() ? value[i] : '\0';
}

std::string basename(const std::string& path) {
    std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

GlobState glob_step(GlobState s, const std::string& pattern, const std::string& text) {
    if (s.matched || s.failed) return s;
    if (s.t >= text.size()) {
        while (s.p < pattern.size() && at(pattern, s.p) == '*') ++s.p;
        s.matched = s.p == pattern.size();
        s.failed = !s.matched;
        return s;
    }

    char c = at(pattern, s.p);
    if (c == '?' && c != '/') {
        if (text[s.t] == '/') {
            s.failed = true;
            return s;
        }
        ++s.p;
        ++s.t;
        return s;
    }
    if (c != '\0' && c != '*' && c == text[s.t]) {
        ++s.p;
        ++s.t;
        return s;
    }
    if (c == '*') {
        bool doubled = at(pattern, s.p + 1) == '*';
        s.p += doubled ? 2 : 1;
        if (doubled) {
            while (at(pattern, s.p) == '/') ++s.p;
        }
        s.star_p = static_cast<long long>(s.p);
        s.star_t = s.t;
        s.star_double = doubled;
        return s;
    }
    if (s.star_p >= 0) {
        if (!s.star_double && text[s.star_t] == '/') {
            s.failed = true;
            return s;
        }
        ++s.star_t;
        s.t = s.star_t;
        s.p = static_cast<std::size_t>(s.star_p);
        return s;
    }
    s.failed = true;
    return s;
}

bool glob_match(const std::string& pattern, const std::string& text) {
    GlobState s;
    int guard = 0;
    while (!s.matched && !s.failed && guard++ < 100000) s = glob_step(s, pattern, text);
    return s.matched;
}

IgnoreRules parse_ignore(const std::string& text) {
    IgnoreRules rules;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos
                                                                       : end - start);
        start = end == std::string::npos ? text.size() + 1 : end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t')) ++first;
        std::size_t last = line.size();
        while (last > first && (line[last - 1] == ' ' || line[last - 1] == '\t')) --last;
        line = line.substr(first, last - first);
        if (line.empty() || line[0] == '#') continue;

        IgnoreRule rule;
        if (line[0] == '!') {
            rule.negated = true;
            line.erase(0, 1);
        }
        if (!line.empty() && line[0] == '/') {
            rule.anchored = true;
            line.erase(0, 1);
        }
        if (!line.empty() && line.back() == '/') {
            rule.directory = true;
            line.pop_back();
        }
        if (line.empty()) continue;
        rule.pattern = line;
        rules.push_back(rule);
    }
    return rules;
}

bool is_ignored(const IgnoreRules& rules, const std::string& relative_path) {
    bool ignored = false;
    for (const auto& rule : rules) {
        bool hit = false;
        if (rule.directory) {
            if (relative_path == rule.pattern ||
                relative_path.compare(0, rule.pattern.size() + 1, rule.pattern + "/") == 0) {
                hit = true;
            }
            std::size_t marker = relative_path.find("/" + rule.pattern + "/");
            if (marker != std::string::npos) hit = true;
        } else {
            std::string subject = relative_path;
            if (!rule.anchored && rule.pattern.find('/') == std::string::npos) {
                subject = basename(relative_path);
            }
            hit = glob_match(rule.pattern, subject);
            if (!hit && !rule.anchored && rule.pattern.find('/') != std::string::npos) {
                std::size_t slash = relative_path.find('/');
                if (slash != std::string::npos) {
                    hit = glob_match(rule.pattern, relative_path.substr(slash + 1));
                }
            }
        }
        if (hit) ignored = !rule.negated;
    }
    return ignored;
}

}  // namespace knot::core
