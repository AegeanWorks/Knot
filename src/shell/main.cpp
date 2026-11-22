// Imperative shell: parse, read at the edge, call the pure core, write at the edge.
#include <string>
#include <vector>

#include "cli.hpp"
#include "core/report.hpp"
#include "files.hpp"

int main(int argc, char** argv) {
    knot::shell::Command cmd = knot::shell::parse_args(argc, argv);
    if (!cmd.error.empty()) {
        knot::shell::write_stderr("knot: " + cmd.error + "\n" + knot::shell::usage_text());
        return 2;
    }

    switch (cmd.kind) {
        case knot::shell::CommandKind::Help:
            knot::shell::write_stdout(knot::shell::usage_text());
            return 0;
        case knot::shell::CommandKind::Version:
            knot::shell::write_stdout("knot 0.1.0\n");
            return 0;
        default:
            break;
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
    std::string rendered = cmd.format == "json"
                               ? knot::core::render_json(analysis, cmd.top)
                               : knot::core::render_text(analysis, cmd.top);
    if (!knot::shell::write_stdout(rendered)) return 3;

    if (cmd.fail_on_cycle && !analysis.cycles.empty()) return 1;
    return 0;
}
