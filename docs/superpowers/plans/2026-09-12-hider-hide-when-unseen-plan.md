# BP_Hider — hide once spotted, then out of sight

**Status (2026-09-12): design approved, not yet implemented.** This is the outcome of a
brainstorming session; the user is doing the implementation themselves (learning Unreal),
so this doc captures the approved plan to pick up later.

## Goal

A new pawn, `BP_Hider`, that reuses the existing Scuttler flee machinery
(`GA_Scuttler_FleeToShadow`, `EQS_FleeToShadow`) but with a different trigger: instead of
fleeing *while* it can see the player (Scuttler's behavior), the Hider only flees once it
has been "spotted" (mutual eye contact) and then loses direct sight of the player.

`ST_Scuttler` and `BP_Scuttler`'s existing behavior are **not changed** — this is purely
additive. A separate `ST_Hider` StateTree carries the new logic.

## Current relevant implementation (as of this doc)

- `AScuttler` (`Source/SurvivalTemplate/{Public,Private}/AI/Scuttler.h/.cpp`) owns
  `bCanSeePlayer`, `CanSeePlayer()` (BlueprintPure), `SetCanSeePlayer()` (BlueprintCallable),
  and a native `HandleTargetPerceptionUpdated(AActor*, FAIStimulus)` bound in `BeginPlay()`
  to the pawn's `UAIPerceptionComponent::OnTargetPerceptionUpdated`. This function is a
  plain `UFUNCTION()`, **not** `BlueprintNativeEvent` — Blueprint cannot override it.
- `FStateTreeCondition_CanSeePlayer` (`StateTreeCondition_CanSeePlayer.h/.cpp`) is a native
  StateTree condition ("Scuttler Can See Player" in the editor) that casts the StateTree's
  owner (or the owning AIController's pawn) to `AScuttler` and returns `CanSeePlayer()`,
  with an optional `bInvert`.
- `ST_Scuttler`'s `Fleeing` state has Enter Condition `Scuttler Can See Player` (not
  inverted) and an `On Tick` transition out of `Idle` re-checking that condition (needed —
  see "Known gotchas" below).
- `GA_Scuttler_FleeToShadow` is a Blueprint subclass of `UGA_FleeToShadow` with
  `FleeQuery = EQS_FleeToShadow` set; it carries gameplay tag `Ability.Scuttler.FleeToShadow`.
  Gameplay tags are not class-name-scoped, so this ability (and the EQS it references) can
  be granted to and activated from any pawn/StateTree — reuse by Hider is safe as-is, no
  duplication needed.
- `AIC_Scuttler` is a generic AIController Blueprint (parent `AIController`, schema
  `StateTreeAIComponentSchema`, holds a `StateTreeAIComponent`) — nothing Scuttler-specific,
  safe to reuse directly for `BP_Hider`.

## Approved design

### 1. Small C++ addition (mirrors existing code exactly)

- Add to `AScuttler`: `bool bHasSeenPlayer`, `bool HasSeenPlayer() const` (BlueprintPure),
  `void SetHasSeenPlayer(bool bNewValue)` (BlueprintCallable) — same shape as the existing
  `bCanSeePlayer` members.
- Add a new native StateTree condition `FStateTreeCondition_HasSeenPlayer`
  (`StateTreeCondition_HasSeenPlayer.h/.cpp`), a near-copy of
  `FStateTreeCondition_CanSeePlayer` but calling `HasSeenPlayer()` instead of
  `CanSeePlayer()`. Display name suggestion: "Scuttler Has Seen Player" (keeps the existing
  "Scuttler" category naming since it lives on the shared `AScuttler` base).

This is the only C++ needed. Everything else below is Blueprint/editor work.

### 2. `BP_Hider`

- New Blueprint, parent `AScuttler` (same as `BP_Scuttler`).
- `AI Controller Class` = `AIC_Scuttler` (reused, not duplicated).
- `DefaultAbilities` includes `GA_Scuttler_FleeToShadow` (reused, not duplicated).
- In `BP_Hider`'s EventGraph, bind a **second**, independent Blueprint event to the
  `AIPerceptionComponent`'s `OnTargetPerceptionUpdated` delegate (e.g. via "Assign On
  Target Perception Updated"). This runs *alongside* the inherited native handler — `Can
  See Player` keeps working exactly as it does today; this new handler only adds the
  front-arc check and sets `Has Seen Player`.
  - Filter: `Actor == player pawn` and `Stimulus` sense is Sight and
    `Stimulus.WasSuccessfullySensed()` — same filtering the native handler does.
  - Front-arc check: angle between the player's forward vector and the vector from the
    player to the Hider, compared against a tunable half-angle (expose as an `EditAnywhere`
    float on `BP_Hider`, e.g. `FrontArcHalfAngleDegrees`). This approximates "the player
    would also notice me" without needing perception on the player itself.
  - If both the sight check and the front-arc check pass: `SetHasSeenPlayer(true)`. This is
    a one-way latch here — never cleared from this handler.

### 3. `ST_Hider` (new StateTree, cloned from `ST_Scuttler`'s pattern)

- `Fleeing` state Enter Conditions (AND'd): `Scuttler Can See Player` (**Invert = true**)
  **and** `Scuttler Has Seen Player` (Invert = false).
- Same `Activate Ability By Tag` task as `ST_Scuttler`'s Fleeing state:
  `AbilityTag = Ability.Scuttler.FleeToShadow`, `bEndAbilityOnExit = true`.
- Add an **Exit Action** on the `Fleeing` state that calls `SetHasSeenPlayer(false)`. This
  fires whenever the state is left for any reason (ability succeeded, failed, or was
  interrupted), so the Hider's "alert" always resets once it's done reacting — no need to
  touch the shared ability to reset the flag.
- Carry over the same `On Tick` re-check transition pattern `ST_Scuttler` uses on `Idle`
  (see "Known gotchas" below) — without it the tree will never re-select `Fleeing` once it
  settles into `Idle`.

### Explicitly out of scope (YAGNI for v1)

- No time-based decay/memory timer for `Has Seen Player`. The Exit Action clears it
  reliably in every case StateTree tracks. Add a decay timer later only if a real scenario
  surfaces where that isn't enough.
- No real "jump"/perception-on-the-player system — the front-arc heuristic is a
  deliberate approximation, not a full mutual-perception simulation.

## Known gotchas from the Scuttler implementation (avoid repeating on Hider)

These bit us during Scuttler's implementation and will bite `BP_Hider` too if skipped —
see `2026-09-02-scuttler-flee-to-shadow-editor-handoff.md` for full detail:

1. **`Auto Possess AI` must be `Placed in World or Spawned`** (not `Spawned`) for a pawn
   placed directly in the level, or the AI controller/StateTree never runs at all.
2. **The AIController's `PathFollowingComponent` needs `Auto Activate = True`.** If it's
   off (it was on `AIC_Scuttler`'s class default), the first AI move works and every move
   after it silently "succeeds" without ever translating the pawn.
3. **StateTree needs an `On Tick` transition to re-select a state**, not just
   `OnStateCompleted` — otherwise once it settles into `Idle` it never re-checks whether
   `Fleeing`'s enter conditions have become true again.
4. If `BP_Hider` uses the same `ABP_Unarmed` anim Blueprint: `ShouldMove` there is
   `GroundSpeed > 0.01` alone (the old `Acceleration != 0` AND-clause was removed — see the
   Scuttler handoff doc — because AI nav movement doesn't reliably keep `Acceleration`
   nonzero even while genuinely moving).

## Suggested implementation order

1. Add `bHasSeenPlayer`/`HasSeenPlayer()`/`SetHasSeenPlayer()` to `Scuttler.h`/`.cpp`.
2. Add `StateTreeCondition_HasSeenPlayer.h`/`.cpp` (copy of
   `StateTreeCondition_CanSeePlayer`, swap the accessor called).
3. Build/compile.
4. Create `BP_Hider` (parent `AScuttler`); set `AI Controller Class` and `DefaultAbilities`.
5. Wire the second `OnTargetPerceptionUpdated` binding + front-arc check in `BP_Hider`.
6. Create `ST_Hider`: `Fleeing` state with the two Enter Conditions, the existing
   `Activate Ability By Tag` task, the `On Tick` re-check transition, and the new Exit
   Action clearing `Has Seen Player`.
7. Place a `BP_Hider` in `Lvl_Horror`; double-check gotchas #1 and #2 above before testing.
8. Test in PIE: walk into the Hider's front arc while it can see you, then break line of
   sight — it should flee to shadow. Walking past without ever entering the front arc
   should not trigger a flee even if it technically "sees" you.
