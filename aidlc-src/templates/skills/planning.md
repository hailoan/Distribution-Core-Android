---
name: Planning
description: During implementation-plan, turn an approved design into traceable vertical work items, small execution-ready owned tasks, and dependency waves; add agile delivery metadata only when requested; write nothing.
---

## Planning

Read the approved SOLUTION-DESIGN plus the module, architecture, DI, risk, and naming topics
already present in the stage packet. Do not open the source context files. Plan
implementation only; surface unresolved design decisions instead of making them.

### Define work items

- The ticket is the parent scope. A work item delivers one testable outcome as a vertical slice;
  use a Story-ID and formal user-story wording only when the source or requested delivery mode uses
  stories. It may touch multiple layers and modules; list every module. Never split it merely by
  layer or module.
- Tasks are the necessary data/domain/state/UI/DI/test steps inside a work item. Give each exactly one
  owner stage: `android-dev` for production/resources/config, or `testing` for test source. Omit
  layers the slice does not need.
- Make each task one coherent implementation or verification step that can complete one
  `implement -> inspect diff -> static verify -> record` loop. A task may touch multiple tightly
  coupled files, but must not combine independently executable boundaries merely to shorten the
  backlog.
- For each task, name exact existing paths and symbols. For implementation-local new files, a
  bounded planned package and responsibility is sufficient when the design intentionally leaves
  the filename unspecified. Use a narrow path pattern only when every matching file is owned.
- State the task objective, preconditions/inputs, observable done condition, verification evidence
  or linked Test-ID, dependencies, and a collision key for any shared file or boundary.
- Split production implementation from test-source authoring because they have different owner
  stages. Link them through Test-ID and dependencies; do not ask android-dev to write tests owned
  by `testing`.
- Add a foundation work item only when multiple outcomes truly require the same prerequisite contract
  and no user-visible slice can own it safely.
- Add an integration work item only when independently deliverable slices need cross-screen,
  navigation, shared-state, socket, or other final wiring.
- Add a timeboxed spike only when evidence is insufficient to estimate.

Prefer a thin, demoable walking skeleton first when it can safely prove the end-to-end path.
Otherwise order the smallest viable slice first.

### Decompose work items into executable tasks

Walk the approved behavior from its innermost required contract to integration and create only the
steps the slice actually needs. Typical boundaries include contract/model, data implementation,
state holder, UI/resources, DI registration, navigation/integration, and test coverage, but these
are prompts for inspection—not mandatory layers or permission to redesign.

Split a task when any of these is true:

- it has more than one independently verifiable objective;
- it crosses owner stages;
- part of it can run concurrently with the rest;
- it edits shared DI, database/schema, socket, navigation, shared state, public API, or build
  configuration together with otherwise isolated feature work;
- its done condition would be vague (for example, "feature works") rather than observable;
- android-dev would need to choose an unstated contract, behavior, path, or technology.

Do not split mechanically by file or force one task per layer. Keep files that must change
atomically together, such as a contract and its required signature updates. If exact scope cannot
be determined from the approved design and bounded code inspection, record an unresolved planning
input or a timeboxed spike; never disguise discovery as an implementation task.

### Optional delivery-backlog mode

Only when the user requests agile delivery metadata:

- map stable `Story-ID` to `FR-ID`, `SC-ID`, and `AC-ID`;
- write `As a … I want … so that …` and Given/When/Then acceptance criteria when meaningful;
- record modules, tasks, inputs/outputs, dependencies, MoSCoW priority, and Fibonacci points;
- keep it independent, valuable, estimable, sprint-sized, and testable.

Use the team's supplied estimation scale and capacity. Do not invent points, priority, or a
five-point split rule when the project has not supplied them. Never split by technical layer.

Resolve apparent screen/module tension in favor of the user outcome: split separate outcomes or
screens when independently demoable, but allow one outcome to cross required modules.

### Order execution

Build a dependency DAG from verified contracts and shared-file touchpoints. Place a work item in a
wave only after all dependencies are satisfied. Also order tasks inside each story and across
stories. Mark within-wave concurrency only when exact path/symbol ownership and collision keys do
not overlap; serialize actual shared infrastructure changes. Do not force a foundation first or
integration last when neither is needed.

Pack waves into sprints only when sprint packing is requested and capacity is supplied. Otherwise
omit sprint assignments entirely; the execution waves are sufficient.

Return:

- Work-item Backlog:
  `FR-ID | SC-ID | AC-ID | Work-ID/Story-ID | outcome | Screens/Modules | Depends-on`; add user-story wording, G/W/T, Points, MoSCoW, and Sprint only in requested delivery-backlog mode with supplied inputs;
- Task Backlog:
  `Task-ID | Story-ID | owner stage | objective | exact path/symbol scope | preconditions/inputs | done condition | verification/Test-ID | depends on | collision key`;
- Dependency Map;
- Execution Waves listing task IDs with concurrency/serialization and file-ownership reasons;
- Sprint Plan only when packing was requested and capacity was supplied;
- unresolved planning inputs.
