---
name: DEV-SPEC Validation
description: Perform a lightweight, failure-only quality check on a feature-analysis evidence bundle before DEV-SPEC handoff. Return validation status and defects; write nothing.
---

## DEV-SPEC Validation

Validate the structured evidence bundle before final rendering. Do not repeat the specification,
search for new evidence, or repair failures. Check:

- every FR has authoritative evidence and no requirement is derived only from current code;
- every Story-ID and SC-ID maps to an FR-ID;
- success conditions are explicit ticket/design statements or `[fact:clarification]`, never invented;
- every material statement has exactly one epistemic status and one or more valid source references;
- conflicts preserve each source independently and material conflicts are blocking;
- assumptions and unknowns have `blocking`, `deferrable`, or `non-material` classification;
- no blocking gap remains for CONTINUE;
- engineering evidence is marked non-normative and contains no solution decision;
- risks cite evidence and an affected requirement or area;
- the selected depth and lookup cap were honored, or the single permitted bounded escalation
  records a blocking/protected-boundary reason and produced material evidence;
- no authored placeholders such as `TBD`, `etc.`, `as appropriate`, or undefined references remain;
  quoted source wording may retain them when it is explicitly marked incomplete;
- generated requirements do not contradict one another or exceed the declared scope;
- ambiguity is explicitly labelled rather than hidden behind confident wording.

Return only:

```yaml
validation:
  status: PASS | NEEDS_CLARIFICATION | INVALID
  failures: []
  warnings: []
  coverage:
    requirements_with_evidence: "0/0"
    stories_mapped: "0/0"
    success_conditions_mapped: "0/0"
    material_unknowns_resolved: "0/0"
```

Use `NEEDS_CLARIFICATION` only for user-answerable blocking gaps. Use `INVALID` for wrong-kind or
inaccessible input, incompatible source structure, guard violations, or insufficient evidence for
a supported conclusion. A budget overrun alone is a process warning. PASS is required for CONTINUE,
and every `SC-ID` must remain available for explicit `SC-ID -> AC-ID` mapping downstream.
