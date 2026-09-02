# Scuttler "Flee to Shadow" — Design Spec

**Date:** 2026-09-02
**Status:** Approved for planning
**Project:** `SurvivalTemplate` (Unreal Engine 5.8.1), repo `CreepyAtmosphere`

## Summary

When `BP_Scuttler` perceives the player via its AIPerception (Sight) component,
it should retreat to a nearby position that (a) breaks line of sight to the
player and (b) is not directly lit by any active light. The target position is
chosen with an Environment Query (EQS). The move itself is performed through the
Gameplay Ability System (GAS). The StateTree (`ST_Scuttler`) owns the decision
to flee; GAS owns the movement.

This introduces a C++ runtime module to the project (previously Blueprint-only).
The C++ provides the mechanism; Blueprint/data assets provide the wiring and
tuning. This is the agreed "hybrid" delivery.

## Goals

- On sight of the player, the Scuttler moves to a reachable position that breaks
  line of sight and is not directly lit.
- Position selection uses EQS.
- Movement is executed via a GAS ability and ability task (navmesh pathfinding).
- The StateTree triggers the behavior from the perception signal and returns to
  normal behavior once the ability completes.
- "Directly lit" is evaluated against runtime lights that opt in by carrying a
  registration component (fully-dynamic Lumen pipeline; no lightmaps to sample).

## Non-goals

- No `AttributeSet` / gameplay attributes (this behavior needs none).
- No multiplayer/replication correctness beyond sane defaults (single-player).
- No fallback "least-bad point" query when every candidate fails (future work).
- No change to the flicker behavior of `BP_HorrorLight` (the light component
  exposes an active toggle, but wiring flicker→darkness is future work).
- No changes to enabled plugins.

## Delivery approach

**Hybrid.** New C++ module `SurvivalTemplate` supplies all runtime logic. EQS
query asset, the GAS ability Blueprint subclass, StateTree edits, perception
wiring exposure, and per-light tuning are done in the editor (driven via the
Rider↔Unreal bridge where possible, otherwise by hand).

`BP_Scuttler` is **reparented** to a new C++ base `AScuttlerCharacter` for a
clean GAS foundation (correct `InitAbilityActorInfo` timing, `IAbilitySystemInterface`).

## Architecture

### New C++ module

Module name: `SurvivalTemplate` — matches the existing
`+ActiveGameNameRedirects` / `/Script/SurvivalTemplate` references in
`DefaultEngine.ini`, so existing Blueprint references to that script package
resolve without redirector changes.

Files:

- `Source/SurvivalTemplate.Target.cs`
- `Source/SurvivalTemplateEditor.Target.cs`
- `Source/SurvivalTemplate/SurvivalTemplate.Build.cs`
- `Source/SurvivalTemplate/SurvivalTemplate.h` / `.cpp` — `IMPLEMENT_PRIMARY_GAME_MODULE`
- `Source/SurvivalTemplate/Public/` and `Source/SurvivalTemplate/Private/` for the classes below

`Build.cs` `PublicDependencyModuleNames`:
`Core`, `CoreUObject`, `Engine`, `InputCore`, `AIModule`, `GameplayAbilities`,
`GameplayTags`, `GameplayTasks`, `NavigationSystem`, `StateTreeModule`,
`GameplayStateTreeModule`.

`.uproject` gains a `Modules` array with one `Runtime` module entry
(`"LoadingPhase": "Default"`). Plugins array unchanged.

Build: Development Editor | Win64, once, from Rider. Project files regenerated
from the `.uproject`.

### Native gameplay tags

`Source/SurvivalTemplate/Public/SurvivalTemplateGameplayTags.h` + `.cpp`:

- `Ability.Scuttler.FleeToShadow` — the ability's identifying tag.
- `State.Scuttler.Fleeing` — loose actor tag applied while fleeing, for other
  systems to observe.

### Light registry

**`ULightRegistrySubsystem : UWorldSubsystem`**
(`Public/Light/LightRegistrySubsystem.h`)

- State: `TArray<TWeakObjectPtr<ULightSourceComponent>> RegisteredLights`.
- `void RegisterLight(ULightSourceComponent*)` / `void UnregisterLight(ULightSourceComponent*)`.
- `bool IsPointLit(const FVector& Point, const AActor* IgnoreActor) const`:
  for each registered, still-valid, active light:
  - if `FVector::Dist(Point, Light->GetComponentLocation()) > Light->Radius` → skip.
  - else line trace `Point → Light location`, visibility channel, ignoring
    `IgnoreActor` and the light's owner. Unobstructed → return `true` (lit).
  - after all lights, return `false`.
- `static ULightRegistrySubsystem* Get(const UObject* WorldContext)` helper.

**`ULightSourceComponent : USceneComponent`**
(`Public/Light/LightSourceComponent.h`)

- UPROPERTYs (`EditAnywhere`, `BlueprintReadWrite`, category `"Light Occlusion"`):
  - `float Radius = 500.f` — effective "directly lit" radius; seed from the
    owning light's attenuation radius.
  - `bool bStartsRegistered = true`.
- `bool bRegistered` (transient).
- `OnRegister` (runtime worlds only) → register if `bStartsRegistered`.
- `OnUnregister` / `EndPlay` → unregister.
- `void SetLightActive(bool)` — `BlueprintCallable`; toggles participation
  (registers/unregisters, or a flag consulted by the subsystem). Used later by
  flicker logic; not wired now.
- Scene component (not actor component) so a light rig can position the sample
  origin independently of the actor origin.

Added to `BP_HorrorLight` in-editor; `Radius` tuned per the light's attenuation.

### EQS

**`UEnvQueryContext_Player : UEnvQueryContext`**
(`Public/EQS/EnvQueryContext_Player.h`)

- `ProvideContext` → `UGameplayStatics::GetPlayerPawn(this, 0)` as the single
  context actor. Null-safe (leaves context empty if no pawn).

**`UEnvQueryTest_DirectLight : UEnvQueryTest`**
(`Public/EQS/EnvQueryTest_DirectLight.h`)

- Constructor: `Cost = EEnvTestCost::High` (does traces); `SetWorkOnFloatValues(false)`;
  `TestPurpose` defaults to *Filter and Score*; `ValidItemType = UEnvQueryItemType_Point::StaticClass()`.
- UPROPERTY `FAIDataProviderFloatValue TraceHeightOffset` (default 60.f) —
  vertical offset added to each sample point before the lit test, so the
  point→light trace is not blocked by the floor the point sits on.
- `RunTest`: get `ULightRegistrySubsystem` for the query world; for each item,
  `bool bLit = Subsystem->IsPointLit(ItemLocation + Z*offset, QueryOwner)`;
  `SetScore(TestPurpose, FilterType, bLit, /*expected*/ false)` — i.e. a lit
  point scores 0 / is filtered out; an unlit point scores 1.
- Degrades safely: no subsystem → treat all points as unlit (log a warning
  once).

**`EQS_FleeToShadow`** (EnvQuery asset, `/Game/Variant_Horror/AI/EQS_FleeToShadow`)

- Generator: **Points: Donut**, center = `Querier`.
  - `InnerRadius ≈ 300`, `OuterRadius ≈ 1200`, `NumberOfRings ≈ 4`,
    `PointsPerRing ≈ 8`. (Tune in-editor.)
- Tests (order is for readability; EQS runs cheapest-first by cost anyway):
  1. **Distance** to `EnvQueryContext_Player` — scoring, `increasing` (prefer
     farther from player), weight ~1.0. This is the "bias away from player."
  2. **Trace** (visibility) to `EnvQueryContext_Player`, `TraceFromContext`,
     `TraceChannel = Visibility` — **filter**, keep only items where the player
     is NOT visible. Guarantees the destination breaks line of sight.
  3. **Pathfinding** (`PathExist`) from `Querier` — **filter** unreachable items.
  4. **Distance** from `Querier` — scoring, `decreasing`, weight ~0.3
     (tie-breaker toward nearer cover).
  5. **`EnvQueryTest_DirectLight`** — **filter** lit items.
- Run mode: **Single Best Item**.
- Failure (no item passes): query returns no result → ability ends without
  moving (see below).

### GAS

**`AScuttlerCharacter : ACharacter`**
(`Public/AI/ScuttlerCharacter.h`)

- `UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent`
  — `CreateDefaultSubobject` in constructor; `SetIsReplicated(true)`;
  `ReplicationMode = EGameplayEffectReplicationMode::Minimal`.
- `IAbilitySystemInterface::GetAbilitySystemComponent()` → the component.
- `UPROPERTY(EditDefaultsOnly, Category="Abilities") TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities`.
- `PossessedBy(AController* NewController)`:
  `Super`; `AbilitySystemComponent->InitAbilityActorInfo(this, this)`;
  if authority and not yet granted, `GiveAbility` for each `DefaultAbilities`
  entry; set a `bAbilitiesGranted` guard.
- `UnPossessed()` → `Super` (abilities persist on the ASC; acceptable for this
  enemy).
- Perception signal (see StateTree section) also lives here.

`BP_Scuttler` reparented to this class in-editor.
`GA_Scuttler_FleeToShadow` added to `DefaultAbilities` in the BP defaults.

**`UGA_FleeToShadow : UGameplayAbility`**
(`Public/AI/GA_FleeToShadow.h`)

- Defaults: `AbilityTags` add `Ability.Scuttler.FleeToShadow`;
  `InstancingPolicy = InstancedPerActor`; `NetExecutionPolicy = ServerOnly`.
- `UPROPERTY(EditDefaultsOnly, Category="Flee") TObjectPtr<UEnvQuery> FleeQuery`.
- `UPROPERTY(EditDefaultsOnly, Category="Flee") float AcceptanceRadius = 60.f`.
- `ActivateAbility`:
  1. `AAIController* AI = Cast<AAIController>(ActorInfo->AvatarActor->GetInstigatorController())`
     (or `GetController()`); null → `EndAbility(replicate=false, wasCancelled=true)`.
  2. `FleeQuery` null → `EndAbility` (cancelled).
  3. `FEnvQueryRequest Request(FleeQuery, AvatarActor);`
     `Request.Execute(EEnvQueryRunMode::SingleResult, this, &UGA_FleeToShadow::OnQueryFinished);`
- `OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)`:
  - not success or no items → `EndAbility` (not cancelled — "nothing to do").
  - else `FVector Dest = Result->GetItemAsLocation(0);`
    create `UAbilityTask_MoveTo::MoveTo(this, Dest, AcceptanceRadius)`;
    bind `OnFinished`; `Task->ReadyForActivation()`.
- `OnMoveFinished(bool bSuccess)` → `EndAbility`.
- `EndAbility` override: ensure the move task is ended and any in-flight
  `MoveTo` request on the AIController is stopped.
- Optional `LogScuttler` category verbose logging at each decision point.

**`UAbilityTask_MoveTo : UAbilityTask`**
(`Public/AI/AbilityTask_MoveTo.h`)

- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveToFinishedDelegate, bool, bSuccess);`
- `UPROPERTY(BlueprintAssignable) FMoveToFinishedDelegate OnFinished;`
- `static UAbilityTask_MoveTo* MoveTo(UGameplayAbility* OwningAbility, FVector Destination, float AcceptanceRadius);`
- `Activate()`:
  - resolve `AAIController` from the ability's avatar; none → broadcast
    `OnFinished(false)` and `EndTask()`.
  - `EPathFollowingRequestResult::Type R = AI->MoveToLocation(Destination, AcceptanceRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true, ...);`
  - `AlreadyAtGoal` → broadcast `OnFinished(true)`, `EndTask()`.
  - `Failed` → broadcast `OnFinished(false)`, `EndTask()`.
  - `RequestSuccessful` → bind `AI->ReceiveMoveCompleted` (dynamic) →
    `OnMoveCompleted(FAIRequestID, EPathFollowingResult::Type)`.
- `OnMoveCompleted` → broadcast `OnFinished(Result == Success)`; `EndTask()`.
- `OnDestroy(bool bInOwnerFinished)` → unbind; if still moving, `AI->StopMovement()`.

**`GA_Scuttler_FleeToShadow`** (Blueprint subclass of `UGA_FleeToShadow`,
`/Game/Variant_Horror/AI/GA_Scuttler_FleeToShadow`) — only assigns
`FleeQuery = EQS_FleeToShadow`. No graph logic.

### StateTree & perception

**Perception signal** (in `AScuttlerCharacter`, unless the AIPerception
component is found to live on `AI_Scuttler`, in which case the equivalent lives
there):

- On `BeginPlay` / component init, bind
  `UAIPerceptionComponent::OnTargetPerceptionUpdated`.
- Handler: if the actor is the player pawn and the stimulus is `Sense_Sight`:
  - `bCanSeePlayer = Stimulus.WasSuccessfullySensed();`
  - on true: cache `SeenPlayer`, `LastSeenTime = GetWorld()->GetTimeSeconds()`.
- `UPROPERTY(BlueprintReadOnly) bool bCanSeePlayer;`
- `UFUNCTION(BlueprintPure) bool CanSeePlayer() const;`

**`FStateTreeCondition_CanSeePlayer`** (C++ StateTree condition,
`Public/AI/StateTreeCondition_CanSeePlayer.h`):

- Instance data: none (or a `bool bInvert`).
- `TestCondition`: fetch the context actor, `Cast<AScuttlerCharacter>`, return
  `Scuttler && Scuttler->CanSeePlayer()`.
- Registered so it appears in the StateTree editor's condition list.

**`FSTTask_ActivateAbilityByTag`** (C++ StateTree task,
`Public/AI/STTask_ActivateAbilityByTag.h`):

- Instance data: `FGameplayTag AbilityTag`, `bool bEndAbilityOnExit = true`,
  and an output/internal `bool bAbilityEnded`.
- `EnterState`:
  - get ASC from context actor via `UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent`.
  - `bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));`
  - not activated → return `EStateTreeRunStatus::Failed`.
  - bind `ASC->OnAbilityEnded` → set `bAbilityEnded` when the ended ability's
    tag matches. Return `Running`.
- `Tick`: `bAbilityEnded ? Succeeded : Running`.
- `ExitState`: unbind; if `bEndAbilityOnExit`,
  `ASC->CancelAbilities(&FGameplayTagContainer(AbilityTag))`.

**`ST_Scuttler` edits (in-editor):**

- Add a state **`Fleeing`** as a sibling of the existing idle/patrol states,
  placed **above** them in evaluation priority.
- Enter transition (on the parent selector, or as a completion/condition
  transition from the lower-priority states): condition
  `StateTreeCondition_CanSeePlayer` → go to `Fleeing`.
- `Fleeing` state:
  - Task: `STTask_ActivateAbilityByTag` with `AbilityTag = Ability.Scuttler.FleeToShadow`,
    `bEndAbilityOnExit = true`.
  - (Optional, parallel) a task applying the `State.Scuttler.Fleeing` actor tag
    for the state's duration.
- Exit transition from `Fleeing`:
  - on task `Succeeded` → transition to the normal state selector.
  - The selector re-evaluates: if `CanSeePlayer` is still true, the enter
    transition fires again → new EQS query → move again. If false → idle/patrol.
- If the task returns `Failed` (no ASC, or ability could not activate) →
  transition straight back to the normal selector so the Scuttler is never stuck.

## Data flow

```
AIPerception (Sight) --OnTargetPerceptionUpdated--> AScuttlerCharacter.bCanSeePlayer
        |
        v
ST_Scuttler: Condition_CanSeePlayer true --> enter "Fleeing"
        |
        v
STTask_ActivateAbilityByTag(Ability.Scuttler.FleeToShadow)
        |
        v
UGA_FleeToShadow.ActivateAbility
        |
        v
FEnvQueryRequest(EQS_FleeToShadow)  ---> tests:
        |                                  Distance from player (score, away)
        |                                  Trace vis to player  (filter: not visible)
        |                                  PathExist            (filter: reachable)
        |                                  Distance from querier (score, near)
        |                                  DirectLight          (filter: not lit)
        |                                     |
        |                                     v
        |                             ULightRegistrySubsystem.IsPointLit
        |                             (radius + trace vs ULightSourceComponents)
        v
best point --> UAbilityTask_MoveTo --> AIController.MoveToLocation (navmesh)
        |
        v
ReceiveMoveCompleted --> OnFinished(bSuccess) --> UGA_FleeToShadow.EndAbility
        |
        v
STTask sees ability ended --> "Fleeing" state Succeeded --> normal selector re-evaluates
```

## Error handling

| Condition | Behavior |
|---|---|
| No `AAIController` on avatar | Ability ends (cancelled); StateTree task returns to selector. |
| `FleeQuery` unset | Ability ends (cancelled); logged as an error. |
| EQS returns no item (all lit / exposed / unreachable) | Ability ends (not cancelled); `Fleeing` succeeds and selector re-evaluates. Scuttler holds position this frame; will re-try next selection while still seen. |
| `MoveToLocation` fails / path invalidated mid-move | `OnFinished(false)` → ability ends → selector re-evaluates (fresh query). |
| `ULightRegistrySubsystem` missing | `DirectLight` test treats all points as unlit; warning logged once. Behavior degrades to "break LoS only." |
| Player lost during move | Move completes; on selector re-eval `CanSeePlayer` is false → idle/patrol. Ability is not interrupted early (avoids stutter); acceptable. |
| ASC not found by StateTree task | Task returns `Failed` → selector re-evaluates; Scuttler never stuck. |

## Testing

**Automation spec** — `Source/SurvivalTemplate/Private/Tests/LightRegistrySubsystemTest.cpp`:

- Register a `ULightSourceComponent` at a known location with `Radius = 500`.
- Assert `IsPointLit` is true for a point 100 uu away with clear line of sight.
- Assert false for a point 600 uu away (outside radius).
- Spawn a blocking `StaticMeshActor` between a near point and the light; assert
  false (trace blocked).
- Unregister; assert false.

**Functional / PIE test** (via the Rider bridge, `ue_play` + `ue_get_logs`
filtered to `LogScuttler`, or a dedicated functional test map):

- Scuttler + one `BP_HorrorLight` + cover geometry in a navmesh-covered area.
- Walk the player into the Scuttler's sight cone.
- Expect: `Fleeing` state entered; EQS picks a point; Scuttler paths to it;
  final point is outside the light radius (or trace-occluded from it) and has no
  line of sight to the player; ability ends on arrival; state returns to normal.
- Move the player to keep sight → expect re-query and a second move.
- Break sight → expect return to idle/patrol.

## Prerequisites to verify via the bridge before/at implementation

1. `BP_Scuttler` current parent class — confirm reparent target compatibility
   (expected `ACharacter` or a first-person template character).
2. AIPerception component owner (`BP_Scuttler` vs `AI_Scuttler`) and that a
   Sight sense config exists; note the player's detectable setup
   (`AIPerceptionStimuliSource` / team).
3. `Lvl_Horror` navmesh coverage — presence of a Nav Mesh Bounds Volume /
   `RecastNavMesh` over the play space. Add one if missing.
4. `ST_Scuttler` existing state layout and how it is executed (StateTree AI
   component on pawn vs. controller; `AI_Scuttler` involvement).
5. `BP_HorrorLight` light attenuation radius → seed `ULightSourceComponent.Radius`.

## File / asset inventory

**New C++ (`Source/SurvivalTemplate/`):**

- `SurvivalTemplate.Build.cs`, `SurvivalTemplate.h/.cpp` (module)
- `../SurvivalTemplate.Target.cs`, `../SurvivalTemplateEditor.Target.cs`
- `Public/SurvivalTemplateGameplayTags.h` + `Private/…cpp`
- `Public/Light/LightRegistrySubsystem.h` + `Private/…cpp`
- `Public/Light/LightSourceComponent.h` + `Private/…cpp`
- `Public/EQS/EnvQueryContext_Player.h` + `Private/…cpp`
- `Public/EQS/EnvQueryTest_DirectLight.h` + `Private/…cpp`
- `Public/AI/ScuttlerCharacter.h` + `Private/…cpp`
- `Public/AI/GA_FleeToShadow.h` + `Private/…cpp`
- `Public/AI/AbilityTask_MoveTo.h` + `Private/…cpp`
- `Public/AI/StateTreeCondition_CanSeePlayer.h` + `Private/…cpp`
- `Public/AI/STTask_ActivateAbilityByTag.h` + `Private/…cpp`
- `Private/Tests/LightRegistrySubsystemTest.cpp`

**Modified:**

- `SurvivalTemplate.uproject` — add `Modules` entry.

**New / modified assets (editor):**

- `/Game/Variant_Horror/AI/EQS_FleeToShadow` (new EnvQuery)
- `/Game/Variant_Horror/AI/GA_Scuttler_FleeToShadow` (new, BP subclass of `UGA_FleeToShadow`)
- `BP_Scuttler` — reparent to `AScuttlerCharacter`; set `DefaultAbilities`
- `BP_HorrorLight` — add `ULightSourceComponent`, tune `Radius`
- `ST_Scuttler` — add `Fleeing` state, condition, task, transitions
- `Lvl_Horror` — Nav Mesh Bounds Volume if none exists (pending verification)

## Open tuning parameters (set in-editor, not blockers)

- Donut inner/outer radius, rings, points per ring.
- Test weights (away-from-player vs. near-cover).
- `ULightSourceComponent.Radius` per light.
- `UEnvQueryTest_DirectLight.TraceHeightOffset`.
- `UGA_FleeToShadow.AcceptanceRadius`.
