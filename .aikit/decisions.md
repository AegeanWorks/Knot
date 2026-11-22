# Decisions

Why this project is built the way it is, oldest first. One entry per decision that changes a
contract, a dependency, a data format or the architecture; IDs are consecutive and never reused.

<!-- Entry format:
### ADR-NNN: <decision in one line>
- **Date**: YYYY-MM-DD
- **Status**: Accepted
- **Context**: <what forced the decision>
- **Decision**: <what was decided>
- **Consequences**: <what follows, including what it costs>
-->

### ADR-001: Functional core, imperative shell and explicit state machines for Knot
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: User directive: a unique C++17 tool for programmers, with Functional Core / Imperative Shell, I/O at the edges and Explicit State Machines treated as hard architectural constraints; no third-party dependencies.
- **Decision**: src/core is pure C++17 with no I/O; the include lexer, iterative DFS cycle detector, identifier scanner and cost worklist are enum state machines with value-in/value-out transition functions. src/shell owns all I/O in files.cpp and a pure argv decoder in cli.cpp; main.cpp only wires parse -> read -> analyze -> write. Reports are deterministic by sorting every input and output. AIKit's ADR-315 Python compat scaffold is retained as tool infrastructure, not part of the implementation.
- **Consequences**: Every state transition is directly unit-testable without files or processes; the same tree always produces byte-identical reports so Knot can be a CI gate; the two scaffold .py files remain in the project for the compatibility gate and are documented as such in the README.

### ADR-002: Add path queries and golden-baseline regression diffing
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Cycles and cost reports identify a problem but not the exact include chain, and nothing detects that coupling grew between revisions; the user asked for continued autonomous implementation of programmer-facing features under the same architectural constraints.
- **Decision**: Added a pure BFS state machine for shortest include paths (knot why), a pure JSON lexer/parser for Knot's own versioned report schema, and a pure snapshot/diff model (knot diff) with CI regression flags. Relative includes are resolved to scanned documents by path suffix inside analyze(), so absolute roots do not duplicate nodes. All new I/O stays in files.cpp.
- **Consequences**: Coupling regressions can be blocked in CI with --fail-on-regression, and 'why is this header here' is answerable without a compiler; a snapshot round-trip property test and 41 new checks protect the new code; the version string moved to 0.2.0. A baseline produced by Knot 0.1.x contains duplicate nodes and must be regenerated.

### ADR-003: Add reverse impact analysis and DOT/Mermaid graph export
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Maintainers need the blast radius of a header change and a reviewable picture of coupling; the user asked for continued autonomous features under the same architectural constraints.
- **Decision**: reverse_graph plus an explicit impact_step BFS compute the sorted set of nodes that transitively include a target; render_dot and render_mermaid emit deterministic, escaped graph documents. Both live in the pure core; the shell only loads sources, resolves the user's node name and writes the result.
- **Consequences**: Impact sets and exports are reproducible enough to attach to CI artifacts and documentation; 18 new checks cover reverse BFS, escaping and format validation; no new dependency or I/O path was introduced.

### ADR-004: Add .knotignore rules at the filesystem edge
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Scans of real repositories included generated and vendored code because the skip list was hardcoded; there was no way to scope an analysis to owned code.
- **Decision**: A pure glob_step state machine supports '*', '?', '**', directory rules and '!' negation with last-match-wins semantics; parse_ignore/is_ignored live in the core, while files.cpp applies them to paths relative to the scan root. Defaults (.git/, .aikit/, node_modules/, build/) are rules and can be negated.
- **Consequences**: Every subcommand now sees the same scoped tree; a fixture smoke test showed three files without the rule file and two with it; 13 new checks cover glob semantics and negation; no third-party dependency was added.

### ADR-005: Add a repository-owned CI policy via knot check
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: CI runners previously had to remember a changing set of CLI flags, so a project's coupling policy was not versioned with its code.
- **Decision**: A pure parse_policy/evaluate_policy model reads a small key=value .knot-policy file and produces a sorted violation list; 'knot check <dir>' applies it together with the ignore rules and an optional baseline, exiting 1 on any violation.
- **Consequences**: One stable CI command with a versioned policy; a fixture with a two-header cycle produced one violation and exit 1, while a clean tree produced zero and exit 0; 13 new checks bring the suite to 159; no new dependency or I/O path was introduced.

### ADR-006: Add advisory missing-include detection
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Using a type whose header is not included is a common C++ error that surfaces far from its cause; Knot already has the identifier scanner and path query needed to flag it.
- **Decision**: missing_includes is a pure function that compares identifiers used in a translation unit with type names derived from scanned headers, and emits an advisory add-include recommendation when the defining header is not reachable from that unit. It is advisory: unlike cycles it is not a policy violation by default.
- **Consequences**: A fixture with 'Widget w;' and no include produced one add-include line; adding the include removed it; five new checks bring the suite to 164. Candidates are restricted to scanned headers and exact identifier matches to limit false positives.

### ADR-007: Add knot init onboarding scaffold
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: New users had to learn the policy and ignore syntax before getting a useful result; the project brief requires zero-friction onboarding.
- **Decision**: Pure generators produce starter .knotignore and .knot-policy content; the shell writes them at the I/O edge and refuses to overwrite without --force.
- **Consequences**: One command produces an editable, documented starting configuration; a temp-directory smoke showed exit 0 on first run, exit 2 on a second run without --force and exit 0 with it; four new checks bring the suite to 168.

### ADR-008: Add a complete-snapshot baseline command
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: Baselines produced by hand with shell redirection could be truncated by top-N output or overwritten accidentally, making regression checks unreliable.
- **Decision**: render_baseline_json emits a complete snapshot by passing the graph size as the top-N limit; 'knot baseline <dir> <file>' writes it at the I/O edge with an overwrite guard requiring --force.
- **Consequences**: A 16-node fixture round-tripped all 16 cost entries through snapshot_from_json; a smoke test showed exit 0 on first write, exit 2 on an unforced second write, and a clean diff against the saved file; six new checks bring the suite to 174.

### ADR-009: Add project stats and fix short-name suffix underflow
- **Date**: 2026-11-22
- **Status**: Accepted
- **Context**: A project-level metrics view was missing, and the first real scan of Knot's own root crashed in a suffix helper that underflowed on short names.
- **Decision**: compute_stats classifies files and edges and finds the deepest include chain with the existing bounded BFS; the graph.cpp suffix helpers now use length-guarded comparisons instead of substr(size - N).
- **Consequences**: A debug build under gdb proved the fix, and the release smoke now reports 33 files, 135 edges and a 4-hop deepest chain on Knot itself; a regression test covers one-character and non-header names; 184 checks pass.
