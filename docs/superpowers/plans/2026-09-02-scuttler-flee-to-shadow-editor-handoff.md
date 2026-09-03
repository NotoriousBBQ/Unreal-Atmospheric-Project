# Scuttler Flee-to-Shadow — COMPLETE

**Status (2026-09-02): working end to end.** In PIE the Scuttler sees the player, enters the
`Fleeing` state, runs `EQS_FleeToShadow`, and paths to an unlit, out-of-sight point:

```
LogScuttler: FleeToShadow activated (avatar=BP_Scuttler_C_..., query=EQS_FleeToShadow)
LogScuttler: query finished, 18 items, chosen=X=-2741.9 Y=580.0 Z=17.5
LogScuttler: move finished (success)
```

The sections below record the bugs found along the way — most were wrong values in this doc's own
earlier instructions or in the base template, not in the C++.

## Root causes fixed (beyond the plan tasks)

1. **`BP_Scuttler` → `Auto Possess AI` was `Spawned`.** The Scuttler is placed in the level, so
   with `Spawned` the AI controller was never created — no controller, no StateTree, nothing
   consumed perception. Set to **`Placed in World or Spawned`**. (This is also why Task 0 found
   `ST_Scuttler` "never ran": it was never possessed.)
2. **The StateTree brain moved to a new controller.** `AI_Scuttler` was replaced with
   `AIC_Scuttler` (parent `AIController`, schema `StateTreeAIComponentSchema`, holds the
   `StateTreeAIComponent`). `BP_Scuttler`'s `AI Controller Class` points at it; the
   `StateTreeAIComponent` was removed from the pawn.
3. **StateTree never re-selected.** The tree's only transition trigger was `OnStateCompleted`, so
   once it settled into `Idle` (whose task runs forever) it never re-checked the flee state's
   `Scuttler Can See Player` enter condition. **Fix:** a transition on `Idle` with
   Trigger `On Tick`, Condition `Scuttler Can See Player`, Transition To the flee state.
4. **EQS Trace test Context was `Querier`.** It traced point→Scuttler instead of point→player, so
   in an open room almost no point was "hidden". **Fix:** Context = `EnvQueryContext_Player`.
5. **EQS Distance test was filtering with Float Max = 0.** A Distance test in a `Filter` purpose
   with Max 0 keeps only points within 0 units — culls everything. **Fix:** both Distance tests
   set to **`Score Only`** (no min/max). Optionally the player-distance one can be
   `Filter and Score` with Max ≈ 1500 to cap how far the Scuttler will run.

Also corrected from this doc's earlier draft: "Project points to navigation" = Donut
Projection Data → **Trace Mode = `Navigation`**; Direct Light filter field is **"Bool Match"**
(no `bIsLit` property); Run Mode is requester-side, not on the ROOT node; the Trace test's
**Bool Match = `true`** (keep LOS-blocked points), not `false`.

## Still uncommitted at hand-off

`EQS_FleeToShadow.uasset`, `AIC_Scuttler.uasset`, `BP_Scuttler.uasset`, `ST_Scuttler.uasset`, and
one `__ExternalActors__` actor for `Lvl_Horror`. `Config/DefaultEditor.ini` also shows a large
diff — that is the editor rewriting the "Epic Headquarters" preview-scene profile, unrelated to
this feature; revert it rather than commit.

---

## Historical notes (superseded)

### Rebuild the editor binary

If PIE logs the old `LogTemp: GA_FleeToShadow: no viable hiding spot` instead of `LogScuttler`,
the editor is running a stale `UnrealEditor-SurvivalTemplate.dll`. Build in Rider
(Build → Build Solution) so the committed C++ compiles to disk.

---

## Task 11 — Configure `EQS_FleeToShadow` — **DONE (committed `81cd02e`), tuning pending**

The graph was built by hand and saved. Verified through the bridge:
- Structurally complete: **Points: Donut** generator + 5 tests (Distance ×2, Trace, Pathfinding,
  **Direct Light**) + `EnvQueryContext_Player` — the two custom C++ classes resolve, not "missing".
- `ProjectionData.TraceMode = Navigation` is serialized (points project to navmesh).
- Compiles and **executes with no errors** — `GA_FleeToShadow` ran `FEnvQueryRequest::Execute`
  against it in PIE twice with no EQS errors/warnings/crashes.

**Open item for Task 16:** in the one scenario tested (Scuttler at ~(-2601, 150), player ~1600u
away and not in sight) the query returned **0 items** ("no viable hiding spot"). That is plausibly
scenario + an untuned donut rather than a config bug, but it must be confirmed with the EQS visual
debugger in a real play session and the donut radii / test settings tuned until the query yields
points. This is the core of Task 16.

Notes from building it, for reference:
- "Project points to navigation" is **Projection Data → Trace Mode = `Navigation`** (no simple
  on/off checkbox). Set `Post Projection Vertical Offset ≈ 10`.
- Direct Light's filter field is **"Bool Match"** (inherited `BoolValue`), under the Filter
  category, shown once Test Purpose = `Filter Only`. Set it to `false` (keep unlit). There is no
  `bIsLit` property — that was only a code comment.
- Run Mode is **not** on the ROOT node — it is a requester-side setting and `GA_FleeToShadow.cpp`
  already passes `EEnvQueryRunMode::SingleResult`.

---

## Task 14 — Wire `ST_Scuttler`

Open `/Game/Variant_Horror/Blueprints/ST_Scuttler`.

> **Fix the pre-existing breakage first.** In PIE the Scuttler's StateTree currently errors with
> `UStateTreeComponentSchema::SetContextRequirements: Missing external data requirements. StateTree
> will not update.` and its component tick is disabled. This is **not** from the reparent — it is in
> the stale committed log from before this branch; `ST_Scuttler` has never run. Before adding the
> `Fleeing` state, make the tree valid: check the ST's **Schema** (should be
> `StateTreeAIComponentSchema` so the AIController/Pawn context is provided to conditions/tasks),
> and resolve any unbound context parameters the schema reports. Verify in PIE that the tree ticks
> (no `StartTree failed` error) before proceeding.

1. Add a state **`Fleeing`** as a child of the root selector, ordered **above** the existing
   idle/patrol state(s) so it wins selection.
2. On `Fleeing`, add **Enter Condition** → `Scuttler Can See Player` (`bInvert = false`).
3. On `Fleeing`, add **Task** → `Activate Ability By Tag`:
   - `AbilityTag` = `Ability.Scuttler.FleeToShadow`
   - `bEndAbilityOnExit` = `true`
4. Add a transition **On State Completed** → back to the normal selector / root selection.
   (The enter condition re-evaluates on selection, so while the player stays visible the tree
   re-enters `Fleeing` and the ability re-runs the query; when sight breaks it falls through to
   idle/patrol.)
5. Skip the optional `State.Scuttler.Fleeing` tag hooks — no consumer yet (YAGNI).
6. Compile & save.

Commit: `git add SurvivalTemplate/Content/Variant_Horror/Blueprints/ST_Scuttler.uasset`
→ `Add Fleeing state to ST_Scuttler`

---

## Task 15 — Navmesh — **SKIP**

Task 0 confirmed `Lvl_Horror` already has 1 `NavMeshBoundsVolume` and a built `RecastNavMesh`.
Nothing to do, no commit.

---

## Task 16 — Integration test & tuning

Task 16 Step 1 (the `LogScuttler` decision trace in `GA_FleeToShadow.cpp`) is **done and committed**.

Remaining:
1. In `Lvl_Horror`: confirm a `BP_Scuttler`, ≥1 `BP_HorrorLight` over an open area, and some cover geometry.
2. `show Navigation`, then PIE. Walk the player into the Scuttler's sight cone.
3. `ue_get_logs` filtered to category `LogScuttler`. Expect:
   - `FleeToShadow activated (avatar=BP_Scuttler_C_0, query=EQS_FleeToShadow)`
   - `query finished, N items, chosen=(X,Y,Z)`
   - Scuttler paths there
   - `move finished (success)`
4. Verify acceptance criteria: chosen point has no LOS to player, is not directly lit, ability
   ends and re-triggers on re-sight, and breaking sight for good returns the Scuttler to idle/patrol.
5. Tune in-editor only (no code): donut radii/rings, Distance test weights,
   `ULightSourceComponent.Radius` (currently 566), `AcceptanceRadius` (60), `TraceHeightOffset` (60).
6. Re-run the automation test — expect still green:
   `UnrealEditor-Cmd.exe SurvivalTemplate.uproject -ExecCmds="Automation RunTests SurvivalTemplate.Light.IsPointLit; Quit" -unattended -nopause -nosplash -nullrhi`

Commit any asset tuning: `Tune Scuttler flee-to-shadow EQS and light radii`.
