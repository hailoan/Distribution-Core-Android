---
name: Feature Clarification
description: Resolve only material feature-analysis ambiguities through a small, consequence-aware question set. Return sourced amendments; write nothing and make no technical decisions.
---

## Feature Clarification

Consume blocking gaps from DEV-SPEC validation and the shared evidence bundle. Do not ask a
question already answerable from supplied evidence, the stage packet, API contracts, or current
code. Ask only when different answers change scope, observable behavior, data ownership,
permissions, or a protected/high-risk boundary.

In a guided run, ask exactly one question at a time, starting with the unresolved question having
the largest effect on scope or observable behavior. Update the evidence bundle with each answer
before selecting the next question. Ask at most five questions by default; exceed that only when
the user explicitly requests a requirements workshop. In a non-interactive run,
return the same questions together so the stage can record CLARIFY. For every question return:

`question | affected FR/SC | why it blocks | consequence if unresolved | decision owner | answer/source`

Prefer a short multiple-choice question only when supplied evidence establishes the choices; do
not invent a menu of designs. Group tightly coupled fields when one answer naturally resolves
them. Record answers as `[fact:clarification]`, retain unanswered items as
`[unknown:blocking]`, and preserve
source conflicts rather than choosing a winner. Return `amendments`, `resolved`, and
`still_blocking`. Do not choose architecture, author tests, or create new requirements beyond the
user's answer.
