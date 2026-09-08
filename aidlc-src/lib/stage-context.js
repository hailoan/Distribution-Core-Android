#!/usr/bin/env node
/*
 * Build the smallest useful context packet for one AI-DLC stage.
 *
 * The generated project owns the three inputs under <root>/.aidlc. This helper
 * deliberately does not load skill files or ticket artifacts; it names the
 * exact files the caller should load next.
 */
"use strict";

const fs = require("fs");
const path = require("path");

const CORE_FILES = ["context.md", "context-collection.md", "pipelines.json"];

function usage() {
  return "usage: node stage-context.js <stage> [--root <project-root>] [--flow <flow-id>] [--ticket-dir <dir>]";
}

function fail(message) {
  process.stderr.write("\u2717 stage-context: " + message + "\n");
  process.exitCode = 1;
}

function parseArgs(argv) {
  const options = { root: process.cwd(), flow: process.env.AIDLC_FLOW || null, ticketDir: null };
  let stage = null;

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "-h" || arg === "--help") return { help: true };
    if (arg === "--root" || arg === "--flow" || arg === "--ticket-dir") {
      if (i + 1 >= argv.length || argv[i + 1].startsWith("--")) {
        throw new Error(arg + " needs a value");
      }
      const value = argv[++i];
      if (arg === "--root") options.root = value;
      if (arg === "--flow") options.flow = value;
      if (arg === "--ticket-dir") options.ticketDir = value;
      continue;
    }
    if (arg.startsWith("--")) throw new Error("unknown option: " + arg);
    if (stage !== null) throw new Error("unexpected argument: " + arg);
    stage = arg;
  }

  if (!stage) throw new Error(usage());
  options.root = path.resolve(options.root);
  const envTicketDir = process.env.AIDLC_OUTPUT_DIR;
  const selectedTicketDir = options.ticketDir || envTicketDir || null;
  options.ticketDir = selectedTicketDir ? path.resolve(selectedTicketDir) : null;
  return { help: false, stage, options };
}

function readCore(root) {
  const aidlcDir = path.join(root, ".aidlc");
  const paths = Object.create(null);
  const missing = [];
  for (const name of CORE_FILES) {
    const file = path.join(aidlcDir, name);
    paths[name] = file;
    try {
      if (!fs.statSync(file).isFile()) missing.push(file);
    } catch (_) {
      missing.push(file);
    }
  }
  if (missing.length) {
    throw new Error("missing core file" + (missing.length === 1 ? "" : "s") + ": " + missing.join(", "));
  }

  const context = fs.readFileSync(paths["context.md"], "utf8");
  const collection = fs.readFileSync(paths["context-collection.md"], "utf8");
  let pipelines;
  try {
    pipelines = JSON.parse(fs.readFileSync(paths["pipelines.json"], "utf8"));
  } catch (error) {
    throw new Error("invalid " + paths["pipelines.json"] + ": " + error.message);
  }
  if (!pipelines || typeof pipelines !== "object" || !pipelines.flows || typeof pipelines.flows !== "object") {
    throw new Error(paths["pipelines.json"] + " must contain a flows object");
  }
  return { aidlcDir, context, collection, pipelines };
}

function cleanInlineMarkdown(value) {
  return String(value || "")
    .replace(/`([^`]*)`/g, "$1")
    .replace(/\*\*([^*]*)\*\*/g, "$1")
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .trim();
}

function splitTableRow(line) {
  let value = line.trim();
  if (!value.startsWith("|")) return [];
  if (value.endsWith("|")) value = value.slice(0, -1);
  value = value.slice(1);
  const cells = [];
  let cell = "";
  let escaped = false;
  let inCode = false;
  for (const char of value) {
    if (escaped) {
      cell += char;
      escaped = false;
    } else if (char === "\\") {
      cell += char;
      escaped = true;
    } else if (char === "`") {
      cell += char;
      inCode = !inCode;
    } else if (char === "|" && !inCode) {
      cells.push(cell.trim());
      cell = "";
    } else {
      cell += char;
    }
  }
  cells.push(cell.trim());
  return cells;
}

function headingLevel(line) {
  const match = /^(#{1,6})\s+/.exec(line);
  return match ? match[1].length : 0;
}

function findHeadingRange(markdown, pattern) {
  const lines = markdown.replace(/\r\n?/g, "\n").split("\n");
  const start = lines.findIndex((line) => pattern.test(line));
  if (start < 0) return null;
  const level = headingLevel(lines[start]);
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i += 1) {
    const nextLevel = headingLevel(lines[i]);
    if (nextLevel && nextLevel <= level) {
      end = i;
      break;
    }
  }
  return { lines, start, end, level };
}

function parseLoadContracts(collection) {
  const range = findHeadingRange(collection, /^###\s+Per-stage load contract\b/i);
  if (!range) throw new Error("context-collection.md has no per-stage load-contract section");
  const contracts = new Map();
  for (let i = range.start + 1; i < range.end; i += 1) {
    const cells = splitTableRow(range.lines[i]);
    if (cells.length < 3) continue;
    const first = cleanInlineMarkdown(cells[0]);
    if (/^stage\b/i.test(first) || /^[-:\s]+$/.test(first)) continue;
    const stage = first.replace(/\s*\([^)]*\)\s*$/, "").trim();
    if (!stage) continue;
    contracts.set(stage, {
      stage,
      topicsCell: cleanInlineMarkdown(cells[1]),
      machineryCell: cleanInlineMarkdown(cells[2]),
    });
  }
  if (!contracts.size) throw new Error("context-collection.md per-stage load-contract table is empty");
  return contracts;
}

function manifestEntries(pipelines) {
  const entries = [];
  for (const flow of Object.keys(pipelines.flows)) {
    const steps = pipelines.flows[flow];
    if (!Array.isArray(steps)) throw new Error("pipeline flow " + flow + " must be an array");
    steps.forEach((step, index) => {
      if (!step || typeof step !== "object") throw new Error("pipeline flow " + flow + " has an invalid step");
      entries.push({ flow, index, step, steps });
    });
  }
  return entries;
}

function canonicalStage(requested, entries) {
  const target = String(requested).replace(/^\//, "");
  const matches = entries.filter(({ step }) => {
    const command = String(step.command || "").replace(/^\//, "");
    return step.agent === requested || step.skill === requested || command === target;
  });
  if (!matches.length) {
    const valid = Array.from(new Set(entries.map(({ step }) => step.agent || step.skill).filter(Boolean))).sort();
    throw new Error("unknown stage " + JSON.stringify(requested) + "; expected one of: " + valid.join(", "));
  }
  const canonical = matches[0].step.agent || matches[0].step.skill;
  if (matches.some(({ step }) => (step.agent || step.skill) !== canonical)) {
    throw new Error("stage alias " + JSON.stringify(requested) + " is ambiguous");
  }
  return canonical;
}

function findLoadContract(stage, entries, contracts) {
  if (contracts.has(stage)) return contracts.get(stage);
  const stageEntry = entries.find(({ step }) => (step.agent || step.skill) === stage);
  if (stageEntry && contracts.has(stageEntry.step.skill)) return contracts.get(stageEntry.step.skill);
  throw new Error("no per-stage load contract for " + stage + " in context-collection.md");
}

function parseTopics(value) {
  if (!value || /^[\u2014-]$/.test(value.trim())) return [];
  return value.split(",").map((topic) => topic.trim()).filter(Boolean);
}

function machinerySectionNumbers(value) {
  const numbers = [];
  const regex = /\u00a7\s*(\d+(?:\.\d+)?)/g;
  let match;
  while ((match = regex.exec(value)) !== null) numbers.push(match[1]);
  return Array.from(new Set(numbers));
}

function parseH2Sections(markdown) {
  const lines = markdown.replace(/\r\n?/g, "\n").split("\n");
  const starts = [];
  for (let i = 0; i < lines.length; i += 1) {
    const match = /^##\s+(.+?)\s*$/.exec(lines[i]);
    if (match) starts.push({ index: i, title: match[1] });
  }
  return starts.map((item, index) => {
    const end = index + 1 < starts.length ? starts[index + 1].index : lines.length;
    const numberMatch = /^(\d+(?:\.\d+)*)\.\s*/.exec(item.title);
    return {
      title: item.title,
      number: numberMatch ? numberMatch[1] : null,
      heading: lines[item.index],
      body: lines.slice(item.index + 1, end).join("\n"),
      order: item.index,
    };
  });
}

function classifySection(section) {
  const heading = cleanInlineMarkdown(section.title)
    .replace(/^\d+(?:\.\d+)*\.\s*/, "")
    .replace(/\{\{[^}]+\}\}/g, "")
    .toLowerCase();
  const tags = new Set();

  if (/^(?:project\s+)?ground rules\b|\binvariants?\b/.test(heading)) tags.add("ground-rules");
  if (/what\s+(?:the\s+)?app\s+is|project\s+(?:overview|identity)|product\s+(?:overview|domain)|\bapp(?:lication)?\s+(?:overview|domain)|^domain\b/.test(heading)) tags.add("app-domain");
  if (/\bmodules?\b|\bmodule\s*\/\s*structure\b|\bpackages?\s+(?:and|\/)\s+structure\b/.test(heading)) tags.add("modules");
  if (/\barchitecture\b|\barchitectural\b/.test(heading)) tags.add("architecture");
  if (/\bnetwork(?:ing)?\b|\bdata\s+access\b|\brepositor(?:y|ies)\b|\bremote\s+(?:source|data|api)\b|\bapi\s+(?:access|layer|client)\b/.test(heading)) tags.add("data");
  if (/\bui\b.*\bstate\b|\bstate\s+management\b|\bpresentation\s+state\b|\bmvi\b|\bmvvm\b/.test(heading)) tags.add("ui-state");
  if (/\bui\b|\buser\s+interface\b|\bview\s+layer\b|\bpresentation\s+layer\b|\breusable\s+(?:ui\s+)?components?\b|\bcompose\b|\bnavigation\b/.test(heading)) tags.add("ui");
  if (/\bdependency\s+injection\b|(?:^|\W)di(?:\W|$)|\binjection\s+wiring\b/.test(heading)) tags.add("di");
  if (/\bstorage\b|\bcoroutines?\b|\bflavou?rs?\b|\bbuild(?:\s+configuration)?\b|\bdatabase\s+configuration\b/.test(heading)) tags.add("storage");
  if (/\bnaming\b|\bname\s+conventions?\b/.test(heading)) tags.add("naming");
  if (/\btesting\s+stack\b|\btest\s+tooling\b|\btesting\s+tooling\b|\btest\s+infrastructure\b/.test(heading)) tags.add("test-tooling");
  if (/\bhigh[-\s]?risk\b|\brisk\s+areas?\b|\bfragile\s+areas?\b/.test(heading)) tags.add("high-risk");
  return tags;
}

function tagsForTopic(topic) {
  const value = topic.toLowerCase().replace(/\s+/g, " ").trim();
  if (value.includes("ground rule") || value.includes("invariant")) return new Set(["ground-rules"]);
  if (value.includes("ui/data") || (value.includes("ui") && value.includes("data") && value.includes("pattern"))) {
    return new Set(["ui", "ui-state", "data"]);
  }
  if (value.includes("app/domain") || value.includes("app domain") || value.includes("project identity")) return new Set(["app-domain"]);
  if (value.includes("module") || value.includes("structure")) return new Set(["modules"]);
  if (value.includes("architecture")) return new Set(["architecture"]);
  if (value === "ui" || value.includes("view layer") || value.includes("user interface")) return new Set(["ui", "ui-state"]);
  if (value.includes("ui state") || value.includes("state management")) return new Set(["ui-state"]);
  if (value === "data" || value.includes("data access") || value.includes("network")) return new Set(["data"]);
  if (value === "di" || value.includes("dependency injection")) return new Set(["di"]);
  if (value.includes("storage") || value.includes("build/flavor")) return new Set(["storage"]);
  if (value.includes("high-risk") || value.includes("high risk") || value.includes("risk area")) return new Set(["high-risk"]);
  if (value.includes("naming")) return new Set(["naming"]);
  if (value.includes("test tooling") || value.includes("testing stack")) return new Set(["test-tooling"]);
  return new Set();
}

function cleanScaffoldBody(body) {
  const marker = /\bTODO\b|\{\{[A-Z0-9_]+\}\}/i;
  const hadScaffold = marker.test(body);
  let value = body.replace(/<!--[\s\S]*?\bTODO\b[\s\S]*?-->/gi, "");
  const lines = value.split("\n");
  const withoutTodoQuotes = [];
  for (let i = 0; i < lines.length; i += 1) {
    if (/^\s*>/.test(lines[i])) {
      const block = [];
      while (i < lines.length && /^\s*>/.test(lines[i])) block.push(lines[i++]);
      i -= 1;
      if (!marker.test(block.join("\n"))) withoutTodoQuotes.push(...block);
      continue;
    }
    withoutTodoQuotes.push(lines[i]);
  }
  value = withoutTodoQuotes
    .join("\n")
    .split(/\n{2,}/)
    .map((block) => {
      if (!marker.test(block)) return block;
      const blockLines = block.split("\n");
      const itemStart = /^\s*(?:[-*+]\s+|\d+[.)]\s+)/;
      if (!blockLines.some((line) => itemStart.test(line))) return "";
      const items = [];
      let current = [];
      blockLines.forEach((line) => {
        if (itemStart.test(line) && current.length) {
          items.push(current);
          current = [];
        }
        current.push(line);
      });
      if (current.length) items.push(current);
      return items.filter((item) => !marker.test(item.join("\n"))).map((item) => item.join("\n")).join("\n");
    })
    .filter(Boolean)
    .join("\n\n")
    .replace(/\n{3,}/g, "\n\n")
    .trim();
  // A scaffold subsection often consists of a heading followed only by TODO
  // prose. Once that prose is removed, drop the orphan heading as well.
  const cleanedLines = value.split("\n");
  value = cleanedLines.filter((line, index) => {
    const heading = /^(#{3,6})\s+/.exec(line);
    if (!heading) return true;
    const level = heading[1].length;
    for (let i = index + 1; i < cleanedLines.length; i += 1) {
      const nextHeading = /^(#{1,6})\s+/.exec(cleanedLines[i]);
      if (nextHeading && nextHeading[1].length <= level) break;
      if (cleanedLines[i].trim() && !/^---\s*$/.test(cleanedLines[i])) return true;
    }
    return false;
  }).join("\n").trim();
  value = value.replace(/^(?:---\s*\n)+/, "").replace(/(?:\n---\s*)+$/, "").trim();
  return { body: value, hadScaffold };
}

function cleanHeading(heading) {
  return heading
    .replace(/\s*[\u2014-]\s*\{\{[A-Z0-9_]+\}\}\s*$/i, "")
    .replace(/\{\{[A-Z0-9_]+\}\}/gi, "")
    .trim();
}

function projectContextPacket(context, topics) {
  const sections = parseH2Sections(context).map((section) => ({
    ...section,
    tags: classifySection(section),
  }));
  const selected = new Set();
  const missing = [];
  const todo = [];
  const empty = [];
  const cleanedByIndex = new Map();

  topics.forEach((topic) => {
    const wanted = tagsForTopic(topic);
    const matches = wanted.size
      ? sections.filter((section) => Array.from(wanted).some((tag) => section.tags.has(tag)))
      : [];
    if (!matches.length) {
      missing.push(topic);
      return;
    }
    let hasContent = false;
    let hasScaffold = false;
    matches.forEach((section) => {
      const sectionIndex = sections.indexOf(section);
      selected.add(sectionIndex);
      let cleaned = cleanedByIndex.get(sectionIndex);
      if (!cleaned) {
        cleaned = cleanScaffoldBody(section.body);
        cleanedByIndex.set(sectionIndex, cleaned);
      }
      if (cleaned.body) hasContent = true;
      if (cleaned.hadScaffold || /\{\{[A-Z0-9_]+\}\}/i.test(section.heading)) hasScaffold = true;
    });
    if (hasScaffold) todo.push(topic);
    if (!hasContent) empty.push(topic);
  });

  const blocks = Array.from(selected)
    .sort((a, b) => sections[a].order - sections[b].order)
    .map((index) => {
      const section = sections[index];
      const cleaned = cleanedByIndex.get(index) || cleanScaffoldBody(section.body);
      const heading = cleanHeading(section.heading).replace(/^##\s+/, "### ");
      return cleaned.body ? heading + "\n\n" + cleaned.body : null;
    })
    .filter(Boolean);

  return {
    blocks,
    gaps: {
      missing: Array.from(new Set(missing)),
      todo: Array.from(new Set(todo)),
      empty: Array.from(new Set(empty)),
    },
  };
}

function machineryPacket(collection, numbers) {
  const sections = parseH2Sections(collection);
  return numbers.map((number) => {
    const section = sections.find((candidate) => candidate.number === number);
    if (!section) throw new Error("context-collection.md is missing required \u00a7" + number);
    const cleaned = cleanScaffoldBody(section.body);
    const heading = cleanHeading(section.heading).replace(/^##\s+/, "### ");
    return cleaned.body ? heading + "\n\n" + cleaned.body : heading;
  });
}

function validateStep(step, flow) {
  const missing = [];
  if (typeof step.agent !== "string" || !step.agent) missing.push("agent");
  if (typeof step.skill !== "string" || !step.skill) missing.push("skill");
  if (!Array.isArray(step.reads)) missing.push("reads[]");
  if (typeof step.artifact !== "string" || !step.artifact) missing.push("artifact");
  if (typeof step.execution !== "string" || !step.execution) missing.push("execution");
  if (typeof step.fanout !== "boolean") missing.push("fanout");
  if (typeof step.human_review !== "boolean") missing.push("human_review");
  if (missing.length) throw new Error("pipeline step in " + flow + " is missing: " + missing.join(", "));
}

function stepContract(step) {
  return {
    reads: step.reads.slice(),
    artifact: step.artifact,
    execution: step.execution,
    fanout: step.fanout,
    human_review: step.human_review,
  };
}

function isGuardedFlow(flow) {
  return flow === "impl-flow" || flow === "fixbug-flow" || flow === "qa-flow" || flow.startsWith("auto-");
}

function contractKey(contract) {
  return JSON.stringify(contract);
}

function fileInTicket(ticketDir, name) {
  if (!ticketDir || typeof name !== "string" || !name) return false;
  const file = path.isAbsolute(name) ? name : path.join(ticketDir, name);
  try {
    return fs.statSync(file).isFile();
  } catch (_) {
    return false;
  }
}

function evidenceScore(entry, ticketDir) {
  let score = 0;
  let evidence = 0;
  for (const read of entry.step.reads) {
    if (fileInTicket(ticketDir, read)) {
      score += 100;
      evidence += 1;
    }
  }
  entry.steps.forEach((step, index) => {
    if (typeof step.artifact !== "string" || !fileInTicket(ticketDir, step.artifact)) return;
    score += index < entry.index ? 10 : 1;
    evidence += 1;
  });
  return { score, evidence };
}

function chooseContracts(stage, entries, pipelines, requestedFlow, ticketDir) {
  let candidates = entries.filter(({ step }) => (step.agent || step.skill) === stage);
  if (requestedFlow) {
    if (!Object.prototype.hasOwnProperty.call(pipelines.flows, requestedFlow)) {
      throw new Error("unknown flow " + JSON.stringify(requestedFlow) + "; expected one of: " + Object.keys(pipelines.flows).join(", "));
    }
    candidates = candidates.filter(({ flow }) => flow === requestedFlow);
    if (!candidates.length) throw new Error("flow " + requestedFlow + " does not contain stage " + stage);
  }
  candidates.forEach(({ step, flow }) => validateStep(step, flow));

  const groupsByKey = new Map();
  candidates.forEach((entry) => {
    const contract = { ...stepContract(entry.step), guarded: isGuardedFlow(entry.flow) };
    const key = contractKey(contract);
    if (!groupsByKey.has(key)) groupsByKey.set(key, { contract, entries: [], score: 0, evidence: 0 });
    groupsByKey.get(key).entries.push(entry);
  });
  const groups = Array.from(groupsByKey.values());
  if (requestedFlow || groups.length === 1) return { groups: [groups[0]], selection: requestedFlow ? "requested" : "unambiguous" };
  if (!ticketDir) return { groups, selection: "variants" };

  groups.forEach((group) => {
    group.entries.forEach((entry) => {
      const result = evidenceScore(entry, ticketDir);
      if (result.score > group.score || (result.score === group.score && result.evidence > group.evidence)) {
        group.score = result.score;
        group.evidence = result.evidence;
      }
    });
  });
  const bestScore = Math.max(...groups.map((group) => group.score));
  const best = groups.filter((group) => group.score === bestScore);
  if (bestScore > 0 && best.length === 1) return { groups: best, selection: "inferred" };
  return { groups, selection: "variants" };
}

function json(value) {
  return JSON.stringify(value);
}

function automationPolicy(collection) {
  const range = findHeadingRange(collection, /^###\s+Ticket folder and automation\b/i);
  if (!range) throw new Error("context-collection.md has no ticket-folder/automation section");
  const body = range.lines.slice(range.start + 1, range.end).join("\n");
  const paragraph = body.split(/\n{2,}/).find((value) => /Guarded flows\b/i.test(value));
  if (!paragraph) throw new Error("context-collection.md has no guarded-flow policy");
  return paragraph.trim();
}

function formatWorkflow(stage, chosen, ticketDir, aidlcDir, guardPolicy) {
  const lines = ["## Workflow protocol", "", "stage: " + json(stage)];
  if (ticketDir) lines.push("ticket_dir: " + json(ticketDir));
  lines.push("selection: " + json(chosen.selection));

  chosen.groups.forEach((group, index) => {
    const flows = group.entries.map((entry) => entry.flow);
    const step = group.entries[0].step;
    if (chosen.groups.length > 1) lines.push("", "### Variant " + (index + 1));
    lines.push("flows: " + json(flows));
    lines.push("agent_definition: " + json(path.join(aidlcDir, "agents", step.agent + ".md")));
    lines.push("skill: " + json(step.skill));
    lines.push("reads: " + json(group.contract.reads));
    lines.push("artifact: " + json(group.contract.artifact));
    lines.push("execution: " + json(group.contract.execution));
    lines.push("fanout: " + json(group.contract.fanout));
    lines.push("human_review: " + json(group.contract.human_review));
    lines.push("guarded: " + json(group.contract.guarded));
    if (ticketDir) {
      const missingReads = group.contract.reads.filter((name) => !fileInTicket(ticketDir, name));
      if (missingReads.length) lines.push("missing_reads: " + json(missingReads));
    }
  });

  if (chosen.groups.some((group) => group.contract.guarded)) {
    lines.push("", "When `guarded` is true: " + guardPolicy);
  }

  lines.push(
    "",
    "If `selection` is `variants`, resolve the exact flow before proceeding. If the selected " +
      "contract has `missing_reads`, name the producing stage and stop. " +
    "Continue under the already-loaded `agent_definition` (load it once only if invoked directly), read the context below and `reads`, and load only atomic skills the agent conditionally routes. " +
      "Write `artifact` plus only source/support outputs permitted by the agent definition. " +
      "Use `execution` as the context boundary. Fan out only disjoint files and serialize shared integration surfaces. " +
      "Pause after writing when `human_review` is true."
  );
  return lines.join("\n");
}

function formatGaps(gaps) {
  const lines = [];
  if (gaps.todo.length) lines.push("TODO/unresolved: " + gaps.todo.map(json).join(", "));
  if (gaps.missing.length) lines.push("missing section: " + gaps.missing.map(json).join(", "));
  if (gaps.empty.length) lines.push("empty after scaffold removal: " + gaps.empty.map(json).join(", "));
  return lines;
}

function buildPacket(stageArg, options) {
  const core = readCore(options.root);
  if (options.ticketDir) {
    try {
      if (fs.existsSync(options.ticketDir) && !fs.statSync(options.ticketDir).isDirectory()) {
        throw new Error("ticket path is not a directory: " + options.ticketDir);
      }
    } catch (error) {
      if (error.message.startsWith("ticket path")) throw error;
      throw new Error("cannot inspect ticket directory " + options.ticketDir + ": " + error.message);
    }
  }

  const entries = manifestEntries(core.pipelines);
  const stage = canonicalStage(stageArg, entries);
  const contracts = parseLoadContracts(core.collection);
  const loadContract = findLoadContract(stage, entries, contracts);
  const topics = parseTopics(loadContract.topicsCell);
  const effectiveTopics = [
    "project ground rules",
    "project identity",
    ...topics.filter((topic) => !/^(?:app\s*\/\s*domain|project identity|project ground rules)$/i.test(topic)),
  ];
  const machineryNumbers = machinerySectionNumbers(loadContract.machineryCell);
  const project = projectContextPacket(core.context, effectiveTopics);
  const machinery = machineryPacket(core.collection, machineryNumbers);
  const chosen = chooseContracts(stage, entries, core.pipelines, options.flow, options.ticketDir);

  const output = [
    "# AI-DLC stage context: " + stage,
    "",
    formatWorkflow(stage, chosen, options.ticketDir, core.aidlcDir, automationPolicy(core.collection)),
    "",
    "## Project context",
    "",
    "topics: " + json(effectiveTopics),
  ];
  if (project.blocks.length) output.push("", project.blocks.join("\n\n"));
  if (machinery.length) output.push("", "## Stage machinery", "", machinery.join("\n\n"));
  const gaps = formatGaps(project.gaps);
  if (gaps.length) output.push("", "## Context gaps", "", gaps.join("\n"));
  return output.join("\n").replace(/\n{3,}/g, "\n\n").trim() + "\n";
}

function main() {
  let parsed;
  try {
    parsed = parseArgs(process.argv.slice(2));
    if (parsed.help) {
      process.stdout.write(usage() + "\n");
      return;
    }
    process.stdout.write(buildPacket(parsed.stage, parsed.options));
  } catch (error) {
    fail(error.message || String(error));
  }
}

if (require.main === module) main();

module.exports = { buildPacket, parseLoadContracts, projectContextPacket };
