# Scuttler Flee-to-Shadow — remaining in-editor work

The C++ (Tasks 1–10) and the bridge-scriptable editor wiring (Tasks 0, 12, 13, the
Task 16 `LogScuttler` trace, and Task 11's EQS graph) are committed. Two pieces are left; both
need hands in the Unreal Editor because UE 5.8 exposes no reliable Python API for StateTree
assets or full EQS validation.

Do them in this order: **rebuild → Task 14 → Task 16**.

---

## Rebuild the editor binary first

The editor is currently running a **stale `UnrealEditor-SurvivalTemplate.dll`** — from before the
`LogScuttler` trace was added (commit `3f74433`). PIE still logs the old `LogTemp: GA_FleeToShadow:
no viable hiding spot` message. Trigger a build in Rider (Build → Build Solution / the hammer) so
the committed C++ is compiled to disk, then the `LogScuttler` lines from Task 16 will appear.

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
