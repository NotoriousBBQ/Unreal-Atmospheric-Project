# Scuttler Flee-to-Shadow — remaining in-editor work

The C++ (Tasks 1–10) and the bridge-scriptable editor wiring (Tasks 0, 12, 13, and the
Task 16 `LogScuttler` trace) are committed. Three pieces are left, and all three need hands
in the Unreal Editor because UE 5.8 exposes no reliable Python API for EQS graphs or
StateTree assets.

Do them in this order: **Task 11 → Task 14 → Task 16**.

---

## Task 11 — Configure `EQS_FleeToShadow`

The asset exists but is **empty** (`/Game/Variant_Horror/AI/EQS_FleeToShadow`). Open it in the
EQS editor and build the graph:

**Generator — Points: Donut**
- Center: `EnvQueryContext_Querier`
- Inner Radius: `300`
- Outer Radius: `1200`
- Number of Rings: `4`
- Points Per Ring: `8`
- Arc Direction / Arc Angle: leave full 360 for now
- Project points to navigation: **on**

**Tests on the generator (in order):**
1. **Distance** → to `EnvQueryContext_Player`. Scoring: Absolute, Factor `+1.0`, Equation Linear.
2. **Trace** → Context `EnvQueryContext_Player`, `TraceFromContext = true`, Channel `Visibility`,
   ItemHeightOffset ≈ `60`, ContextHeightOffset ≈ `60`. Purpose **Filter Only**, Filter `Match = false`
   (keep points with line of sight to the player broken).
3. **Pathfinding** → Context `EnvQueryContext_Querier`, `TestMode = PathExist`. Purpose **Filter Only**,
   Filter `Match = true` (keep reachable).
4. **Distance** → to `EnvQueryContext_Querier`. Purpose **Score Only**, Factor `-0.3` (tie-break toward nearer cover).
5. **Direct Light** (`Direct Light` — the custom `UEnvQueryTest_DirectLight`) → Purpose **Filter Only**,
   `Bool Value / bIsLit = false` (keep unlit), `TraceHeightOffset ≈ 60`.

**Query settings:** Run Mode **Single Best Item**. Save (compiles on save).

Optional sanity check: drop an `EQSTestingPawn` in `Lvl_Horror`, assign the query, confirm points
generate and no test errors.

Commit: `git add SurvivalTemplate/Content/Variant_Horror/AI/EQS_FleeToShadow.uasset`
→ `Configure EQS_FleeToShadow generator and tests`

---

## Task 14 — Wire `ST_Scuttler`

Open `/Game/Variant_Horror/Blueprints/ST_Scuttler`. (It compiles clean today and is run by the
`StateTreeAIComponent` on `BP_Scuttler`.)

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
