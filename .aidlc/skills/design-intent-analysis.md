---
name: Design Intent Analysis
description: During feature-analysis, inspect every supplied design reference for visible product intent, preferring a prepared ticket-local Figma cache and using the remote connector only through figma-fetch. Return sourced evidence and unknowns; write nothing.
---

## Design Intent Analysis

Run whenever the ticket contains any design reference: URL, local path, attachment, image,
design-as-code, structured data, or exported folder. Record the reference verbatim and its actual
type. If it cannot be read, return `read_status: not_read` rather than substituting an
interpretation.

For a remote Figma source, first read `<ticket-dir>/figma/manifest.json` and its referenced digest
and screenshots. When valid cached evidence exists, do not ask for the URL and do not call the
remote connector. When it does not exist, the routing agent must invoke `figma-fetch.md` once; this
skill then analyzes the prepared cache. Do not independently download data, generate screenshots,
or inspect unrequested frames. Record the source revision/version when the manifest exposes one.

Use the view-layer topic from the stage packet, then capture only visible evidence:

- screens and loading/empty/error/success states;
- visible user actions and text;
- visible accessibility evidence such as labels or contrast intent, without inferring absent behavior;
- requirements explicitly represented by a design element;
- ticket/design mismatches.

Label design-implied but unstated requirements `[assumption]`. Label absent transitions,
unsupported schema semantics, or unreadable details `[unknown]`. Do not extract implementation
measurements/tokens or complete missing flows.

Return:

- `design_reference: { value, type, read_status, revision }`;
- `observed_evidence`: `Design-Ref | frame/screen | screen/state/action/requirement | design element | label`;
- `mismatches`;
- `assumptions`;
- `unknowns`.

This skill covers all design-source types despite its compatibility name. Classify each result as
`observed`, `implied`, `absent`, or `unreadable`; absence alone is not a requirement.

For cached Figma, use the manifest frame's exact `design_ref` (`figma:<node-id>`). For a supplied
local design, use `local:<ticket-relative-path>#<explicit-screen-or-frame-name>`. Never invent a
frame reference from a filename or attach evidence from one frame to another.
