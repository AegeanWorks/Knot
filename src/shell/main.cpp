// Imperative shell: parse, read at the edge, call the pure core, write at the edge.
#include <string>
#include <vector>

#include "cli.hpp"
#include "core/diff.hpp"
#include "core/export.hpp"
#include "core/impact.hpp"
#include "core/ignore.hpp"
#include "core/path.hpp"
#include "core/policy.hpp"
#include "core/report.hpp"
#include "core/scaffold.hpp"
#include "core/stats.hpp"
#include "files.hpp"

namespace {

bool exceeds(const std::vector<knot::core::Cost>& costs, std::size_t limit) {
    if (limit == 0) return false;
    for (const auto& cost : costs) {
        if (cost.transitive > limit) return true;
    }
    return false;
}

bool exceeds(const std::vector<knot::core::Hotspot>& hotspots, std::size_t limit) {
    if (limit == 0) return false;
    for (const auto& hot : hotspots) {
        if (hot.included_by > limit) return true;
    }
    return false;
}

knot::core::IgnoreRules load_rules(const knot::shell::Command& cmd) {
    knot::core::IgnoreRules rules = knot::core::parse_ignore(
        ".git/\n.aikit/\nnode_modules/\nbuild/\n");
    const std::string path = cmd.ignore_file.empty()
                                 ? cmd.root + "/.knotignore"
                                 : cmd.ignore_file;
    if (!knot::shell::file_exists(path)) return rules;
    std::string text;
    if (!knot::shell::read_text_file(path, text)) return rules;
    knot::core::IgnoreRules user = knot::core::parse_ignore(text);
    rules.insert(rules.end(), user.begin(), user.end());
    return rules;
}

knot::core::Policy load_policy(const knot::shell::Command& cmd) {
    const std::string path = cmd.policy_file.empty()
                                 ? cmd.root + "/.knot-policy"
                                 : cmd.policy_file;
    if (!knot::shell::file_exists(path)) return knot::core::Policy{};
    std::string text;
    if (!knot::shell::read_text_file(path, text)) return knot::core::Policy{};
    return knot::core::parse_policy(text);
}

}  // namespace

int main(int argc, char** argv) {
    const knot::shell::Command cmd = knot::shell::parse_args(argc, argv);
    if (!cmd.error.empty()) {
        knot::shell::write_stderr("knot: " + cmd.error + "\n" + knot::shell::usage_text());
        return 2;
    }

    switch (cmd.kind) {
        case knot::shell::CommandKind::Help:
            knot::shell::write_stdout(knot::shell::usage_text());
            return 0;
        case knot::shell::CommandKind::Version:
            knot::shell::write_stdout("knot 0.2.0\n");
            return 0;
        default:
            break;
    }

    if (cmd.kind == knot::shell::CommandKind::Diff) {
        std::string baseline_text;
        if (!knot::shell::read_text_file(cmd.baseline, baseline_text)) {
            knot::shell::write_stderr("knot: cannot read baseline: " + cmd.baseline + "\n");
            return 3;
        }
        knot::core::Snapshot before;
        std::string error;
        if (!knot::core::snapshot_from_json(baseline_text, before, error)) {
            knot::shell::write_stderr("knot: invalid baseline: " + error + "\n");
            return 2;
        }
        std::vector<knot::core::SourceDoc> docs;
        if (!knot::shell::load_sources(cmd.roots, load_rules(cmd), docs)) {
            knot::shell::write_stderr("knot: cannot read directory: " + cmd.root + "\n");
            return 3;
        }
        const knot::core::Analysis analysis = knot::core::analyze(std::move(docs));
        const knot::core::DiffReport diff =
            knot::core::diff_snapshots(before, knot::core::snapshot_from_analysis(analysis));
        const std::string rendered = cmd.format == "json"
                                         ? knot::core::render_diff_json(diff)
                                         : knot::core::render_diff_text(diff);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return cmd.fail_on_regression && diff.regressions ? 1 : 0;
    }

    std::vector<knot::core::SourceDoc> docs;
    if (cmd.kind == knot::shell::CommandKind::Demo) {
        docs = knot::shell::demo_docs();
    } else if (!knot::shell::load_sources(cmd.roots, load_rules(cmd), docs)) {
        knot::shell::write_stderr("knot: cannot read directory: " + cmd.root + "\n");
        return 3;
    }

    // I/O stops here. Everything below is pure computation.
    const knot::core::Analysis analysis = knot::core::analyze(std::move(docs));

    if (cmd.kind == knot::shell::CommandKind::Init) {
        const std::string ignore_path = cmd.root + "/.knotignore";
        const std::string policy_path = cmd.root + "/.knot-policy";
        if (!cmd.force && (knot::shell::file_exists(ignore_path) ||
                           knot::shell::file_exists(policy_path))) {
            knot::shell::write_stderr(
                "knot: configuration already exists; pass --force to overwrite\n");
            return 2;
        }
        if (!knot::shell::write_text_file(ignore_path, knot::core::default_ignore_text()) ||
            !knot::shell::write_text_file(policy_path, knot::core::default_policy_text())) {
            knot::shell::write_stderr("knot: cannot write configuration in " + cmd.root + "\n");
            return 3;
        }
        knot::shell::write_stdout("knot: wrote .knotignore and .knot-policy\n");
        return 0;
    }

    if (cmd.kind == knot::shell::CommandKind::Check) {
        const knot::core::Policy policy = load_policy(cmd);
        knot::core::DiffReport diff;
        if (!policy.baseline.empty()) {
            std::string baseline_path = policy.baseline;
            if (baseline_path.find(':') == std::string::npos && baseline_path[0] != '/') {
                baseline_path = cmd.root + "/" + baseline_path;
            }
            std::string baseline_text;
            if (!knot::shell::read_text_file(baseline_path, baseline_text)) {
                knot::shell::write_stderr("knot: cannot read baseline: " + baseline_path + "\n");
                return 3;
            }
            knot::core::Snapshot before;
            std::string error;
            if (!knot::core::snapshot_from_json(baseline_text, before, error)) {
                knot::shell::write_stderr("knot: invalid baseline: " + error + "\n");
                return 2;
            }
            diff = knot::core::diff_snapshots(
                before, knot::core::snapshot_from_analysis(analysis));
        }
        const std::vector<knot::core::Violation> violations =
            knot::core::evaluate_policy(analysis, policy, diff);
        const std::string rendered = cmd.format == "json"
                                         ? knot::core::render_violations_json(violations)
                                         : knot::core::render_violations_text(violations);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return violations.empty() ? 0 : 1;
    }

    if (cmd.kind == knot::shell::CommandKind::Stats) {
        const knot::core::Stats stats = knot::core::compute_stats(analysis);
        const std::string rendered = cmd.format == "json"
                                         ? knot::core::render_stats_json(stats)
                                         : knot::core::render_stats_text(stats);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return 0;
    }

    if (cmd.kind == knot::shell::CommandKind::Baseline) {
        if (!cmd.force && knot::shell::file_exists(cmd.baseline)) {
            knot::shell::write_stderr(
                "knot: baseline already exists; pass --force to overwrite\n");
            return 2;
        }
        std::vector<knot::core::SourceDoc> docs;
        if (!knot::shell::load_sources(cmd.roots, load_rules(cmd), docs)) {
            knot::shell::write_stderr("knot: cannot read directory: " + cmd.root + "\n");
            return 3;
        }
        const knot::core::Analysis analysis = knot::core::analyze(std::move(docs));
        if (!knot::shell::write_text_file(cmd.baseline,
                                          knot::core::render_baseline_json(analysis))) {
            knot::shell::write_stderr("knot: cannot write baseline: " + cmd.baseline + "\n");
            return 3;
        }
        knot::shell::write_stdout("knot: wrote baseline " + cmd.baseline + "\n");
        return 0;
    }

    if (cmd.kind == knot::shell::CommandKind::Graph) {
        const std::string rendered = cmd.format == "mermaid"
                                         ? knot::core::render_mermaid(analysis)
                                         : knot::core::render_dot(analysis);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return 0;
    }

    if (cmd.kind == knot::shell::CommandKind::Impact) {
        const std::string target = knot::core::resolve_node(analysis.graph, cmd.target);
        const std::vector<std::string> impacted = knot::core::impact_set(analysis.graph, target);
        const std::string rendered = cmd.format == "json"
                                         ? knot::core::render_impact_json(target, impacted)
                                         : knot::core::render_impact_text(target, impacted);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return 0;
    }

    if (cmd.kind == knot::shell::CommandKind::Why) {
        const std::string target = knot::core::resolve_node(analysis.graph, cmd.target);
        const std::vector<std::string> path =
            cmd.from.empty() ? knot::core::best_path_to(analysis.graph, target)
                             : knot::core::shortest_path(
                                   analysis.graph, knot::core::resolve_node(analysis.graph, cmd.from),
                                   target);
        const std::string rendered = cmd.format == "json"
                                         ? knot::core::render_path_json(target, path)
                                         : knot::core::render_path_text(target, path);
        if (!knot::shell::write_stdout(rendered)) return 3;
        return 0;
    }

    const std::string rendered = cmd.format == "json"
                                     ? knot::core::render_json(analysis, cmd.top)
                                     : knot::core::render_text(analysis, cmd.top);
    if (!knot::shell::write_stdout(rendered)) return 3;

    if (cmd.fail_on_cycle && !analysis.cycles.empty()) return 1;
    if (exceeds(analysis.costs, cmd.fail_cost)) return 1;
    if (exceeds(analysis.hotspots, cmd.fail_hotspot)) return 1;
    return 0;
}
