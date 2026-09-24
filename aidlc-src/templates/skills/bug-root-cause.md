---
name: Bug Root Cause
description: Diagnose bugs during bug-investigation by classifying impact and tracing logs, crashes, ANRs, and code evidence. Return confirmed or unconfirmed cause findings; write nothing and do not implement.
---

## Bug Root Cause

Use the architecture/high-risk packet topics plus failing-path and impact evidence supplied by the
routing agent. Report missing evidence rather than loading another skill.

### Classify impact

| Signals in the report | Technical severity |
| --- | --- |
| typo, text, label, icon, color, spacing, small animation, wrong asset | Low |
| UI state, lists/paging, API/business logic, navigation, notification, recoverable crash | Medium |
| production crash, ANR, data loss, leak, security/privacy, DB corruption, or sync corruption | Critical |

Record ticket priority separately from evidence-derived technical severity. Default unmatched
reports to Medium; never classify a crash trace as Low. The tier controls breadth and verification
depth, not a mandatory number of hypotheses, solutions, or tokens. For the current project, inspect lifecycle,
notification routing, socket ordering, cache reconciliation, SDK callbacks, process recreation,
and flavor configuration only when the failing path reaches them.

### Trace evidence

- Build a timeline from relevant log lines and map tags/classes/messages to source symbols.
  Separate causal evidence from correlation.
- For crashes/ANRs, resolve the exception, throwing frame, first app-owned frame, call chain, and
  thread. Record obfuscated or missing frames as unknown.
- Test only plausible causes until one explains the observed/expected difference and evidence.
  For critical impact, also scope affected users/data, containment, rollback, and monitoring.
- Include the reachable re-check surface supplied by the routing agent; flag it as a follow-up when
  absent.

Return:

- `tier` and its evidence;
- `status: confirmed | unconfirmed`;
- when confirmed, the root cause and `file:line` evidence chain;
- when unconfirmed, ranked remaining candidates, evidence for/against each, and the next
  discriminating check;
- fix constraints or options only when evidence supports them, plus regression/containment needs.

Never present an unverified cause or solution as confirmed.
