// Zero-dependency test harness. One binary, no framework, no network, no sleeps.
#include <iostream>
#include <string>
#include <vector>

#include "core/diff.hpp"
#include "core/export.hpp"
#include "core/graph.hpp"
#include "core/impact.hpp"
#include "core/ignore.hpp"
#include "core/missing.hpp"
#include "core/json.hpp"
#include "core/lexer.hpp"
#include "core/path.hpp"
#include "core/policy.hpp"
#include "core/report.hpp"
#include "core/scaffold.hpp"
#include "core/stats.hpp"
#include "shell/cli.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* label, int line) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL line " << line << ": " << label << "\n";
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

using namespace knot::core;

void test_lexer_states() {
    LexerState s;
    CHECK(s.state == LexState::Normal);
    LexStep a = lex_step(s, '#');
    CHECK(a.state.state == LexState::AfterHash);
    LexStep b = lex_step(a.state, 'i');
    CHECK(b.state.state == LexState::Directive);
    CHECK(b.state.directive == "i");
    CHECK(!a.emitted && !b.emitted);
}

void test_scan_includes() {
    std::string src =
        "#include \"alpha.hpp\"\n"
        "  # include <beta/gamma.h>\n"
        "// #include \"ignored.hpp\"\n"
        "/* #include \"also_ignored.hpp\" */\n"
        "const char* s = \"#include \\\"string.hpp\\\"\";\n"
        "#pragma once\n"
        "#include \"delta.hpp\"\n";
    std::vector<IncludeEdge> edges = scan_includes(src, "unit.cpp");
    CHECK(edges.size() == 3);
    if (edges.size() == 3) {
        CHECK(edges[0].header == "alpha.hpp" && edges[0].line == 1);
        CHECK(edges[1].header == "beta/gamma.h" && edges[1].line == 2);
        CHECK(edges[2].header == "delta.hpp" && edges[2].line == 7);
        for (const auto& e : edges) CHECK(e.from == "unit.cpp");
    }
}

void test_lexer_edge_cases() {
    CHECK(scan_includes("#include \"\"\n", "a.cpp").empty());
    CHECK(scan_includes("#include \"unterminated\n", "a.cpp").empty());
    CHECK(scan_includes("x# include \"not_at_line_start.hpp\"\n", "a.cpp").empty());
    std::vector<IncludeEdge> multiline = scan_includes("/*\n#include \"c.hpp\"\n*/\n#include \"d.hpp\"\n", "a.cpp");
    CHECK(multiline.size() == 1);
    if (multiline.size() == 1) {
        CHECK(multiline[0].header == "d.hpp");
        CHECK(multiline[0].line == 4);
    }
}

void test_graph_build() {
    std::vector<IncludeEdge> edges{
        {"b.cpp", "z.hpp", 1}, {"a.cpp", "z.hpp", 1}, {"z.hpp", "m.hpp", 2}};
    IncludeGraph g = build_graph(edges);
    CHECK(g.ids.size() == 4);
    CHECK(g.ids[0] == "a.cpp" && g.ids[1] == "b.cpp" && g.ids[2] == "m.hpp" && g.ids[3] == "z.hpp");
    CHECK(g.adj[g.index.at("a.cpp")].size() == 1);
    CHECK(g.adj[g.index.at("a.cpp")][0] == "z.hpp");
    CHECK(g.adj[g.index.at("m.hpp")].empty());
}

void test_cycles() {
    IncludeGraph none = build_graph({{"a.cpp", "b.hpp", 1}, {"a.cpp", "c.hpp", 2}});
    CHECK(find_cycles(none).empty());

    IncludeGraph self = build_graph({{"a.hpp", "a.hpp", 1}});
    std::vector<Cycle> sc = find_cycles(self);
    CHECK(sc.size() == 1);
    if (sc.size() == 1) CHECK(sc[0].nodes == std::vector<std::string>({"a.hpp", "a.hpp"}));

    IncludeGraph pair = build_graph({{"a.hpp", "b.hpp", 1}, {"b.hpp", "a.hpp", 1}});
    std::vector<Cycle> pc = find_cycles(pair);
    CHECK(pc.size() == 1);
    if (pc.size() == 1) CHECK(pc[0].nodes == std::vector<std::string>({"a.hpp", "b.hpp", "a.hpp"}));

    IncludeGraph triple = build_graph({{"a.hpp", "b.hpp", 1}, {"b.hpp", "c.hpp", 1},
                                       {"c.hpp", "a.hpp", 1}, {"x.cpp", "a.hpp", 1}});
    std::vector<Cycle> tc = find_cycles(triple);
    CHECK(tc.size() == 1);
    if (tc.size() == 1) CHECK(tc[0].nodes == std::vector<std::string>({"a.hpp", "b.hpp", "c.hpp", "a.hpp"}));
}

void test_dfs_step_machine() {
    IncludeGraph g = build_graph({{"a.hpp", "b.hpp", 1}, {"b.hpp", "a.hpp", 1}});
    DfsState s = dfs_init(g);
    CHECK(s.marks.size() == 2);
    CHECK(s.marks[0] == Mark::White);
    int steps = 0;
    while (!s.done && steps < 100) {
        s = dfs_step(s, g);
        ++steps;
    }
    CHECK(s.done);
    CHECK(steps < 100);
    CHECK(s.cycles.size() == 1);
}

void test_costs_and_fan_in() {
    std::vector<IncludeEdge> edges{
        {"app.cpp", "a.hpp", 1}, {"app.cpp", "b.hpp", 2},
        {"a.hpp", "c.hpp", 1}, {"b.hpp", "c.hpp", 1},
        {"other.cpp", "a.hpp", 1}};
    IncludeGraph g = build_graph(edges);
    std::vector<Cost> costs = transitive_costs(g);
    CHECK(costs.size() == 5);
    CHECK(costs[0].node == "app.cpp" && costs[0].transitive == 3 && costs[0].direct == 2);
    CHECK(costs[1].node == "other.cpp" && costs[1].transitive == 2);
    std::vector<Hotspot> hot = fan_in(g);
    CHECK(hot[0].header == "a.hpp" && hot[0].included_by == 2);
    CHECK(hot[1].header == "c.hpp" && hot[1].included_by == 2);
    CHECK(hot[2].header == "b.hpp" && hot[2].included_by == 1);
}

void test_identifier_machine() {
    std::vector<Token> tokens = scan_identifiers("Widget* w; const Widget& r; Widget value;");
    int widget_hits = 0;
    for (const auto& t : tokens) {
        if (t.text == "Widget") {
            ++widget_hits;
            CHECK(t.next_significant == '*' || t.next_significant == '&' || t.next_significant == 'v');
        }
    }
    CHECK(widget_hits == 3);

    std::vector<Token> hidden = scan_identifiers("// Widget w\n\"Widget\" 'x' /* Widget */");
    for (const auto& t : hidden) CHECK(t.text != "Widget");

    CHECK(can_forward_declare("Widget* w = nullptr;", "Widget"));
    CHECK(can_forward_declare("void f(const Widget& w);", "Widget"));
    CHECK(!can_forward_declare("Widget w;", "Widget"));
    CHECK(!can_forward_declare("std::unique_ptr<Widget> p;", "Widget"));
    CHECK(!can_forward_declare("int x;", "Widget"));
    CHECK(type_name_from_header("include/widget.hpp") == "Widget");
    CHECK(type_name_from_header("foo_bar.h") == "Foo_bar");
}

void test_recommendations() {
    std::vector<SourceDoc> docs{
        {"main.cpp", "#include \"widget.hpp\"\n#include \"widget.hpp\"\nWidget* p;\n"},
        {"widget.hpp", "struct Widget {};\n"}};
    std::vector<IncludeEdge> edges = scan_includes(docs[0].text, "main.cpp");
    IncludeGraph g = build_graph(edges);
    std::vector<Recommendation> recs = recommendations(g, edges, docs, find_cycles(g));
    CHECK(recs.size() == 2);
    CHECK(recs[0].kind == Recommendation::Kind::RemoveDuplicate);
    CHECK(recs[1].kind == Recommendation::Kind::ForwardDeclare);
    CHECK(recs[1].header == "widget.hpp");
}

void test_analyze_and_render() {
    std::vector<SourceDoc> docs{
        {"b.hpp", "#include \"a.hpp\"\n#include \"a.hpp\"\n"},
        {"a.hpp", "#include \"b.hpp\"\n"}};
    Analysis a = analyze(docs);
    CHECK(a.docs[0].path == "a.hpp");
    CHECK(a.edges.size() == 3);
    CHECK(a.cycles.size() == 1);
    std::string text = render_text(a, 5);
    std::string again = render_text(analyze(docs), 5);
    CHECK(text == again);
    CHECK(text.find("Cycles: 1") != std::string::npos);
    CHECK(text.find("[remove-duplicate] b.hpp -> a.hpp") != std::string::npos);
    std::string json = render_json(a, 5);
    CHECK(json.find("\"schema\": 1") != std::string::npos);
    CHECK(json.find("\"kind\": \"break-cycle\"") != std::string::npos);
}

void test_json_escaping() {
    std::vector<SourceDoc> docs{{"a.cpp", "#include \"weird\\name.hpp\"\n"}};
    Analysis a = analyze(docs);
    std::string json = render_json(a, 3);
    CHECK(json.find("weird\\\\name.hpp") != std::string::npos);
}

void test_cli() {
    using namespace knot::shell;
    char* only[] = {const_cast<char*>("knot")};
    CHECK(parse_args(1, only).kind == CommandKind::Help);

    char* argv0 = const_cast<char*>("knot");
    char* argv1 = const_cast<char*>("scan");
    char* argv2 = const_cast<char*>("src");
    char* argv3 = const_cast<char*>("--format");
    char* argv4 = const_cast<char*>("json");
    char* argv5 = const_cast<char*>("--top");
    char* argv6 = const_cast<char*>("3");
    char* argv7 = const_cast<char*>("--fail-on-cycle");
    char* argv[] = {argv0, argv1, argv2, argv3, argv4, argv5, argv6, argv7};
    Command c = parse_args(8, argv);
    CHECK(c.kind == CommandKind::Scan);
    CHECK(c.root == "src");
    CHECK(c.format == "json");
    CHECK(c.top == 3);
    CHECK(c.fail_on_cycle);

    char* bad[] = {argv0, argv1, argv2, argv3, const_cast<char*>("yaml")};
    CHECK(!parse_args(5, bad).error.empty());
    char* badtop[] = {argv0, argv1, argv2, argv5, const_cast<char*>("0")};
    CHECK(!parse_args(5, badtop).error.empty());
    char* unknown[] = {argv0, const_cast<char*>("frobnicate")};
    CHECK(!parse_args(2, unknown).error.empty());
}

void test_json_parser() {
    JsonResult r = parse_json("{\"a\": 1, \"b\": [\"x\", true, null], \"c\": {\"d\": -2}}");
    CHECK(r.ok);
    CHECK(json_member(r.value, "a")->number == 1);
    const JsonValue* b = json_member(r.value, "b");
    CHECK(b != nullptr && b->type == JsonType::Array && b->array.size() == 3);
    CHECK(json_member(*json_member(r.value, "c"), "d")->number == -2);
    CHECK(json_member(r.value, "b")->array[1].boolean);
    CHECK(!parse_json("{\"x\":").ok);
    CHECK(!parse_json("").ok);
    CHECK(parse_json("\"a\\nb\"").value.text == "a\nb");
    CHECK(json_type_name(JsonType::Array) == "array");
}

void test_snapshot_roundtrip() {
    std::vector<SourceDoc> docs{
        {"b.hpp", "#include \"a.hpp\"\n"},
        {"a.hpp", "#include \"b.hpp\"\n"},
        {"app.cpp", "#include \"a.hpp\"\n#include \"a.hpp\"\nWidget* p;\n"}};
    Analysis a = analyze(docs);
    Snapshot original = snapshot_from_analysis(a);
    Snapshot restored;
    std::string error;
    CHECK(snapshot_from_json(render_json(a, 10), restored, error));
    CHECK(original.cycles == restored.cycles);
    CHECK(original.costs == restored.costs);
    CHECK(original.hotspots == restored.hotspots);
    CHECK(original.recommendations.size() == restored.recommendations.size());
    Snapshot bad;
    CHECK(!snapshot_from_json("not json", bad, error));
}

void test_diff() {
    Snapshot before;
    before.costs["app.cpp"] = 3;
    before.hotspots["common.hpp"] = 2;
    Snapshot after = before;
    after.costs["app.cpp"] = 5;
    after.cycles.push_back({"a.hpp", "b.hpp", "a.hpp"});
    DiffReport growth = diff_snapshots(before, after);
    CHECK(growth.regressions);
    CHECK(growth.entries.size() == 2);
    bool saw_cost = false;
    bool saw_cycle = false;
    for (const auto& e : growth.entries) {
        if (e.kind == DiffEntry::Kind::CostUp) saw_cost = true;
        if (e.kind == DiffEntry::Kind::CycleAdded) saw_cycle = true;
    }
    CHECK(saw_cost && saw_cycle);

    Snapshot improved = before;
    improved.costs["app.cpp"] = 1;
    DiffReport better = diff_snapshots(before, improved);
    CHECK(!better.regressions);
    CHECK(better.entries.size() == 1);
    CHECK(better.entries[0].kind == DiffEntry::Kind::CostDown);
    CHECK(render_diff_text(better).find("[cost-down] app.cpp : 3 -> 1") != std::string::npos);
    CHECK(render_diff_json(growth).find("\"regression\": true") != std::string::npos);
}

void test_paths() {
    IncludeGraph g = build_graph({
        {"app.cpp", "a.hpp", 1}, {"app.cpp", "direct.hpp", 2},
        {"a.hpp", "b.hpp", 1}, {"b.hpp", "c.hpp", 1},
        {"direct.hpp", "c.hpp", 1}});
    std::vector<std::string> p = shortest_path(g, "app.cpp", "c.hpp");
    CHECK(p == std::vector<std::string>({"app.cpp", "direct.hpp", "c.hpp"}));
    std::vector<std::string> best = best_path_to(g, "c.hpp");
    CHECK(best == p);
    CHECK(shortest_path(g, "a.hpp", "missing.hpp").empty());
    CHECK(shortest_path(g, "c.hpp", "app.cpp").empty());
    CHECK(render_path_text("c.hpp", p).find("app.cpp -> direct.hpp -> c.hpp") != std::string::npos);
    CHECK(render_path_json("c.hpp", p).find("\"path\": [") != std::string::npos);
}

void test_cli_extensions() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* why = const_cast<char*>("why");
    char* root = const_cast<char*>("src");
    char* target = const_cast<char*>("c.hpp");
    char* from = const_cast<char*>("--from");
    char* file = const_cast<char*>("app.cpp");
    char* argv[] = {argv0, why, root, target, from, file};
    Command c = parse_args(6, argv);
    CHECK(c.kind == CommandKind::Why);
    CHECK(c.root == "src" && c.target == "c.hpp" && c.from == "app.cpp");

    char* diff = const_cast<char*>("diff");
    char* baseline = const_cast<char*>("--baseline");
    char* basefile = const_cast<char*>("base.json");
    char* current = const_cast<char*>("--current");
    char* regress = const_cast<char*>("--fail-on-regression");
    char* dargv[] = {argv0, diff, baseline, basefile, current, root, regress};
    Command d = parse_args(7, dargv);
    CHECK(d.kind == CommandKind::Diff);
    CHECK(d.baseline == "base.json" && d.root == "src" && d.fail_on_regression);
    CHECK(d.error.empty());

    char* bad[] = {argv0, diff, baseline, basefile};
    CHECK(!parse_args(4, bad).error.empty());
}

void test_absolute_root_resolution() {
    std::vector<SourceDoc> docs{
        {"C:/tree/app.cpp", "#include \"a.hpp\"\n"},
        {"C:/tree/a.hpp", "#include \"b.hpp\"\n"},
        {"C:/tree/b.hpp", "struct B {};\n"}};
    Analysis a = analyze(docs);
    CHECK(a.graph.ids.size() == 3);
    CHECK(a.edges.size() == 2);
    CHECK(resolve_node(a.graph, "b.hpp") == "C:/tree/b.hpp");
    std::vector<std::string> p = shortest_path(a.graph, "C:/tree/app.cpp", "C:/tree/b.hpp");
    CHECK(p.size() == 3);
    CHECK(best_path_to(a.graph, "C:/tree/b.hpp") == p);

    std::vector<SourceDoc> cyclic{
        {"C:/tree/app.cpp", "#include \"a.hpp\"\n"},
        {"C:/tree/a.hpp", "#include \"b.hpp\"\n"},
        {"C:/tree/b.hpp", "#include \"a.hpp\"\n"}};
    CHECK(analyze(cyclic).cycles.size() == 1);
}

void test_impact() {
    IncludeGraph g = build_graph({
        {"app.cpp", "a.hpp", 1}, {"app.cpp", "direct.hpp", 2},
        {"a.hpp", "b.hpp", 1}, {"direct.hpp", "b.hpp", 1},
        {"b.hpp", "core.hpp", 1}});
    std::vector<std::string> affected = impact_set(g, "core.hpp");
    CHECK(affected.size() == 4);
    CHECK(affected[0] == "a.hpp" && affected[3] == "direct.hpp");
    CHECK(impact_set(g, "missing.hpp").empty());
    ImpactState s = impact_init(g, "core.hpp");
    CHECK(s.marks[g.index.at("core.hpp")] == ImpactMark::Seen);
    int steps = 0;
    while (!s.done && steps++ < 100) s = impact_step(s, g, reverse_graph(g));
    CHECK(s.done && steps < 100);
    CHECK(render_impact_text("core.hpp", affected).find("4 affected node(s)") != std::string::npos);
    CHECK(render_impact_json("core.hpp", affected).find("\"impact\": [") != std::string::npos);
}

void test_export() {
    std::vector<SourceDoc> docs{
        {"app.cpp", "#include \"a.hpp\"\n#include <vector>\n"},
        {"a.hpp", "struct A {};\n"}};
    Analysis a = analyze(docs);
    std::string dot = render_dot(a);
    std::string mermaid = render_mermaid(a);
    CHECK(dot.find("digraph knot {") == 0);
    CHECK(dot.find("\"app.cpp\" -> \"a.hpp\";") != std::string::npos);
    CHECK(mermaid.find("graph TD") == 0);
    CHECK(mermaid.find(" --> n") != std::string::npos);
    CHECK(render_dot(analyze(docs)) == dot);
    CHECK(render_mermaid(analyze(docs)) == mermaid);
    std::vector<SourceDoc> quoted{{"a.cpp", "#include \"weird\\name.hpp\"\n"}};
    CHECK(render_dot(analyze(quoted)).find("weird\\\\name.hpp") != std::string::npos);
}

void test_cli_wave3() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* graph = const_cast<char*>("graph");
    char* dir = const_cast<char*>("src");
    char* fmt = const_cast<char*>("--format");
    char* mermaid = const_cast<char*>("mermaid");
    char* gargv[] = {argv0, graph, dir, fmt, mermaid};
    Command g = parse_args(5, gargv);
    CHECK(g.kind == CommandKind::Graph && g.format == "mermaid" && g.error.empty());
    char* default_graph[] = {argv0, graph, dir};
    CHECK(parse_args(3, default_graph).format == "dot");
    char* bad_graph[] = {argv0, graph, dir, fmt, const_cast<char*>("json")};
    CHECK(!parse_args(5, bad_graph).error.empty());

    char* impact = const_cast<char*>("impact");
    char* target = const_cast<char*>("core.hpp");
    char* iargv[] = {argv0, impact, dir, target};
    Command i = parse_args(4, iargv);
    CHECK(i.kind == CommandKind::Impact && i.target == "core.hpp" && i.error.empty());
}

void test_ignore_rules() {
    IgnoreRules rules = parse_ignore(
        "# generated\nbuild/\n*.pb.h\nthird_party/**\n!third_party/keep.h\n");
    CHECK(rules.size() == 4);
    CHECK(is_ignored(rules, "build/gen.cpp"));
    CHECK(is_ignored(rules, "src/lib/a.pb.h"));
    CHECK(is_ignored(rules, "third_party/foo/bar.hpp"));
    CHECK(!is_ignored(rules, "third_party/keep.h"));
    CHECK(!is_ignored(rules, "src/main.cpp"));

    CHECK(glob_match("*.hpp", "a.hpp"));
    CHECK(!glob_match("*.hpp", "dir/a.hpp"));
    CHECK(glob_match("**/a.hpp", "dir/a.hpp"));
    CHECK(!glob_match("a?b", "ac/b"));
    CHECK(glob_match("a?b", "acb"));
    GlobState s;
    int steps = 0;
    while (!s.matched && !s.failed && steps++ < 100) s = glob_step(s, "**/a.hpp", "dir/a.hpp");
    CHECK(s.matched && steps < 100);
}

void test_cli_ignore() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* scan = const_cast<char*>("scan");
    char* dir = const_cast<char*>("src");
    char* ignore = const_cast<char*>("--ignore");
    char* file = const_cast<char*>(".knotignore");
    char* argv[] = {argv0, scan, dir, ignore, file};
    Command c = parse_args(5, argv);
    CHECK(c.kind == CommandKind::Scan && c.ignore_file == ".knotignore" && c.error.empty());
}

void test_policy() {
    Policy p = parse_policy("# ci policy\nfail_on_cycle = false\nmax_cost = 3\n"
                            "max_hotspot = 2\nbaseline = base.json\n");
    CHECK(!p.fail_on_cycle);
    CHECK(p.max_cost == 3 && p.max_hotspot == 2);
    CHECK(p.baseline == "base.json");
    CHECK(parse_policy("fail_on_cycle = true").fail_on_cycle);
    CHECK(parse_policy("max_cost = nope").max_cost == 0);

    std::vector<SourceDoc> docs{
        {"a.hpp", "#include \"b.hpp\"\n"},
        {"b.hpp", "#include \"a.hpp\"\n"},
        {"app.cpp", "#include \"a.hpp\"\n"}};
    Analysis a = analyze(docs);
    Policy strict;
    strict.fail_on_cycle = true;
    strict.max_cost = 1;
    std::vector<Violation> violations = evaluate_policy(a, strict, DiffReport{});
    CHECK(violations.size() >= 2);
    bool saw_cycle = false;
    bool saw_cost = false;
    for (const auto& v : violations) {
        if (v.kind == ViolationKind::Cycle) saw_cycle = true;
        if (v.kind == ViolationKind::Cost) saw_cost = true;
    }
    CHECK(saw_cycle && saw_cost);
    CHECK(evaluate_policy(a, Policy{false, 0, 0, ""}, DiffReport{}).empty());

    DiffReport regression;
    regression.regressions = true;
    DiffEntry entry;
    entry.kind = DiffEntry::Kind::CostUp;
    regression.entries.push_back(entry);
    std::vector<Violation> with_regression = evaluate_policy(a, Policy{false, 0, 0, ""}, regression);
    CHECK(with_regression.size() == 1);
    CHECK(with_regression[0].kind == ViolationKind::Regression);
    CHECK(render_violations_text(violations).find("violation(s)") != std::string::npos);
    CHECK(render_violations_json(with_regression).find("\"kind\": \"regression\"") != std::string::npos);
}

void test_cli_policy() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* check_cmd = const_cast<char*>("check");
    char* dir = const_cast<char*>("src");
    char* policy = const_cast<char*>("--policy");
    char* file = const_cast<char*>(".knot-policy");
    char* argv[] = {argv0, check_cmd, dir, policy, file};
    Command c = parse_args(5, argv);
    CHECK(c.kind == CommandKind::Check && c.policy_file == ".knot-policy" && c.error.empty());
}

void test_missing_includes() {
    std::vector<SourceDoc> missing{
        {"widget.hpp", "struct Widget {};\n"},
        {"app.cpp", "Widget w;\n"}};
    Analysis a = analyze(missing);
    bool found = false;
    for (const auto& rec : a.recommendations) {
        if (rec.kind == Recommendation::Kind::AddInclude) {
            found = true;
            CHECK(rec.file == "app.cpp" && rec.header == "widget.hpp");
        }
    }
    CHECK(found);
    CHECK(render_text(a, 5).find("[add-include]") != std::string::npos);

    std::vector<SourceDoc> included{
        {"widget.hpp", "struct Widget {};\n"},
        {"app.cpp", "#include \"widget.hpp\"\nWidget w;\n"}};
    Analysis b = analyze(included);
    for (const auto& rec : b.recommendations) {
        CHECK(rec.kind != Recommendation::Kind::AddInclude);
    }
    CHECK(missing_includes(b).empty());
    CHECK(missing_includes(a).size() == 1);
}

void test_scaffold() {
    std::string ignore_text = default_ignore_text();
    std::string policy_text = default_policy_text();
    CHECK(ignore_text.find("build/") != std::string::npos);
    CHECK(parse_ignore(ignore_text).size() >= 3);
    Policy policy = parse_policy(policy_text);
    CHECK(policy.fail_on_cycle && policy.max_cost == 0 && policy.baseline.empty());
}

void test_cli_init() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* init = const_cast<char*>("init");
    char* dir = const_cast<char*>("src");
    char* force = const_cast<char*>("--force");
    char* argv[] = {argv0, init, dir, force};
    Command c = parse_args(4, argv);
    CHECK(c.kind == CommandKind::Init && c.force && c.root == "src" && c.error.empty());
}

void test_baseline_completeness() {
    std::vector<SourceDoc> docs;
    std::string includes;
    for (int i = 0; i < 15; ++i) {
        std::string name = "h" + std::to_string(i) + ".hpp";
        docs.push_back(SourceDoc{name, "struct T {};\n"});
        includes += "#include \"" + name + "\"\n";
    }
    docs.push_back(SourceDoc{"app.cpp", includes});
    Analysis a = analyze(docs);
    CHECK(a.graph.ids.size() > 10);
    Snapshot restored;
    std::string error;
    CHECK(snapshot_from_json(render_baseline_json(a), restored, error));
    CHECK(restored.costs.size() == a.graph.ids.size());
    CHECK(render_json(a, 10).find("\"costs\": [") != std::string::npos);
}

void test_cli_baseline() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* baseline = const_cast<char*>("baseline");
    char* dir = const_cast<char*>("src");
    char* file = const_cast<char*>("base.json");
    char* force = const_cast<char*>("--force");
    char* argv[] = {argv0, baseline, dir, file, force};
    Command c = parse_args(5, argv);
    CHECK(c.kind == CommandKind::Baseline && c.root == "src" &&
          c.baseline == "base.json" && c.force && c.error.empty());
    char* missing[] = {argv0, baseline, dir};
    CHECK(!parse_args(3, missing).error.empty());
}

void test_stats() {
    std::vector<SourceDoc> docs{
        {"app.cpp", "#include \"a.hpp\"\n#include <vector>\n"},
        {"a.hpp", "#include \"b.hpp\"\n"},
        {"b.hpp", "#include \"c.hpp\"\n"},
        {"c.hpp", "struct C {};\n"}};
    Stats stats = compute_stats(analyze(docs));
    CHECK(stats.files == 4 && stats.translation_units == 1 && stats.headers == 3);
    CHECK(stats.edges == 4 && stats.internal_edges == 3 && stats.external_edges == 1);
    CHECK(stats.max_depth == 3);
    CHECK(stats.deepest_chain == std::vector<std::string>({"app.cpp", "a.hpp", "b.hpp", "c.hpp"}));
    CHECK(render_stats_text(stats).find("3 internal, 1 external") != std::string::npos);
    CHECK(render_stats_json(stats).find("\"max_depth\": 3") != std::string::npos);
}

void test_short_names() {
    std::vector<SourceDoc> docs{
        {"a.h", "struct A {};\n"},
        {"app.cpp", "#include \"a.h\"\n"}};
    Analysis a = analyze(docs);
    CHECK(a.edges.size() == 1);
    CHECK(compute_stats(a).files == 2);
    CHECK(analyze({{"x.y", "struct X {};\n"},
                   {"app.cpp", "#include \"x.y\"\n"}}).graph.ids.size() == 2);
}

void test_cli_stats() {
    using namespace knot::shell;
    char* argv0 = const_cast<char*>("knot");
    char* stats = const_cast<char*>("stats");
    char* dir = const_cast<char*>("src");
    char* argv[] = {argv0, stats, dir};
    Command c = parse_args(3, argv);
    CHECK(c.kind == CommandKind::Stats && c.root == "src" && c.error.empty());
}

}  // namespace

int main() {
    test_lexer_states();
    test_scan_includes();
    test_lexer_edge_cases();
    test_graph_build();
    test_cycles();
    test_dfs_step_machine();
    test_costs_and_fan_in();
    test_identifier_machine();
    test_recommendations();
    test_analyze_and_render();
    test_json_escaping();
    test_cli();
    test_json_parser();
    test_snapshot_roundtrip();
    test_diff();
    test_paths();
    test_cli_extensions();
    test_absolute_root_resolution();
    test_impact();
    test_export();
    test_cli_wave3();
    test_ignore_rules();
    test_cli_ignore();
    test_policy();
    test_cli_policy();
    test_missing_includes();
    test_scaffold();
    test_cli_init();
    test_baseline_completeness();
    test_cli_baseline();
    test_stats();
    test_short_names();
    test_cli_stats();
    std::cout << "knot: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
