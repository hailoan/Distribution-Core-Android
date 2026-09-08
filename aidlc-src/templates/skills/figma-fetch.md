---
name: Figma Fetch
description: During feature-analysis or solution-design, prepare a supplied remote Figma URL at most once per ticket. Reuse a valid ticket-local cache; otherwise emit raw data when available, a compact digest, selected screenshots, and a manifest.
---

## Figma Fetch

Run in feature-analysis, or in solution-design only when feature-analysis did not prepare a valid
cache, for a remote `figma.com` URL. Preserve the visual source; leave product interpretation to
the routing agent. Local TSX, image, JSON, and export sources are read directly by that agent and
must not load this skill.

### Cache gate — before any remote request

Check `output/<ticket>/figma/manifest.json` first. A cache is valid when the manifest is readable
and at least its referenced digest or screenshot evidence is readable. When valid, return
`cache_status: reused` with the manifest/source metadata and make **zero** connector calls. Do not
ask the user for the URL again, even for a later pipeline stage. A legacy manifest without source
metadata remains reusable when its referenced evidence is valid.

A directory by itself is not evidence: when `figma/` exists but the manifest or all referenced
evidence is missing/unreadable, return `cache_status: incomplete` before requesting anything. Use a
URL already recorded in DEV-SPEC/ticket evidence; ask the user once only when no recorded URL exists
and remote design evidence is required. Make at most one remote preparation attempt per invocation
and record its result in the manifest so downstream stages do not retry a failed/limited request
silently.

### Prepare the source

- Validate `figma.com`, extract the file key and `node-id` (`-` to `:` for the API), and reuse
  `output/<ticket>/figma/figma-data.json` only when its file key, node ID, and available revision
  match the requested source.
- For an unreadable/unsupported URL, preserve it and report `not read`.

Use only capabilities the configured connector exposes; raw JSON or screenshots may be
unavailable. For fetched Figma data:

1. Preserve the raw response as `figma-data.json`. Capture a **style-bearing** response, not a
   names-and-geometry-only one: the persisted data must carry, where the connector exposes them,
   typography (font family/weight/size/line-height), fills and other colors, corner radii, strokes,
   effects, opacity, and the icon/vector/image asset inventory for each scoped frame. A
   metadata-only response (layer names, bounds, and visible text with no style tokens) is
   insufficient — record `request_status: partial` in the manifest and note the missing style
   dimensions so the design stage does not treat absent tokens as "intentionally unspecified". When
   the connector separates metadata from style/variables, combine the calls so one persisted
   `figma-data.json` (or a sibling captured alongside it) carries both structure and tokens.
2. Before any model reads the raw JSON, run
   `.aidlc/lib/figma-digest.js figma-data.json figma-digest.json`. The digest whitelist already
   surfaces fills, text styles, corners, strokes, and effects, so a style-bearing raw response in
   step 1 yields a token-rich digest; a metadata-only response yields a digest with no design
   tokens. Load the digest only; inspect raw JSON afterward solely when the digest lacks a required
   field.
3. Select only requested or Level-1 frames from the digest. Build each filesystem-safe `safe_id`
   by replacing `:` and other unsafe characters with `-`; never use a raw node ID in a filename.
   When one URL resolves to multiple screens/states, keep them as separate `frames[]` entries with
   separate `design_ref` values; do not merge screens into one synthetic reference.
4. Render only frames explicitly named by the ticket/design scope or required to evidence an
   approved visible state. Use the digest to select first so unrelated frames do not consume
   screenshot requests. Render selected screenshots when the configured connector supports it, under
   `output/<ticket>/figma/screenshots/`.
5. Write `output/<ticket>/figma/manifest.json`:

```json
{
  "source": {
    "url": "https://www.figma.com/design/...?...node-id=12-34",
    "file_key": "...",
    "node_id": "12:34",
    "revision": null,
    "requested_at": "<timestamp>",
    "request_status": "success"
  },
  "frames": [{
    "node_id": "12:34",
    "safe_id": "12-34",
    "design_ref": "figma:12:34",
    "name": "Example",
    "raw_json": "figma/figma-data.json",
    "digest": "figma/figma-digest.json",
    "screenshot": "figma/screenshots/12-34.png",
    "screenshot_status": "available"
  }]
}
```

Paths are ticket-folder relative; use `null` for unavailable files. Ensure every generated
filename is reachable through this manifest. `design_ref` is the stable downstream reference and
must be `figma:<original-node-id>`; never substitute `safe_id` for it. Record the source revision and apply configured size
limits/redaction rules before persisting remote design data.

One remote file/frame-tree response may populate many manifest frame entries and still counts as
one preparation. Screenshot rendering may require separate connector operations, so record a
`screenshot_status` per frame and request screenshots only for scoped frames. Downstream stages
reuse prepared entries and never refetch the parent file to obtain an already-indexed frame.

Return the source type/path, `cache_status: reused | fetched | incomplete | failed`, read/request
status, manifest path, prepared frame names, whether the captured data is style-bearing or
metadata-only, and missing screenshots/style dimensions/details. Do not author a stage artifact and
do not retry a recorded failed/rate-limited request without explicit user instruction.
