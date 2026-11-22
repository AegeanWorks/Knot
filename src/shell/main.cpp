// Imperative shell: parse, read at the edge, call the pure core, write at the edge.
#include <string>
#include <vector>

#include "cli.hpp"
#include "core/diff.hpp"
#include "core/path.hpp"
#include "core/report.hpp"
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
        if (!knot::shell::load_sources(cmd.root, docs)) {
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
    } else if (!knot::shell::load_sources(cmd.root, docs)) {
        knot::shell::write_stderr("knot: cannot read directory: " + cmd.root + "\n");
        return 3;
    }

    // I/O stops here. Everything below is pure computation.
    const knot::core::Analysis analysis = knot::core::analyze(std::move(docs));

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
