// Zero-dependency test harness. One binary, no framework, no network, no sleeps.
#include <iostream>
#include <string>
#include <vector>

#include "core/graph.hpp"
#include "core/lexer.hpp"
#include "core/report.hpp"
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
    std::cout << "knot: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
