// Reverse impact analysis: who is affected when a node changes.
// Pure, explicit BFS state machine over the reverse include graph.
#pragma once

#include <string>
#include <vector>

#include "types.hpp"

namespace knot::core {

enum class ImpactMark { Pending, Seen };

struct ImpactState {
    std::vector<ImpactMark> marks;
    std::vector<std::string> frontier;
    std::vector<std::string> found;
    bool done = false;
};

std::vector<std::vector<std::string>> reverse_graph(const IncludeGraph& g);
ImpactState impact_init(const IncludeGraph& g, const std::string& target);
ImpactState impact_step(ImpactState s, const IncludeGraph& g,
                        const std::vector<std::vector<std::string>>& reverse);

// Sorted transitive set of nodes that include `target`, excluding itself.
std::vector<std::string> impact_set(const IncludeGraph& g, const std::string& target);

std::string render_impact_text(const std::string& target,
                               const std::vector<std::string>& impacted);
std::string render_impact_json(const std::string& target,
                               const std::vector<std::string>& impacted);

}  // namespace knot::core
