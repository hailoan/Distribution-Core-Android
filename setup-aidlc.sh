#!/bin/bash
# ============================================================
# AI-DLC generator (thin renderer)
# ============================================================
# Generates the full AI-DLC setup for a given AI coding framework by rendering
# the templates in aidlc-src/ against the project profile aidlc.project.json.
#
#   • profile  → aidlc.project.json   (the ONE file you edit per project:
#                identity, stack, ticket example, models)
#   • templates→ aidlc-src/templates/ (context-collection.md, agents/*.md,
#                skills/*.md — edit rules here as real files, not heredocs)
#   • output   → .aidlc/ (shared core) + per-framework rules file & commands
#
# The shared machinery is regenerated; project-owned `.aidlc/context.md` is seeded
# once and preserved unless AIDLC_REFRESH_CONTEXT=1 is set.
#
# Usage:
#   ./setup-aidlc.sh <framework>
#     claude | claude-code     → CLAUDE.md                       + .claude/commands/*.md
#     cursor                   → .cursor/rules/aidlc.mdc          + .cursor/commands/*.md
#     codex                    → AGENTS.md                        + .codex/prompts/*.md
#     github-copilot | copilot → .github/copilot-instructions.md + .github/prompts/*.prompt.md
#     all                      → every framework above
#
# To adapt to another project: edit aidlc.project.json, generate once, then fill the
# project facts in `.aidlc/context.md`.
# ============================================================

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
FRAMEWORK="${1:-}"
# Output destination — where generated files (.aidlc/, CLAUDE.md, .claude/…) land.
# Defaults to ROOT so running `./setup-aidlc.sh` from a project root behaves as
# before. When the extension runs this bundled script from its own assets/ dir,
# it sets AIDLC_DEST to the target project so the toolkit never has to be copied
# into the project — only the generated output is written there.
DEST="${AIDLC_DEST:-$ROOT}"
# Toolkit source (templates + runtime helpers). A legacy custom source cannot be mixed with the
# compact agents because its contracts differ; fall back to a complete bundled source when one is
# available, otherwise stop with a migration error.
source_compatible() {
  local base="$1" rel schema=""
  [ -f "$base/toolkit.schema" ] || return 1
  IFS= read -r schema < "$base/toolkit.schema" || return 1
  [ "$schema" = "4" ] || return 1
  local required=(
    lib/render.js lib/stage-context.js lib/figma-digest.js
    templates/context.md templates/context-collection.md
    templates/agents/android-dev.md templates/agents/bug-investigation.md
    templates/agents/discovery.md templates/agents/feature-analysis.md
    templates/agents/implementation-plan.md templates/agents/review.md
    templates/agents/solution-design.md templates/agents/testing.md
    templates/skills/api-analysis.md templates/skills/architecture-analysis.md
    templates/skills/bug-root-cause.md templates/skills/codebase-search.md
    templates/skills/compose-guideline.md templates/skills/dependency-analysis.md
    templates/skills/dev-spec-validation.md templates/skills/feature-clarification.md
    templates/skills/figma-fetch.md templates/skills/design-intent-analysis.md
    templates/skills/paging-guideline.md templates/skills/planning.md
    templates/skills/regression-analysis.md templates/skills/reuse-detection.md
    templates/skills/review-checklist.md templates/skills/risk-analysis.md
    templates/skills/room-guideline.md templates/skills/ticket-reading.md
  )
  for rel in "${required[@]}"; do
    [ -f "$base/$rel" ] || return 1
  done
}

BUNDLED_SRC=""
for candidate in "$ROOT/aidlc-src" "$ROOT/assets/aidlc-src"; do
  if source_compatible "$candidate"; then BUNDLED_SRC="$candidate"; break; fi
done

if [ -n "${AIDLC_SRC:-}" ]; then
  if source_compatible "$AIDLC_SRC"; then
    SRC="$AIDLC_SRC"
  else
    echo "✗ Configured AIDLC_SRC is legacy or incomplete (toolkit schema 4 required): $AIDLC_SRC"
    echo "  Generation stopped before writing output so custom workflow behavior is preserved."
    echo "  Migrate it, or stop passing it; IDE users can move the local toolkit aside after preserving custom edits."
    exit 1
  fi
elif [ -n "$BUNDLED_SRC" ]; then
  SRC="$BUNDLED_SRC"
else
  bad_src="$ROOT/aidlc-src"
  echo "✗ AI-DLC source is legacy or incomplete (toolkit schema 4 required): $bad_src"
  echo "  Migrate it to the compact flat-agent layout (including stage-context.js and"
  echo "  figma-digest.js), or run the generator from a current bundled toolkit."
  exit 1
fi
TPL="$SRC/templates"
PROFILE="${AIDLC_PROFILE:-$DEST/aidlc.project.json}"
RENDER="$SRC/lib/render.js"

usage() {
  cat <<USAGE
Usage: ./setup-aidlc.sh <framework>

  claude | claude-code     → CLAUDE.md + .claude/commands/*.md
  cursor                   → .cursor/rules/aidlc.mdc + .cursor/commands/*.md
  codex                    → AGENTS.md + .codex/prompts/*.md
  github-copilot | copilot → .github/copilot-instructions.md + .github/prompts/*.prompt.md
  all                      → generate for every framework

Regenerates shared machinery: .aidlc/context-collection.md, .aidlc/skills/*.md,
.aidlc/agents/*.md, .aidlc/lib/*.js, and .aidlc/pipelines.json.
Seeds .aidlc/context.md only when missing unless AIDLC_REFRESH_CONTEXT=1.

Project identity comes from:  $PROFILE
Templates (edit rules here):  $TPL
Output is written to:         $DEST
USAGE
}

case "$FRAMEWORK" in
  claude|claude-code|cursor|codex|github-copilot|copilot|all) ;;
  -h|--help|help|"") usage; exit 0 ;;
  *) echo "✗ Unknown framework: '$FRAMEWORK'"; echo; usage; exit 1 ;;
esac

# --- prerequisites ---------------------------------------------------------
command -v node >/dev/null 2>&1 || {
  echo "✗ 'node' is required (to read $PROFILE). Install Node.js and retry."; exit 1; }

# scaffold_profile — write a generic, project-neutral profile so generation
# never blocks on a missing profile. Prefers the bundled default next to this
# script (assets/aidlc.project.json); otherwise writes an inline default.
scaffold_profile() {
  # Prefer a bundled default next to this script — either co-located
  # (assets/aidlc.project.json when run from the extension) or under assets/.
  local bundled
  for bundled in "$ROOT/aidlc.project.json" "$ROOT/assets/aidlc.project.json"; do
    if [ -f "$bundled" ] && [ "$bundled" != "$PROFILE" ]; then
      cp "$bundled" "$PROFILE"
      return
    fi
  done
  cat > "$PROFILE" <<'PROFILE_JSON'
{
  "project": {
    "name": "My Project",
    "shortName": "MyProject",
    "package": "com.example.myproject",
    "workspaceName": "MyProjectFeatureFactory",
    "featureModule": "app"
  },
  "stack": {
    "projectType": "android",
    "language": "kotlin",
    "architecture": "clean-architecture",
    "uiFramework": "compose",
    "diFramework": "hilt",
    "minSdk": "24",
    "targetSdk": "35",
    "modules": ["app"],
    "androidDevArtifact": "app/src/main/java"
  },
  "ticket": { "example": "PROJ-123" },
  "commands": { "review": "aidlc-review" },
  "models": {
    "heavy": "claude-opus-4-8",
    "mid": "claude-sonnet-5"
  },
  "frameworkTitles": {
    "claude": "CLAUDE.md — My Project",
    "cursor": "My Project — project rules",
    "codex": "AGENTS.md — My Project",
    "copilot": "GitHub Copilot — My Project"
  },
  "cursorRuleDescription": "My Project — project rules and AI-DLC pipeline (always on)"
}
PROFILE_JSON
}

if [ ! -f "$PROFILE" ]; then
  echo "ℹ No project profile found — creating a default one:"
  echo "    $PROFILE"
  echo "  These settings are project-specific. Edit this file later (identity, stack,"
  echo "  ticket example, models) to match your project, then re-run this script."
  echo ""
  scaffold_profile
fi

[ -d "$TPL" ]     || { echo "✗ Templates not found: $TPL (restore aidlc-src/)"; exit 1; }
[ -f "$RENDER" ]  || { echo "✗ Render helper not found: $RENDER (restore aidlc-src/)"; exit 1; }

# Validate and load every profile scalar before writing generated core or adapter output.
# Capture separately so render failure is not swallowed by eval's successful empty input.
# render.js rejects unsafe/colliding command basenames (including separators and traversal).
profile_vars="$(node "$RENDER" vars "$PROFILE")"
eval "$profile_vars"
unset profile_vars

mkdir -p "$DEST"
cd "$DEST"

# render <template> — print the template with {{PLACEHOLDERS}} substituted.
render() { node "$RENDER" file "$PROFILE" "$1"; }

# write_atomic <path> <command> [args...] — replace a generated file only after
# its producer succeeds, so a render error never truncates a working artifact.
write_atomic() {
  local out="$1" dir tmp
  shift
  dir="$(dirname "$out")"
  mkdir -p "$dir"
  tmp="$(mktemp "$dir/.aidlc-tmp.XXXXXX")"
  if "$@" > "$tmp"; then
    mv "$tmp" "$out"
    echo "  • $out"
  else
    rm -f "$tmp"
    return 1
  fi
}

# Remove retired Markdown from toolkit-owned generated trees. This prevents old skills/agents
# from surviving an upgrade and being rediscovered by a model after their templates are gone.
prune_generated_markdown() { # <template-dir> <generated-dir>
  local source_dir="$1" generated_dir="$2" file rel removed=0
  [ -d "$generated_dir" ] || return 0
  while IFS= read -r -d '' file; do
    rel="${file#"$generated_dir"/}"
    if [ ! -f "$source_dir/$rel" ]; then
      rm -f -- "$file"
      removed=$((removed+1))
    fi
  done < <(find "$generated_dir" -type f -name '*.md' -print0)
  [ "$removed" -eq 0 ] || echo "  • pruned $removed retired file(s) from $generated_dir"
}

# Reconcile only adapter files recorded as generator-owned. User-created files in the same
# framework directory are preserved because they never appear in this manifest. Keep ownership
# outside `.aidlc/state/` so clearing transient ticket state does not erase the ledger.
reconcile_owned_files() { # <adapter-dir> <manifest-key> <current-basename-list>
  local dir="$1" key="$2" current="$3" owned old candidate
  owned=".aidlc/generated-adapters/$key.txt"
  if [ -f "$owned" ]; then
    while IFS= read -r old; do
      case "$old" in ""|*/*|*\\*) continue;; esac
      if ! grep -Fqx -- "$old" "$current"; then
        candidate="$dir/$old"
        [ ! -f "$candidate" ] || rm -f -- "$candidate"
      fi
    done < "$owned"
  fi
  mkdir -p "$(dirname "$owned")"
  sort -u "$current" > "$owned.tmp"
  mv "$owned.tmp" "$owned"
}

# Before ownership manifests existed, adapters could leave retired files behind. Migrate each
# adapter once. Historical commands with the old generator signature are quarantined outside
# adapter discovery; retired agents require their exact generated provenance before removal.
migrate_legacy_commands() { # <adapter-dir> <manifest-key> <suffix> <current-list>
  local dir="$1" key="$2" suffix="$3" current="$4" sentinel cmd candidate quarantine base n
  sentinel=".aidlc/generated-adapters/$key.migrated-v3"
  [ ! -f "$sentinel" ] || return 0
  for cmd in collect dev doc figma gen-spec it plan release; do
    grep -Fqx -- "$cmd$suffix" "$current" && continue
    candidate="$dir/$cmd$suffix"
    if [ -f "$candidate" ] &&
        grep -Eq "^# /$cmd — .+ \\(AI-DLC stage\\)$" "$candidate" &&
        grep -Fq 'AI-DLC pipeline.' "$candidate" &&
        grep -Eq '\.aidlc/(skills|agents)/' "$candidate"; then
      # Signatures predate an explicit ownership marker. Move matches out of adapter
      # discovery instead of deleting them, so even a coincidental user file is recoverable.
      base=".aidlc/generated-adapters/legacy/$key/$cmd$suffix"
      quarantine="$base"
      n=1
      while [ -e "$quarantine" ] || [ -L "$quarantine" ]; do
        quarantine="$base.$n"
        n=$((n+1))
      done
      mkdir -p "$(dirname "$quarantine")"
      mv -- "$candidate" "$quarantine"
      echo "  • quarantined legacy adapter file: $candidate → $quarantine"
    fi
  done
  mkdir -p "$(dirname "$sentinel")"
  printf '3\n' > "$sentinel"
}

echo "🚀 Generating $PROJECT_NAME AI-DLC shared core (.aidlc/) …"

# --- shared core: rendered straight from templates -------------------------
write_atomic .aidlc/context-collection.md render "$TPL/context-collection.md"
if [ -f .aidlc/context.md ] && [ "${AIDLC_REFRESH_CONTEXT:-0}" != "1" ]; then
  echo "  • .aidlc/context.md (preserved; set AIDLC_REFRESH_CONTEXT=1 to regenerate)"
else
  write_atomic .aidlc/context.md render "$TPL/context.md"
fi
for f in "$TPL"/skills/*.md; do
  write_atomic ".aidlc/skills/$(basename "$f")" render "$f"
done
prune_generated_markdown "$TPL/skills" ".aidlc/skills"

# agent definitions — rendered preserving their
# tree under .aidlc/agents/. Optional: skipped if the templates carry no agents/ dir.
if [ -d "$TPL/agents" ]; then
  while IFS= read -r f; do
    write_atomic ".aidlc/${f#"$TPL"/}" render "$f"
  done < <(find "$TPL/agents" -type f -name '*.md' | sort)
fi
prune_generated_markdown "$TPL/agents" ".aidlc/agents"

# Runtime helpers keep deterministic context selection and large-design preprocessing
# out of agent prompts. They are copied, not loaded, until a stage invokes them.
for helper in stage-context.js figma-digest.js; do
  [ -f "$SRC/lib/$helper" ] || { echo "✗ Runtime helper not found: $SRC/lib/$helper"; exit 1; }
  write_atomic ".aidlc/lib/$helper" cat "$SRC/lib/$helper"
  chmod +x ".aidlc/lib/$helper"
done

# =========================================================
# Machine-readable pipeline manifest for the orchestrator
# (single source: derived from the STAGES map + the flows below).
# Zero-YAML — the extension reads this JSON directly.
# =========================================================

# agent id → generated slash command
agent_cmd() { case "$1" in
  feature-analysis) echo "/study";;
  bug-investigation) echo "/fixbug";;
  solution-design) echo "/design";;
  implementation-plan) echo "/task";;
  android-dev) echo "/implement";;
  testing) echo "/ut";;
  qa-plan) echo "/qa-plan";;
  automation-test) echo "/autotest";;
  review) echo "/$CMD_REVIEW";;
  discovery) echo "/discover";;
esac; }

# agent id → produced artifact, relative to the ticket's output dir
agent_artifact() { case "$1" in
  feature-analysis) echo "DEV-SPEC.md";;
  bug-investigation) echo "BUG-INVESTIGATION.md";;
  solution-design) echo "SOLUTION-DESIGN.md";;
  implementation-plan) echo "IMPLEMENT-PLAN.md";;
  android-dev) echo "CHANGESET.md";;
  testing) echo "UNIT-TEST-REPORT.md";;
  qa-plan) echo "TEST-CASES.md";;
  automation-test) echo "AUTOMATION-TEST-REPORT.md";;
  review) echo "CODE-REVIEW.md";;
  discovery) echo "FLOW-DISCOVERY.md";;
esac; }

# agent id → prior-stage artifact(s) this stage CONSUMES, per the §11 load contract — the
# machine-checkable prerequisites the orchestrator can verify exist before it runs the stage.
# (The skill's full "Reads" list may add project files — the diff, the codebase — that aren't
# pipeline artifacts and so aren't listed here.) Space-separated basenames, resolved to the ticket
# folder by the orchestrator. android-dev is intentionally absent: it consumes whichever plan /
# investigation preceded it, so the generator resolves its reads from the actual predecessor step
# (feature flow → IMPLEMENT-PLAN.md + SOLUTION-DESIGN.md, bug flow → BUG-INVESTIGATION.md).
agent_reads() { case "$1" in
  solution-design) echo "DEV-SPEC.md";;
  implementation-plan) echo "SOLUTION-DESIGN.md";;
  testing)         echo "CHANGESET.md";;
  qa-plan)         echo "DEV-SPEC.md";;
  automation-test) echo "CHANGESET.md";;
  *)                echo "";;   # entry stages — external input only (no prior artifact)
esac; }

# Flow-specific inputs belong here so the manifest stays executable without
# duplicating contracts across JSON, YAML, and skill prose.
step_reads() { # <flow> <agent> <previous-artifact>
  local flow="$1" agent="$2" previous="$3"
  case "$flow:$agent" in
    impl-flow:android-dev|auto-feature-flow:android-dev) echo "IMPLEMENT-PLAN.md SOLUTION-DESIGN.md";;
    impl-flow:review) echo "CHANGESET.md UNIT-TEST-REPORT.md AUTOMATION-TEST-REPORT.md IMPLEMENT-PLAN.md SOLUTION-DESIGN.md";;
    fixbug-flow:review) echo "CHANGESET.md UNIT-TEST-REPORT.md BUG-INVESTIGATION.md";;
    qa-flow:automation-test) echo "TEST-CASES.md";;   # QA flow: automate cases, not a code changeset
    qa-flow:review) echo "AUTOMATION-TEST-REPORT.md TEST-CASES.md DEV-SPEC.md";;
    techlead-review-flow:review) echo "";;
    *:android-dev) echo "$previous";;
    *) agent_reads "$agent";;
  esac
}

# space-separated basenames → a JSON string array, e.g. `feature UNIT-TEST-REPORT.md` → ["feature", "UNIT-TEST-REPORT.md"]
reads_json() {
  local out="[" first=1 r
  for r in $1; do
    [ "$first" = "1" ] || out="$out, "
    out="$out\"$r\""; first=0
  done
  printf '%s]' "$out"
}

# agent id → default model alias (heavier reasoning for analysis/design/review, mid for execution).
# The orchestrator passes this to the platform's model flag; overridable per step/task.
agent_model() { case "$1" in
  feature-analysis|bug-investigation|solution-design|review|discovery|qa-plan) echo "opus";;
  implementation-plan|android-dev|testing|automation-test) echo "sonnet";;
  *) echo "";;
esac; }

# agent id → execution mode. Every stage gets an isolated context; human_review remains the
# independent artifact approval gate. Fan-out stages may dispatch further only for disjoint work.
agent_exec() { echo "subagent"; }

# agent id → fan-out flag: a stage that dispatches one subagent per disjoint slice.
# Feature analysis, bug investigation, design, planning, and review use one isolated context with
# no fan-out.
# Android development and testing may fan out only after disjoint file ownership is assigned;
# shared integration surfaces remain serialized. See context-collection.md §11.
agent_fanout() { case "$1" in
  android-dev|testing|automation-test) echo "true";;
  *) echo "false";;
esac; }

# flow id → ordered "agent:humanReview(0|1)" steps.
flow_steps() { case "$1" in
  impl-flow)            echo "feature-analysis:0 solution-design:0 implementation-plan:0 android-dev:0 testing:0 automation-test:0 review:0";;
  auto-feature-flow)    echo "feature-analysis:0 solution-design:0 implementation-plan:0 android-dev:0";;
  discover-flow)        echo "discovery:0";;
  fixbug-flow)          echo "bug-investigation:0 android-dev:0 testing:0 review:0";;
  fixcrash-flow)        echo "bug-investigation:0 android-dev:0";;
  auto-bug-flow)        echo "bug-investigation:0 android-dev:0";;
  automation-test-flow) echo "automation-test:0";;
  qa-flow)              echo "feature-analysis:0 qa-plan:0 automation-test:0 review:0";;
  techlead-review-flow) echo "review:0";;
esac; }

gen_pipelines_json() {
  local flows="impl-flow auto-feature-flow discover-flow fixbug-flow fixcrash-flow auto-bug-flow automation-test-flow qa-flow techlead-review-flow"
  local nf; nf=$(echo $flows | wc -w | tr -d ' ')
  printf '{\n  "version": "2.0",\n  "flows": {\n'
  local fi=0
  for flow in $flows; do
    fi=$((fi+1))
    printf '    "%s": [\n' "$flow"
    local steps ns si=0 prev_artifact=""; steps="$(flow_steps "$flow")"; ns=$(echo $steps | wc -w | tr -d ' ')
    for st in $steps; do
      si=$((si+1))
      local agent="${st%%:*}" hr="${st##*:}" hrb="false"
      [ "$hr" = "1" ] && hrb="true"
      local art reads; art="$(agent_artifact "$agent")"
      reads="$(step_reads "$flow" "$agent" "$prev_artifact")"
      printf '      { "agent": "%s", "command": "%s", "skill": "%s", "reads": %s, "artifact": "%s", "model": "%s", "execution": "%s", "fanout": %s, "human_review": %s }' \
        "$agent" "$(agent_cmd "$agent")" "$agent" "$(reads_json "$reads")" "$art" "$(agent_model "$agent")" "$(agent_exec "$agent")" "$(agent_fanout "$agent")" "$hrb"
      [ "$si" -lt "$ns" ] && printf ','
      printf '\n'
      prev_artifact="$art"
    done
    printf '    ]'
    [ "$fi" -lt "$nf" ] && printf ','
    printf '\n'
  done
  printf '  }\n}\n'
}

write_atomic .aidlc/pipelines.json gen_pipelines_json

# ============================================================
# Per-framework: context rules file + agent slash commands
# ============================================================

# write_rules <h1-title> <outfile> [frontmatter]
# The always-loaded rules file stays SLIM: the tool-appropriate H1, the always-on
# essentials (generic + project ground rules and project identity), then a pointer to the two
# context files. The rich
# per-stage context is NOT duplicated here — stage-context.js selects the applicable
# topics from the two authoritative context files, so the always-on context stays small.
write_rules() {
  local h1="$1" out="$2" fm="${3:-}"
  mkdir -p "$(dirname "$out")"
  {
    [ -n "$fm" ] && printf -- '%s\n\n' "$fm"
    printf -- '# %s\n' "$h1"
    # generic ground rules — the "Ground rules" section of context-collection.md
    # (numbering-agnostic: match the heading by name, print until the next "## ").
    awk '/^## / && g { exit } /^## .*Ground rules/ { g=1 } g { print }' .aidlc/context-collection.md
    # project-specific invariants are always-on too; stage packets also include this section.
    awk '/^## / && p { exit } /^## .*[Gg]round rules/ { p=1 } p && !/TODO:/ { print }' .aidlc/context.md
    # project identity — the "What the app is" section of context.md (same approach, so a
    # project can renumber or reorganize context.md without breaking this extraction).
    awk '/^## / && w { exit } /^## .*[Ww]hat the app is/ { w=1 } w && !/^TODO:/ { print }' .aidlc/context.md
    cat <<'PTR'
---

## Project context → `.aidlc/context.md` · AI-DLC machinery → `.aidlc/context-collection.md`

Everything specific to this codebase lives in **`.aidlc/context.md`**: modules,
architecture, data, UI, DI, storage, naming, testing, and high-risk areas. Stages
load only their relevant topics through `.aidlc/lib/stage-context.js`; section
numbers may differ between projects.

The generic AI-DLC machinery lives in **`.aidlc/context-collection.md`**: ground rules
(§0), planning conventions (§10), the feature→review workflow + per-stage artifact &
load contract (§11), and the testing process (§12).

Before a non-trivial change, generate the compact stage packet so the relevant
project conventions are present without loading both context files in full.
PTR
  } > "$out"
  echo "  • $out"
}

# Stage table: command | agent id | title | artifact
STAGES=(
  "study|feature-analysis|Feature Analysis|DEV-SPEC.md"
  "design|solution-design|Solution Design|SOLUTION-DESIGN.md"
  "task|implementation-plan|Implementation Plan|IMPLEMENT-PLAN.md"
  "implement|android-dev|Android Dev|CHANGESET.md + code in $FEATURE_MODULE"
  "ut|testing|Testing|UNIT-TEST-REPORT.md"
  "qa-plan|qa-plan|QA Test Plan|TEST-CASES.md"
  "autotest|automation-test|Automation Test|AUTOMATION-TEST-REPORT.md"
  "$CMD_REVIEW|review|Review|CODE-REVIEW.md"
  "discover|discovery|Flow Discovery|FLOW-DISCOVERY.md"
  "fixbug|bug-investigation|Bug Investigation|BUG-INVESTIGATION.md"
)

# Apply-line differs per tool: Claude Code & Codex substitute $ARGUMENTS; Cursor reads the
# chat; Copilot prompt files use ${input:...}. (Single-quoted so the placeholders stay literal.)
ARG_SUB='Apply it to: $ARGUMENTS'
ARG_CHAT='Apply it to the ticket / request described in this chat.'
ARG_COPILOT='Apply it to: ${input:request}'

# How the stage asks for a missing input. The extension's New-ticket form guarantees the
# required fields; a bare slash command typed in a terminal does not — so every command
# carries an input contract and asks for whatever the invocation left out. Claude Code has
# a first-class question tool; the other tools just ask in the chat.
ASK_CLAUDE='Ask in plain text for free-form values (ticket id, paths, links); for a fixed-choice input use the **AskUserQuestion** tool.'
ASK_CHAT='Ask in the chat — one message per missing input — and wait.'

# cmd_inputs <command> <skill> — the stage's input contract as Markdown table rows.
# Mirrors the separately maintained extension's New-ticket form: same labels and required/optional
# split, so a stage behaves identically whether it is launched from the form or typed as a bare
# slash command. Keyed by command because feature analysis and bug
# investigation take different inputs. Everything else is a pipeline
# stage whose only external input is the ticket — its prerequisites come from agent_reads().
cmd_inputs() {
  local cmd="$1" skill="$2" reads
  case "$cmd" in
    study) cat <<ROWS
| Ticket id | **yes** | e.g. \`$TICKET_EXAMPLE\` — resolves the ticket folder \`output/<ticket>/\` |
| BA spec / API doc | **yes** | link or file path; a non-\`.md\` file is converted into \`output/<ticket>/input/\` first |
ROWS
      ;;
    fixbug) cat <<ROWS
| Ticket id | **yes** | e.g. \`FIXBUG-1\` — resolves the ticket folder \`output/<ticket>/\` |
| Step / flow where it errors | **yes** | reproduction steps / where the bug shows up |
| Expected result | **yes** | what should happen instead |
ROWS
      ;;
    discover) cat <<ROWS
| Ticket id | **yes** | e.g. \`DISCOVER-101\` — resolves the ticket folder \`output/<ticket>/\` |
| Short description | **yes** | which flow / behavior to reverse-engineer |
| Scope (code) | **yes** | modules / packages / files to scan |
ROWS
      ;;
    "$CMD_REVIEW") cat <<ROWS
| Ticket id *(or the branch pair)* | **yes** | pipeline review: resolves \`output/<ticket>/\`, which must already hold \`CHANGESET.md\` + \`UNIT-TEST-REPORT.md\`. Standalone branch review: give the branches instead |
ROWS
      ;;
    *)
      # android-dev prerequisites differ between feature and bug flows.
      if [ "$skill" = "android-dev" ]; then
        reads="IMPLEMENT-PLAN.md + SOLUTION-DESIGN.md (feature) or BUG-INVESTIGATION.md (bug)"
      elif [ "$skill" = "automation-test" ]; then
        # automation-test consumes a code changeset in impl/standalone flows, but the written
        # test cases in the QA flow.
        reads="CHANGESET.md (impl / standalone) or TEST-CASES.md (qa-flow)"
      else
        reads="$(agent_reads "$skill")"
        [ -n "$reads" ] || reads="—"
      fi
      cat <<ROWS
| Ticket id | **yes** | e.g. \`$TICKET_EXAMPLE\` — resolves the ticket folder \`output/<ticket>/\`, which must already hold: $reads |
ROWS
      ;;
  esac
}

# write_inputs <command> <skill> <ask-line> — the "ask for anything missing" gate.
write_inputs() {
  cat <<GATE

## Inputs

Ask one concise question for each missing required value, in table order, then wait. $3
Never invent identifiers, paths, or branches; do not ask for optional values.

| Input | Required | Notes |
| --- | --- | --- |
$(cmd_inputs "$1" "$2")

If a required prior artifact is missing, name its producing stage and stop.
GATE
}

CURSOR_FM="---
description: $CURSOR_DESC
globs:
alwaysApply: true
---"

# write_commands <dir> <argline> <askline> [suffix=.md] [emit_frontmatter=0]
write_commands() {
  local dir="$1" argline="$2" askline="$3" suffix="${4:-.md}" fm="${5:-0}" s cmd skill title artifact current key
  current="$(mktemp)"
  key="${dir//\//_}"; key="${key//./_}"
  mkdir -p "$dir"
  for s in "${STAGES[@]}"; do
    IFS='|' read -r cmd skill title artifact <<< "$s"
    {
      [ "$fm" = "1" ] && printf -- '---\nmode: agent\ndescription: %s — AI-DLC stage\n---\n\n' "$title"
      cat <<BODY
# /$cmd — $title (AI-DLC stage)

Act as the **$title** agent in the $PROJECT_SHORT AI-DLC pipeline.

If the native \`$skill\` agent definition is already active, use that loaded contract and do not
read it again. Otherwise read \`.aidlc/agents/$skill.md\` exactly once and follow it. The agent
generates its compact context packet and preflight; do not run the loader twice or load either
context file wholesale. Load only atomic skills the agent conditionally routes for this task.

- **Agent:** \`.aidlc/agents/$skill.md\`
- **Writes:** \`<resolved-ticket-folder>/$artifact\`
BODY
      write_inputs "$cmd" "$skill" "$askline"
      printf '\n%s\n' "$argline"
    } > "$dir/$cmd$suffix"
    printf '%s\n' "$cmd$suffix" >> "$current"
  done
  # /vibe — autonomous full flow
  {
    [ "$fm" = "1" ] && printf -- '---\nmode: agent\ndescription: Autonomous AI-DLC vibe flow\n---\n\n'
    cat <<BODY
# /vibe — autonomous AI-DLC (vibe flow)

Run the guarded vibe flow end-to-end, no human gates: feature-analysis → solution-design →
implementation-plan → android-dev → testing → automation-test → review. Resolve the **ticket output folder**
once (\$AIDLC_OUTPUT_DIR if set, else output/<ticket>/) and use it for every stage. For each
stage, use a fresh native subagent/context when the runtime supports it and return only its marker,
artifact path, and short summary before continuing. A native subagent already has its agent
definition; do not reload it. Without native isolation, read \`.aidlc/agents/<stage>.md\` once.
Add \`--flow impl-flow\` to that agent's context-loader command. Use only the resulting packet,
named prior artifacts, and applicable atomic skills. Require each stage artifact's first
line to be \`AUTOMATION: CONTINUE\` or
\`AUTOMATION: STOP — <reason>\`, and stop immediately on STOP. In android-dev, implement each parallel
wave from the plan's Dependency Map concurrently only when file ownership is disjoint; keep DI,
navigation, database schema/migrations, shared state, and build configuration serialized. Stop
and report at <ticket folder>/CODE-REVIEW.md.
BODY
    write_inputs "study" "feature-analysis" "$askline"
    printf '\n%s\n' "$argline"
  } > "$dir/vibe$suffix"
  printf '%s\n' "vibe$suffix" >> "$current"
  # /qa — autonomous QA / automation-tester flow
  {
    [ "$fm" = "1" ] && printf -- '---\nmode: agent\ndescription: Autonomous AI-DLC QA flow\n---\n\n'
    cat <<BODY
# /qa — autonomous AI-DLC (qa flow)

Run the guarded QA / automation-tester flow end-to-end, no human gates: feature-analysis → qa-plan →
automation-test → review. This flow tests behavior from a requirement; it writes no production code.
Resolve the **ticket output folder** once (\$AIDLC_OUTPUT_DIR if set, else output/<ticket>/) and use
it for every stage. For each stage, use a fresh native subagent/context when the runtime supports it
and return only its marker, artifact path, and short summary before continuing. A native subagent
already has its agent definition; do not reload it. Without native isolation, read
\`.aidlc/agents/<stage>.md\` once. Add \`--flow qa-flow\` to that agent's context-loader command. Use
only the resulting packet, named prior artifacts, and applicable atomic skills. feature-analysis
produces \`DEV-SPEC.md\` (acceptance criteria), qa-plan turns each AC into reviewable test cases in
\`TEST-CASES.md\`, automation-test automates the automatable cases (TC-ID → Test-ID) under the
androidTest source set and runs them, and review signs off coverage against the acceptance criteria.
Require each stage artifact's first line to be \`AUTOMATION: CONTINUE\` or
\`AUTOMATION: STOP — <reason>\`, and stop immediately on STOP. In automation-test, fan out one
subagent per disjoint test file only; never touch production code. Stop and report at
<ticket folder>/CODE-REVIEW.md.
BODY
    write_inputs "study" "feature-analysis" "$askline"
    printf '\n%s\n' "$argline"
  } > "$dir/qa$suffix"
  printf '%s\n' "qa$suffix" >> "$current"
  migrate_legacy_commands "$dir" "$key-commands" "$suffix" "$current"
  reconcile_owned_files "$dir" "$key-commands" "$current"
  rm -f -- "$current"
  echo "  • $dir/*$suffix ($(( ${#STAGES[@]} + 2 )) commands)"
}

gen_claude() {
  write_rules "$TITLE_CLAUDE" "CLAUDE.md"
  write_commands ".claude/commands" "$ARG_SUB" "$ASK_CLAUDE"
}
gen_cursor() {
  write_rules "$TITLE_CURSOR" ".cursor/rules/aidlc.mdc" "$CURSOR_FM"
  write_commands ".cursor/commands" "$ARG_CHAT" "$ASK_CHAT"
}
gen_codex() {
  write_rules "$TITLE_CODEX" "AGENTS.md"
  write_commands ".codex/prompts" "$ARG_SUB" "$ASK_CHAT"
}
gen_copilot() {
  write_rules "$TITLE_COPILOT" ".github/copilot-instructions.md"
  write_commands ".github/prompts" "$ARG_COPILOT" "$ASK_CHAT" ".prompt.md" 1
}

echo ""
echo "🧩 Generating framework adapter: $FRAMEWORK"
case "$FRAMEWORK" in
  claude|claude-code)      gen_claude ;;
  cursor)                  gen_cursor ;;
  codex)                   gen_codex ;;
  github-copilot|copilot)  gen_copilot ;;
  all)                     gen_claude; gen_cursor; gen_codex; gen_copilot ;;
esac

echo ""
echo "✅ Done — context + skills + workflow generated for: $FRAMEWORK"
