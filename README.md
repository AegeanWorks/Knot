# Knot

[![Built with AIKit](https://img.shields.io/badge/built%20with-AIKit-6f42c1)](https://github.com/AegeanWorks/AIKit)

Knot is a zero-dependency C++17 include-graph analyzer for programmers: it parses include directives with an explicit state machine, detects cycles, quantifies transitive header cost, traces the exact include chain that pulls a header in, reports the reverse blast radius of changing a header, diffs a tree against a recorded baseline to catch coupling regressions, exports the graph to DOT/Mermaid, honours `.knotignore` rules for generated or vendor code, enforces a repository-owned policy with `knot check`, flags used types whose header is not included, and recommends forward declarations as deterministic, actionable reports.

## ⚡ 30-Second Cold-Start Quick Reference

- **Runtime Prerequisites**: C++17 toolchain — GNU g++ 12 or newer, MSVC 19.30 or newer, or Clang 14 or newer. Verified on MinGW g++ 16.2. No third-party libraries.
- **Canonical Regression Shield Gate**:
  ```bash
  scripts\test.cmd
  ```

### What it does

Point Knot at a C/C++ tree and it answers ten questions a compiler will not
tell you directly:

1. Which headers form an include cycle? (`cycles`)
2. How expensive is each file — how many distinct headers does it pull in? (`scan`, cost column)
3. Which headers are included far too often, and which includes could become a
   forward declaration? (`hotspots`, `scan`, recommendations)
4. *Why* is a header here — what is the shortest include chain from a translation
   unit to it? (`why`)
5. Did coupling grow since a recorded baseline? (`diff --baseline ... --current ...`)
6. What breaks if I change this header — the reverse transitive closure? (`impact`)
7. Can I review the whole graph as a picture? (`graph --format dot|mermaid`)
8. Does the tree satisfy my project's coupling policy? (`knot check`, driven by
   an in-repository `.knot-policy`)
9. Did I forget to include the header that defines a type I used? (`scan`,
   advisory `add-include` recommendations)
10. How deep and how coupled is the tree overall? (`stats`, including the
    deterministic deepest include chain)

Output is deterministic: input documents are sorted by path, graph nodes and
recommendations are sorted by stable keys, and the JSON schema is versioned.
The same tree always produces the same report, so Knot can be a CI gate.

### Usage

```text
knot scan <dir> [--format text|json] [--top N] [--fail-on-cycle]
                  [--fail-on-cost N] [--fail-on-hotspot N]
knot cycles <dir> [--format text|json]
knot hotspots <dir> [--format text|json] [--top N]
knot stats <dir> [--format text|json]
knot csv <dir> [--kind costs|hotspots|recommendations]
knot report <dir> <file> [--force]
knot why <dir> <header> [--from <file>] [--format text|json]
knot impact <dir> <header> [--format text|json]
knot graph <dir> [--format dot|mermaid]
knot check <dir> [--policy <file>] [--format text|json]
knot init <dir> [--force]
knot baseline <dir> <file> [--force]
knot diff --baseline <file> --current <dir> [--format text|json]
                  [--fail-on-regression]
knot demo
knot help | knot --help | knot --version
```

Every scan command accepts repeatable `--root <dir>` to build one graph across
several source trees. With more than one root, paths are prefixed by the root's
directory name (`one/app.cpp`, `two/app.cpp`) so identical relative paths do
not collide. With one root, paths are unchanged.

A `.knotignore` file at the scan root is applied automatically; use
`--ignore <file>` for another rule file. Rules support `#` comments, `*` and
`?` (no `/` crossing), `**` (crosses `/`), directory rules like `vendor/`,
and `!` negation (last match wins). `.git/`, `.aikit/`, `node_modules/` and
`build/` are ignored by default and can be re-included with `!`.

`knot init <dir>` writes starter `.knotignore` and `.knot-policy` files; it
refuses to overwrite existing ones unless `--force` is given.

`knot check` applies a repository-owned `.knot-policy` with the keys
`fail_on_cycle`, `max_cost`, `max_hotspot` and `baseline` (`key = value` per
line). Use `--policy <file>` to select another policy. Its violations are a
sorted, deterministic list with text and JSON output.

CI flags exit `1` on a finding: `--fail-on-cycle`, `--fail-on-cost N`,
`--fail-on-hotspot N`, `--fail-on-regression`. A baseline is any
`knot scan <dir> --format json` report saved to a file; `knot baseline <dir>
<file>` writes a complete one without top-N truncation and refuses to overwrite
unless `--force` is given.
Exit codes: `0` clean, `1` finding/regression, `2` usage/bad baseline, `3` I/O error.

```text
$ knot demo
Knot: 3 file(s), 5 include(s), 4 node(s)
Cycles: 1
theme.hpp -> widget.hpp -> theme.hpp
...
Recommendations (3):
  [break-cycle] widget.hpp -> theme.hpp : closes an include cycle
  [remove-duplicate] app.cpp -> widget.hpp : included 2 times
  [forward-declare] app.cpp -> widget.hpp : Widget is used only as pointer or reference
```

### Architecture (hard constraints)

- **Functional core** (`src/core/`): pure C++17. No file, console, clock or
  network access. `lex_step`, `dfs_step` and `ident_step` are value-in,
  value-out transition functions folded by `scan_includes`, `find_cycles` and
  `scan_identifiers`. `analyze` maps source documents to an immutable `Analysis`.
- **Imperative shell** (`src/shell/`): owns the only I/O boundary in
  `files.cpp` and the CLI decoder in `cli.cpp`. `main.cpp` parses, reads at the
  edge, calls the pure core, and writes at the edge.
- **Explicit state machines**: the include lexer (15 states), the iterative DFS
  cycle detector (white/gray/black marks + frame stack), the identifier scanner,
  the transitive-cost worklist, the BFS path search (`path_step`), the reverse
  impact BFS (`impact_step`), the JSON lexer (`json_lex_step`) and the glob
  matcher (`glob_step`) and the policy evaluator (`parse_policy`,
  `evaluate_policy`). `missing_includes` reuses the identifier scanner and the
  path query to flag a used type whose header is not reachable. No hidden
  recursion drives analysis state.
- **Pure snapshot diff**: `render_json` output parses back through `parse_json`
  into the same `Snapshot` (property test), and `diff_snapshots` reports added
  cycles, cost/hotspot changes and recommendation changes deterministically.
- **Include resolution**: relative quoted includes are matched to scanned
  documents by path suffix before graph construction, so an absolute scan root
  never creates two nodes for one file.
- **Reverse impact and export**: `reverse_graph` + `impact_step` compute the
  sorted set of nodes that transitively include a header; `render_dot` and
  `render_mermaid` emit stable, escaped graph documents.
- **Ignore rules**: `parse_ignore` and `is_ignored` are pure; the filesystem
  walker applies them to paths relative to the scan root, so one rule set is
  shared by every subcommand.
- Tests in `tests/test_all.cpp` call the transition functions directly and pin
  their outputs; 703 checks include a seeded 500-case generated-input harness
  that drives every state machine and the whole pipeline, plus comments,
  strings, malformed directives,
  self/2/3-node cycles, duplicate recommendations, JSON escaping, snapshot
  round-trips, path queries, regression verdicts and CLI errors.

### 🛠️ AIKit Commands (run from the AIKit folder)

| Role | Command |
|---|---|
| Status, task and gates | `python aikit.py status knot` |
| Phase 4: every gate | `python aikit.py check knot` |
| Repair memory and map, then check | `python aikit.py fix knot` |
| Resume an interrupted task | `python aikit.py resume knot` |

## 📦 What's Inside

```text
knot/
├── .gitattributes            <- LF line endings on every platform
├── .gitignore                <- transient AIKit state excluded
├── LICENSE                   <- Aegean License 1.1 (AL-1.1): free personal use
├── COMMERCIAL.md             <- Commercial licensing terms
├── LICENSES/                 <- REUSE license texts
├── REUSE.toml                <- Machine-readable SPDX metadata
├── README.md
├── src/                      <- C++17 implementation: pure core + shell
├── scripts/                  <- build and test entry points
├── tests/                    <- zero-dependency test harness
├── src/core/types.hpp        <- immutable value types (pure)
├── src/core/lexer.hpp        <- 15-state include lexer declaration
├── src/core/lexer.cpp        <- lex_step transition + fold
├── src/core/graph.hpp        <- graph and state-machine declarations
├── src/core/graph.cpp        <- DFS, cost, fan-in, heuristic transitions
├── src/core/report.hpp       <- analyze() and renderer declarations
├── src/core/report.cpp       <- pure text/JSON renderers + include resolution
├── src/core/path.hpp         <- BFS path query declarations
├── src/core/path.cpp         <- path_step, resolve_node, path renderers
├── src/core/json.hpp         <- minimal JSON value types
├── src/core/json.cpp         <- JSON lexer state machine + parser
├── src/core/diff.hpp         <- snapshot and diff declarations
├── src/core/diff.cpp         <- snapshot reader, diff verdict, renderers
├── src/core/impact.hpp       <- reverse impact declarations
├── src/core/impact.cpp       <- reverse_graph, impact_step, renderers
├── src/core/export.hpp       <- DOT/Mermaid export declarations
├── src/core/export.cpp       <- pure, escaped graph renderers
├── src/core/ignore.hpp       <- glob and ignore-rule declarations
├── src/core/ignore.cpp       <- glob_step, parse_ignore, is_ignored
├── src/core/policy.hpp       <- CI policy and violation declarations
├── src/core/policy.cpp       <- pure policy parser and evaluator
├── src/core/missing.hpp      <- missing-include advisory declarations
├── src/core/missing.cpp      <- pure used-type / header reachability check
├── src/core/scaffold.hpp     <- starter configuration generators
├── src/core/scaffold.cpp     <- pure .knotignore/.knot-policy content
├── src/core/stats.hpp        <- project metrics declarations
├── src/core/stats.cpp        <- pure counts + deepest chain via BFS
├── src/core/paths.hpp        <- multi-root path qualification
├── src/core/paths.cpp        <- pure qualified-path rule
├── src/core/csv.hpp          <- CSV renderer declarations
├── src/core/csv.cpp          <- pure, correctly escaped CSV tables
├── src/core/html.hpp         <- HTML report declaration
├── src/core/html.cpp         <- pure standalone HTML with strict escaping
├── src/shell/cli.hpp         <- CLI decoder declaration
├── src/shell/cli.cpp         <- pure argv decoding
├── src/shell/files.hpp       <- I/O boundary declaration
├── src/shell/files.cpp       <- file reads, demo fixture, stdout writes
├── src/shell/main.cpp        <- imperative wiring and exit codes
├── scripts/test.cmd          <- canonical regression gate (build + tests + demo)
├── tests/test_all.cpp        <- zero-dependency harness, 690 checks + fuzz
├── inventory.py              <- AIKit compat scaffold (ADR-315), not implementation
├── demo.py                   <- AIKit compat scaffold (ADR-315), not implementation
└── .aikit/                   <- this project's memory: record, ledger, decisions
```

## 📜 License

Free for personal use under the Aegean License 1.1 (AL-1.1); see [LICENSE](LICENSE). Any commercial use needs the Aegean Commercial License 1.0 (ACL-1.0); see [COMMERCIAL.md](COMMERCIAL.md).
