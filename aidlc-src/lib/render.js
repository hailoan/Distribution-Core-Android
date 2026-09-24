#!/usr/bin/env node
/*
 * aidlc render helper — the ONE place that maps and validates the project profile
 * (aidlc.project.json) onto the {{PLACEHOLDERS}} used by the templates and by
 * setup-aidlc.sh. Two modes:
 *
 *   node render.js vars <profile>            → prints shell `KEY='value'` assignments
 *                                              for setup-aidlc.sh to `eval`.
 *   node render.js file <profile> <template> → prints the template with every
 *                                              {{KEY}} substituted, to stdout.
 *   node render.js modules <profile>         → prints the validated module registry.
 *
 * Keeping this tiny and dependency-free (plain JSON.parse, no yaml/json5) so the
 * generator has no npm install step.
 */
"use strict";
const fs = require("fs");
const path = require("path");

function fail(msg) {
  process.stderr.write("✗ aidlc render: " + msg + "\n");
  process.exit(1);
}

function loadProfile(file) {
  let raw;
  try {
    raw = fs.readFileSync(file, "utf8");
  } catch {
    fail("cannot read profile: " + file);
  }
  // Tolerate // and /* */ comments and trailing commas so the profile can be
  // documented inline without breaking a strict JSON.parse.
  const stripped = raw
    .replace(/\/\*[\s\S]*?\*\//g, "")
    .replace(/(^|[^:"'])\/\/.*$/gm, "$1")
    .replace(/,(\s*[}\]])/g, "$1");
  try {
    return JSON.parse(stripped);
  } catch (e) {
    fail("profile is not valid JSON (" + file + "): " + e.message);
  }
}

function req(v, where) {
  if (v === undefined || v === null || v === "") {
    fail("missing required profile field: " + where);
  }
  return v;
}

function safePathSegment(v, where) {
  req(v, where);
  if (typeof v !== "string" || !/^[A-Za-z0-9][A-Za-z0-9._-]{0,119}$/.test(v)) {
    fail(where + " must be a 1-120 character filename segment using only letters, numbers, '.', '_', or '-'");
  }
  const folded = v.toLowerCase();
  const stageCommands = new Set(["study", "design", "task", "implement", "ut", "it", "discover", "fixbug", "vibe"]);
  if (stageCommands.has(folded)) {
    fail(where + " conflicts with the built-in /" + folded + " command");
  }
  if (/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?$/.test(folded)) {
    fail(where + " is a reserved filename on Windows");
  }
  return v;
}

/** The full placeholder map. Multi-line values (ENV_MODULES) are fine for file mode. */
function buildVars(p) {
  const project = p.project || {};
  const stack = p.stack || {};
  const models = p.models || {};
  const commands = p.commands || {};
  const titles = p.frameworkTitles || {};
  const modules = Array.isArray(stack.modules) ? stack.modules : [];
  if (!modules.length || modules.some((m) => typeof m !== "string" || !m.trim())) {
    fail("stack.modules must be a non-empty array of module names");
  }
  return {
    // models (full ids used in workspace.yaml agent list)
    MODEL_HEAVY: req(models.heavy, "models.heavy"),
    MODEL_MID: req(models.mid, "models.mid"),
    // workspace identity
    WORKSPACE_NAME: req(project.workspaceName, "project.workspaceName"),
    PROJECT_NAME: req(project.name, "project.name"),
    PROJECT_SHORT: req(project.shortName, "project.shortName"),
    ANDROID_DEV_ARTIFACT: req(stack.androidDevArtifact, "stack.androidDevArtifact"),
    // package as a source path (com.example.app → com/example/app) for
    // {{ANDROID_DEV_ARTIFACT}}/{{PACKAGE_PATH}}/<layer>/ style examples.
    PACKAGE_PATH: String(req(project.package, "project.package")).split(".").join("/"),
    CMD_REVIEW: safePathSegment(commands.review, "commands.review"),
    // Ticket example used by slash-command input hints. Optional for older profiles.
    TICKET_EXAMPLE: (p.ticket && p.ticket.example) || "PROJ-123",
    // environment block
    ENV_PROJECT_TYPE: req(stack.projectType, "stack.projectType"),
    ENV_LANGUAGE: req(stack.language, "stack.language"),
    ENV_ARCHITECTURE: req(stack.architecture, "stack.architecture"),
    ENV_UI_FRAMEWORK: req(stack.uiFramework, "stack.uiFramework"),
    ENV_DI_FRAMEWORK: req(stack.diFramework, "stack.diFramework"),
    ENV_MIN_SDK: String(req(stack.minSdk, "stack.minSdk")),
    ENV_TARGET_SDK: String(req(stack.targetSdk, "stack.targetSdk")),
    ENV_MODULES: modules.map((m) => "    - " + m).join("\n"),
    ENV_MODULES_MD: modules.map((m) => "- `" + m + "`").join("\n"),
    // framework rules-file H1 titles + cursor frontmatter description
    TITLE_CLAUDE: req(titles.claude, "frameworkTitles.claude"),
    TITLE_CURSOR: req(titles.cursor, "frameworkTitles.cursor"),
    TITLE_CODEX: req(titles.codex, "frameworkTitles.codex"),
    TITLE_COPILOT: req(titles.copilot, "frameworkTitles.copilot"),
    CURSOR_DESC: req(p.cursorRuleDescription, "cursorRuleDescription"),
  };
}

function stringArray(value, where, options = {}) {
  const allowEmpty = options.allowEmpty === true;
  if (!Array.isArray(value) || (!allowEmpty && value.length === 0) || value.some((item) => typeof item !== "string" || !item.trim())) {
    fail(where + " must be " + (allowEmpty ? "an" : "a non-empty") + " array of non-empty strings");
  }
  return value.map((item) => item.trim());
}

function buildModuleRegistry(p) {
  const declared = Array.isArray(p.modules) ? p.modules : [];
  if (!declared.length) fail("modules must be a non-empty array of module records");
  const ids = new Set();
  const modules = declared.map((item, index) => {
    const where = "modules[" + index + "]";
    if (!item || typeof item !== "object" || Array.isArray(item)) fail(where + " must be an object");
    const id = req(item.id, where + ".id");
    if (typeof id !== "string" || !/^[a-z][a-z0-9_-]*$/.test(id)) fail(where + ".id must be a lowercase module id");
    if (ids.has(id)) fail("duplicate module id: " + id);
    ids.add(id);
    if (typeof item.gradlePath !== "string" || !/^:[A-Za-z0-9:_-]+$/.test(item.gradlePath)) {
      fail(where + ".gradlePath must be an absolute Gradle project path such as :app");
    }
    const publicContract = item.publicContract;
    if (typeof publicContract !== "boolean") fail(where + ".publicContract must be boolean");
    if (![true, false, "unknown"].includes(item.externalConsumers === undefined ? false : item.externalConsumers)) {
      fail(where + ".externalConsumers must be true, false, or \"unknown\"");
    }
    const roots = stringArray(item.roots, where + ".roots");
    if (roots.some((root) => root.startsWith("/") || root.split("/").includes(".."))) {
      fail(where + ".roots must contain relative project paths without '..'");
    }
    return {
      id,
      gradlePath: item.gradlePath,
      kind: req(item.kind, where + ".kind"),
      namespace: req(item.namespace, where + ".namespace"),
      role: req(item.role, where + ".role"),
      status: req(item.status, where + ".status"),
      roots,
      dependsOn: stringArray(item.dependsOn || [], where + ".dependsOn", { allowEmpty: true }),
      dependencySemantics: item.dependencySemantics || "project-dependency",
      publicContract,
      externalConsumers: item.externalConsumers === undefined ? false : item.externalConsumers,
      verification: stringArray(item.verification, where + ".verification"),
      riskTags: stringArray(item.riskTags || [], where + ".riskTags", { allowEmpty: true }),
    };
  });
  modules.forEach((module) => module.dependsOn.forEach((dependency) => {
    if (!ids.has(dependency)) fail("module " + module.id + " depends on unknown module " + dependency);
    if (dependency === module.id) fail("module " + module.id + " cannot depend on itself");
  }));

  const visiting = new Set();
  const visited = new Set();
  const byId = new Map(modules.map((module) => [module.id, module]));
  function visit(id, trail) {
    if (visiting.has(id)) fail("module dependency cycle: " + [...trail, id].join(" -> "));
    if (visited.has(id)) return;
    visiting.add(id);
    byId.get(id).dependsOn.forEach((dependency) => visit(dependency, [...trail, id]));
    visiting.delete(id);
    visited.add(id);
  }
  modules.forEach((module) => visit(module.id, []));

  const stackModules = new Set(stringArray((p.stack || {}).modules, "stack.modules"));
  const registryIds = new Set(modules.map((module) => module.id));
  const mismatch = [...new Set([...stackModules, ...registryIds])].filter((id) => stackModules.has(id) !== registryIds.has(id));
  if (mismatch.length) fail("stack.modules and modules registry differ: " + mismatch.sort().join(", "));

  const quality = p.quality || {};
  const repairCycles = quality.maxAutonomousRepairCycles === undefined ? 0 : quality.maxAutonomousRepairCycles;
  if (!Number.isInteger(repairCycles) || repairCycles < 0 || repairCycles > 5) {
    fail("quality.maxAutonomousRepairCycles must be an integer from 0 to 5");
  }
  return {
    version: "1.0",
    project: req((p.project || {}).name, "project.name"),
    source: "aidlc.project.json",
    quality: {
      requireModuleImpact: quality.requireModuleImpact !== false,
      requireIntegrationGateFor: stringArray(quality.requireIntegrationGateFor || [], "quality.requireIntegrationGateFor", { allowEmpty: true }),
      maxAutonomousRepairCycles: repairCycles,
    },
    modules,
  };
}

function renderFile(vars, template) {
  let s;
  try {
    s = fs.readFileSync(template, "utf8");
  } catch {
    fail("cannot read template: " + template);
  }
  const missing = new Set();
  const rendered = s.replace(/\{\{([A-Z_]+)\}\}/g, (m, key) => {
    if (Object.prototype.hasOwnProperty.call(vars, key)) {
      return vars[key];
    }
    missing.add(key);
    return m;
  });
  if (missing.size) {
    fail("unresolved placeholder(s) in " + path.basename(template) + ": " + [...missing].sort().join(", "));
  }
  return rendered;
}

/** Shell-safe single-quoted assignment: KEY='...' with embedded ' escaped. */
function shAssign(key, value) {
  const v = String(value).split("'").join("'\\''");
  return `${key}='${v}'`;
}

function main() {
  const [mode, profileFile, templateFile] = process.argv.slice(2);
  if (!mode || !profileFile) {
    fail("usage: render.js <vars|file|modules> <profile> [template]");
  }
  const profile = loadProfile(profileFile);
  if (mode === "modules") {
    process.stdout.write(JSON.stringify(buildModuleRegistry(profile), null, 2) + "\n");
    return;
  }
  const vars = buildVars(profile);
  if (mode === "vars") {
    // Only the scalars setup-aidlc.sh needs in shell (skip multi-line ENV_MODULES).
    const shellKeys = [
      "MODEL_HEAVY", "MODEL_MID",
      "PROJECT_NAME", "PROJECT_SHORT", "CMD_REVIEW",
      "TICKET_EXAMPLE",
      "TITLE_CLAUDE", "TITLE_CURSOR", "TITLE_CODEX", "TITLE_COPILOT", "CURSOR_DESC",
    ];
    process.stdout.write(shellKeys.map((k) => shAssign(k, vars[k])).join("\n") + "\n");
    return;
  }
  if (mode === "file") {
    if (!templateFile) {
      fail("file mode needs a template path");
    }
    process.stdout.write(renderFile(vars, templateFile));
    return;
  }
  fail("unknown mode: " + mode);
}

main();
