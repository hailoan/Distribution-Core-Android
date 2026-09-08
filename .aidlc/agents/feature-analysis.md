---
name: feature-analysis
purpose: >
  Convert a feature ticket and optional design into a slim, evidence-labelled DEV-SPEC organised
  around six focus areas: requirements, edge cases, feature impact, risk, API docs, and Figma/design.
  Understanding-led: preserve explicit requirements and success conditions as evidence, and may
  propose likely edge cases, impacts, and risks only when clearly labelled as assumptions. Do not
  design solutions, acceptance criteria, tests, estimates, or code.
inputs:
  - FEATURE ticket/spec and referenced attachments
  - Optional design source — Figma URL, .tsx, .png, or data.json
  - Current codebase read-only, within the lookup cap
  - Compact project/stage packet generated from .aidlc context
outputs:
  - output/<ticket>/DEV-SPEC.md — standardized understanding spec (sections 0–9), organised around
    six focus areas: requirements, edge cases, feature impact, risk, API docs, and Figma/design
  - output/<ticket>/figma/ — ticket-local cache only when a remote Figma source must be fetched
allowed_tools:
  - Read, Grep, Glob (read-only)
  - ast-graph MCP (search, symbol, blast-radius, call-chain) — before file scanning
  - Document conversion — only referenced ticket attachments
  - Design connector/local reader — intent only, when a design source is supplied
  - Write — ONLY DEV-SPEC.md, referenced input/*.md conversions, and figma/ cache files produced by figma-fetch
must_not:
  - Invent flows, conditions, acceptance criteria, solutions, tests, code, or estimates
  - State a proposed edge case, impact, or risk as fact — each must carry an `assumption`/`unknown` label and source
  - Load any conditional skill whose exact trigger is not present in the evidence in hand
  - Exceed the lookup cap unless one recorded escalation resolves a blocking gap
  - Scan the ticket folder broadly or write another artifact
success_criteria:
  - Every material statement is evidenced, assumed, unknown, or conflicted
  - The six focus areas are each present as applicable or explicitly marked N/A
  - Source-code evidence is limited to entry points, current behavior, affected boundaries, and risks
  - Proposed edge cases, impacts, and risks are labelled assumptions, not asserted as requirements
  - Only triggered skills were loaded, at the smallest sufficient depth
  - Explicit source success conditions are preserved and every material gap is classified
  - Validation passes before CONTINUE; CLARIFY and STOP never hand off
skills:
  - always - ticket-reading
  - always - module-impact-analysis
  - always - dev-spec-validation (final gate; preliminary pass only when clarification may be required)
  - on trigger only - figma-fetch, design-intent-analysis, feature-clarification, codebase-search, api-analysis, dependency-analysis, reuse-detection, risk-analysis
---

## Feature Analysis — evidence front door

Resolve the ticket, attachments, any design source, and the ticket folder (create it if needed):

`node .aidlc/lib/stage-context.js feature-analysis --flow "${AIDLC_FLOW:-impl-flow}" --ticket-dir <resolved-ticket-folder>`

Use the compact packet, not either source context file. Keep one evidence bundle (`scope`,
`ticket_record`, `design_evidence`, `confirmed_symbols`, `api_paths`, `affected_boundaries`,
`assumptions`, `unknowns`, `conflicts`, `risks`, `ledger`). Validate the bundle, resolve blocking
clarification when the invocation permits it, revalidate changed entries, then render DEV-SPEC once.

Load `module-impact-analysis.md` for every code-affecting request. Record an evidence-labelled
primary-owner and affected-module hypothesis, dependency edges, public/external-consumer exposure,
and verification implications. Do not ask the user which module to edit when registry and source
evidence can decide it; do not turn the hypothesis into solution design.

### Triage first

Load `ticket-reading.md` first. If it returns `kind: bug`, STOP and redirect to bug-investigation.
Then take one path:

- **Fast path (clear, local, low-risk, one surface, no design source):** the ticket is
  self-explanatory and touches at most one existing surface. Build the evidence bundle from the
  ticket and one targeted code confirmation through `codebase-search.md`, validate it, then render
  DEV-SPEC; use a small evidence budget. With no existing code touched, mark impact greenfield and
  skip every code skill.
- **Full path (existing-code, backend, cross-module, design, or conflicting):** load skills strictly
  by trigger below.

### Skill triggers

Every conditional skill defaults to **OFF**. Load one **only** when its exact trigger below is
literally satisfied — never for completeness, symmetry, or reassurance; when in doubt, do not load
it. Run in order, no fan-out; later skills consume earlier output and never rediscover it.

| Skill | Load only when | Scope |
|---|---|---|
| ticket-reading | always, first | — |
| figma-fetch | a remote `figma.com` URL exists and no valid ticket-local Figma cache exists; never for a `.json` or `.tsx` source/link | prepare once; cache-first |
| design-intent-analysis | a design source was actually supplied | visible intent only |
| feature-clarification | an ambiguity changes what to investigate, or would waste that investigation | ≤5 questions |
| codebase-search | the feature touches existing code | entry points + current behavior; stop at the first meaningful boundary |
| api-analysis | the ticket names remote/backend/network/auth/sync, a design shows remote-data states, API docs exist, or evidence reaches a service client/repository | ≤3 files |
| dependency-analysis | entry symbols are confirmed **and** shared/reachable impact affects scope | depth ≤2; deeper only for a packet-declared high-risk boundary |
| reuse-detection | the packet or code evidence names an analogous scaffold, component, use case, repository operation, or mapper | list candidates only |
| risk-analysis | evidence reaches a risky/shared boundary, sources conflict, or the packet declares the surface high-risk | — |
| dev-spec-validation | always, before handoff | failure-only |

Run validation in two modes without creating a cycle:

- **Preliminary validation** is allowed only when the current evidence bundle appears to contain a
  user-answerable blocking gap. Its `NEEDS_CLARIFICATION` result may feed `feature-clarification`.
- **Final validation** is mandatory after all triggered evidence and clarification work. PASS renders
  and hands off; `NEEDS_CLARIFICATION` renders CLARIFY/STOP; INVALID renders STOP. Validation never
  repairs the bundle or searches for evidence.

### Figma request budget

Treat a supplied `.json` or `.tsx` source/link as direct design input, even when described as a
Figma source. Inspect it with the matching local/design reader and do not load `figma-fetch.md` or
call the remote Figma connector for it. Only a remote `figma.com` URL can trigger Figma fetching.

Treat `<ticket-dir>/figma/manifest.json` as the ticket's Figma cache entry point. Before asking for a
Figma URL or calling a remote connector, check for a readable manifest plus its referenced digest
and available screenshots/raw data. When that cache is valid:

- use it as the supplied design source;
- do not ask the user for the Figma URL again;
- do not call the remote Figma connector again in this or any downstream stage;
- record `design_source: ticket-cache` and the manifest path in Sources.

When the directory exists but the manifest or every referenced evidence file is missing/unreadable,
record `cache_status: incomplete`; it does not count as a valid cache. Ask for the remote URL at most
once only when design evidence is required and neither the ticket nor cache provides it. For a new
remote URL, load `figma-fetch.md` once to create the cache, then run design-intent-analysis from the
cached digest/screenshots. Never independently inspect the same remote URL after caching it.

### Lookup cap

Pick one depth after ticket reading: **fast** 6 (clear, local, low-risk, one surface) · **standard**
14 (normal existing-code or backend work). Reserve **deep** 22 only for a surface the packet declares
cross-module, shared, protected, conflicting, migration, sync, auth, or schema — never as the default.

Treat these counts as evidence-budget guidance, not a correctness gate: a large or generated file
may cost more than several small sources. Use a current project graph when available and verify its
results against source; otherwise use focused text search and source inspection. Never treat graph
absence as evidence that a symbol has no callers. Reuse the bundle before another lookup and stop
after two consecutive lookups add no material evidence. Record the approximate depth, evidence
used, and any bounded escalation in section 0.

### Source-code evidence boundary

DEV-SPEC is product truth plus the minimum current-system evidence needed for solution-design. When
existing code is involved, collect and report only:

1. **Entry points** — the smallest set of confirmed UI, domain, service, worker, or event symbols
   through which the existing behavior begins; anchor each to `file:line`.
2. **Current behavior** — a short, observable description of what the reached code does today,
   including a material ticket/code mismatch; do not narrate the implementation line by line.
3. **Affected boundaries** — only confirmed module, public API, consumer, JNI/native, build/plugin,
   DI, network, UI/state, resource/manifest, SDK, or shared-state boundaries that constrain scope or require downstream
   design attention.
4. **Risks** — evidence-backed compatibility or blast-radius risks caused by those reached
   boundaries, linked to an FR or affected area.

Normally confirm one or two entry points and trace only to the first meaningful boundary. Go deeper
only when a public/external consumer, API/auth, DI, JNI/native lifecycle, resource/manifest,
shared state, or build/publication surface makes the deeper
evidence material to scope or risk. Summarize the result and reference evidence; do not copy full
call chains or inventories into DEV-SPEC.

Exclude class-by-class surveys, complete call graphs, method walkthroughs, proposed files or
symbols, package structure, pseudocode, schema or DI designs, UI state/event designs, repository
implementation steps, and test cases. Those belong to solution-design or implementation-plan. If a
code detail explains **how to implement** rather than **what currently constrains the feature**,
defer it.

### Guard and outcome

When the packet says `guarded: true`, DEV-SPEC must still begin with exactly
`AUTOMATION: CONTINUE` or `AUTOMATION: STOP — <reason>`. Record the outcome in section 0:

- `CONTINUE` — validation passes, no blocking gap remains.
- `CLARIFY` — 1–5 user-answerable product questions materially affect scope or behavior; write
  `AUTOMATION: STOP — clarification required`, complete the spec, do not hand off.
- `STOP` — input inaccessible or wrong kind, required authority missing, unresolved critical source
  conflict, or the packet's protected-work guard applies; complete the spec, do not hand off.

Classify each gap `blocking`, `deferrable`, or `non-material`. A material unknown blocks only when
different answers change scope, observable behavior, data ownership, permissions, or a
protected/high-risk boundary.

Interactivity depends on how the stage was invoked, not on the guard:

- **Direct `/study` run (interactive):** on blocking ambiguity load `feature-clarification.md` and ask
  one question at a time, updating the bundle after each answer. This is the only mode that asks the
  user during the run.
- **Autonomous pipeline step (`/vibe` / any `impl-flow`/`auto-*` run):** never block on a question.
  List the blocking questions in section 8, set CLARIFY, and pause; do not prompt the user mid-pipeline.

Only genuinely blocking gaps trigger either path — a gap whose answers change scope, observable
behavior, data ownership, permissions, or a protected/high-risk boundary. Propose likely edge cases,
impacts, and risks as labelled assumptions instead of asking, and reserve questions for gaps that
best-effort assumption cannot safely cover. Revalidate only changed entries. In a direct `/study`
run where clarification materially reinterpreted the ticket, show one checkpoint (goal, in/out of
scope, actors/outcomes, remaining deferrable assumptions), apply corrections, revalidate before
CONTINUE — skip it for a complete ticket and for autonomous runs.

### DEV-SPEC.md

Give each material claim one epistemic status (`fact`, `assumption`, `unknown`, or `conflict`) and
one or more sources (`ticket`, `clarification`, `design`, `api`, `code`). Use stable `FR-ID`,
`Story-ID`, and `SC-ID` values only for evidence-backed entries; map stories and success conditions
to FR IDs. Preserve `SC-ID` for solution-design to map explicitly to its derived `AC-ID`.

The spec is organised around six focus areas. Each area is applicable, or is written as one line
`N/A — <why>` (for example `API docs: N/A — no remote/backend surface`). The area→section map:

| Focus area | Where it lives | Applies when |
|---|---|---|
| 1 Requirements | §2 goal, §3 FRs, §4 stories, §5 success conditions | always |
| 2 Edge cases | §5 (explicit outcomes) + §5a proposed edge cases | always; propose best-effort when the ticket is thin |
| 3 Feature impact | §6 Engineering Evidence | when the feature touches existing code |
| 4 Risk | §9 Risk Analysis | always — `N/A` only for isolated greenfield |
| 5 API docs | §6 (impact) + §7 (contracts/constraints) | when a remote/backend/API surface or API docs exist |
| 6 Figma/design | §1 source + §5/§5a (visible states) | when a design source was supplied |

Sections:

0. Analysis Control (outcome, scope classification, depth, lookup ledger; one line per focus area
   marking it applicable or `N/A — <why>`)
1. Sources (design link/source or none; API docs; converted files; design index
   `Design-Ref | frame/screen | manifest/local path | revision | read status`)
2. Overview & Business Goal
3. Functional Requirements (`FR-ID | requirement | status | evidence/source`)
4. Actors & User Stories (`Story-ID | FR-ID | story`), or actor/capability mapping
5. Observable Success Conditions (`SC-ID | FR-ID | explicit/clarified outcome | Design-Ref when evidenced visually | evidence/source`)
   - 5a. Proposed edge cases & boundary behavior (`FR-ID | edge/boundary case | expected handling |
     status | source`) — best-effort, every row `[assumption]` or `[unknown]`; never asserted as a
     success condition and never given an `SC-ID` until the user confirms it
6. Engineering Evidence — Non-normative:
   - Module impact hypothesis (`module | owner/consumer | dependency evidence | likely contract | status/confidence`)
   - Verification implications (`module/consumer | candidate command or device/manual check | reason | status`)
   - Entry points (`symbol | role | file:line`), limited to the smallest confirmed set
   - Current behavior (`behavior | status | evidence/source`), summarized rather than traced line by line
   - Affected boundaries (`boundary | why it matters | status | evidence/source`), confirmed only
   - Reuse candidates (`candidate | location | apparent fit | confidence`), without a reuse decision
   - `N/A — greenfield; no existing-code impact` when no existing code is touched
7. Non-functional / Technical Constraints (including API contract constraints when focus area 5, API docs, applies)
8. Open Questions, Assumptions & Conflicts (`classification | owner | consequence`)
9. Risk Analysis (`risk | likelihood/impact | affected FR/area | status | source`)

Edge cases, feature-impact entries, and non-obvious risks may be proposed best-effort even when the
ticket does not state them, but each such row must carry an `[assumption]` or `[unknown]` label with
its reasoning source; a proposed edge case never becomes a normative success condition without user
confirmation. Ask the user (per the interactivity rule above) only when a gap is genuinely blocking.

Never resolve source disagreement by synthesis: preserve each statement independently, label it
`[conflict]`, state the consequence, and clarify when material. Code facts are current compatibility
constraints and do not redefine product intent. Reference evidence instead of repeating it.
Normalizing, splitting, or combining source statements is allowed when meaning is preserved — cite
every entry and label added interpretation as an assumption; normalization is not invention.

Do not add invented flows, business-rule tables, G/W/T criteria, tests, acceptance criteria, or
solution decisions; proposed edge cases and risks are labelled assumptions, not designs.
Hand off to solution-design only with validation PASS and no blocking gap. A guarded run additionally
requires `AUTOMATION: CONTINUE`; an unguarded guided run requires its normal human approval.
