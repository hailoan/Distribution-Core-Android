---
name: Ticket Reading
description: During feature-analysis or bug-investigation, normalize supplied ticket files and extract a source-tagged feature or bug record. Write only converted input copies; return facts, assumptions, and gaps.
---

## Ticket Reading

Resolve the ticket folder from `$AIDLC_OUTPUT_DIR`, otherwise `output/<ticket>/` (or `output/`
when no ID is needed). The loading stage creates the folder; this skill may write only converted
copies under `input/`.

### Normalize referenced files

For each non-Markdown source document, log, or structured export:

1. convert it to Markdown with an available document converter while preserving headings, tables,
   lists, code, and log lines;
2. sanitize the basename, avoid collisions, and save `input/<safe-original-name>.md` without overwriting the source;
3. read the converted copy and record the original-to-copy mapping.

Read existing Markdown directly. Use available ticket/Jira connectors for referenced URLs and
record their revision/time when exposed. Bound very large logs/exports while preserving the
relevant line numbers and record omitted ranges. If conversion loses material structure, report
the loss instead of silently summarizing it.

### Extract without analyzing

Tag every result with exactly one `kind`.

For a feature:

```yaml
kind: feature
source_refs: []
problem: ""
goal: ""
actors: []
raw_requirements: []
explicit_success_conditions: []
nfr_hints: []
ambiguities: []
contradictions: []
missing_fields: []
```

For a bug/crash:

```yaml
kind: bug
source_refs: []
observed: ""
expected: ""
reproduction_steps: []
affected_surface: ""
environment: {}
priority_signals: []
ambiguities: []
contradictions: []
missing_fields: []
```

Keep each declared type when a value is absent (for example, `reproduction_steps: []` and
`environment: {}`) and list its key in `missing_fields`. Preserve a source reference beside each
fact. Mark implications `[assumption]`; do not turn them into requirements, causes, or solutions.
Return the tagged record plus `converted_sources`. For every source record `source_type`,
`source_location`, `retrieved_at`, and `revision` when available.

When invoked by feature-analysis and `kind: bug`, return the record unchanged with
`route: bug-investigation`; do not reinterpret the bug as a feature. Preserve explicit acceptance
criteria or success conditions under `explicit_success_conditions`; never complete missing ones.
