# Scuttler "Flee to Shadow" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When `BP_Scuttler` sees the player, it uses EQS to pick a reachable, unlit position that breaks line of sight and moves there via a GAS ability.

**Architecture:** A new C++ runtime module (`SurvivalTemplate`) supplies the mechanism: a light registry the "is this point lit" EQS test consults, a GameplayAbility + AbilityTask that runs the query and drives a navmesh move, and StateTree condition/task nodes that connect the perception signal to the ability. Editor data (an EQS query asset, a one-node ability Blueprint subclass, StateTree edits, a component on `BP_HorrorLight`, and a reparent of `BP_Scuttler`) wires it together. The StateTree owns the decision to flee; GAS owns the move.

**Tech Stack:** Unreal Engine 5.8.1, C++; plugins GameplayAbilities (GAS), GameplayStateTree/StateTree, AIModule (EQS, perception, navigation). Rider for building; the Rider↔Unreal MCP bridge (`ue_*` tools) for editor-side work and running automation tests in the live editor.

**Spec:** `docs/superpowers/specs/2026-09-02-scuttler-flee-to-shadow-design.md` — read it alongside this plan.

## Global Constraints

- Engine version: **Unreal Engine 5.8.1**. `EngineAssociation` in `.uproject` is `"5.8"`.
- The project is currently **Blueprint-only** — this plan adds the first C++ module. After it lands, opening the `.uproject` prompts to rebuild; Rider builds target **Development Editor | Win64**.
- C++ module name **must be** `SurvivalTemplate` — `DefaultEngine.ini` already has `+ActiveGameNameRedirects` mapping `TP_FirstPersonBP` → `/Script/SurvivalTemplate`, and existing Blueprints reference `/Script/SurvivalTemplate`.
- Rendering is fully dynamic (Lumen, Substrate, `r.AllowStaticLighting=False`). **Do not** add static/baked lights or lightmass. The light test works off runtime light actors only.
- Custom trace channel `ECC_GameTraceChannel1` = `"Projectile"` exists. Light-occlusion and visibility traces in this feature use `ECC_Visibility`, not the Projectile channel.
- Every StateTree / GAS / EQS C++ API in this plan must be checked against the installed engine headers before use — the StateTree and GAS APIs shifted across 5.x releases. Engine source lives under the UE 5.8 install (e.g. `…/UE_5.8/Engine/Source/Runtime/`). Where this plan's signature disagrees with the header, the header wins; note the deviation in the commit message.
- Naming: C++ types use the project's default prefix conventions (`U`/`A`/`F`). New content assets live under `/Game/Variant_Horror/AI/`.
- Commit after every task. Use present-tense imperative subjects. End every commit message with:
  ```
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01MPB5LpRaxjWbvW3cLK244K
  ```

---

## File Structure

**New C++ (`SurvivalTemplate/Source/`):**

| File | Responsibility |
|---|---|
| `SurvivalTemplate.Target.cs`, `SurvivalTemplateEditor.Target.cs` | Build targets |
| `SurvivalTemplate/SurvivalTemplate.Build.cs` | Module deps |
| `SurvivalTemplate/SurvivalTemplate.h` / `.cpp` | Primary game module impl |
| `SurvivalTemplate/Public/SurvivalTemplateGameplayTags.h` / `Private/SurvivalTemplateGameplayTags.cpp` | Native gameplay tags |
| `Public/Light/LightSourceComponent.h` / `Private/...cpp` | Per-light opt-in: position + radius, self-(un)registers |
| `Public/Light/LightRegistrySubsystem.h` / `Private/...cpp` | Tracks lights; `IsPointLit` query |
| `Public/EQS/EnvQueryContext_Player.h` / `Private/...cpp` | EQS context → player pawn |
| `Public/EQS/EnvQueryTest_DirectLight.h` / `Private/...cpp` | EQS test: filter/score by lit state |
| `Public/AI/ScuttlerCharacter.h` / `Private/...cpp` | C++ base for `BP_Scuttler`: ASC + perception signal |
| `Public/AI/AbilityTask_MoveTo.h` / `Private/...cpp` | AbilityTask wrapping AIController navmesh MoveTo |
| `Public/AI/GA_FleeToShadow.h` / `Private/...cpp` | GameplayAbility: run EQS, move to result |
| `Public/AI/StateTreeCondition_CanSeePlayer.h` / `Private/...cpp` | StateTree condition reading the perception signal |
| `Public/AI/STTask_ActivateAbilityByTag.h` / `Private/...cpp` | StateTree task: activate ability by tag, finish on end |
| `Private/Tests/LightRegistrySubsystemTest.cpp` | Automation spec for `IsPointLit` |

**Modified:** `SurvivalTemplate.uproject` (add `Modules`).

**Editor assets (Phase 5, via the bridge):**
`/Game/Variant_Horror/AI/EQS_FleeToShadow`, `/Game/Variant_Horror/AI/GA_Scuttler_FleeToShadow`; edits to `BP_Scuttler`, `BP_HorrorLight`, `ST_Scuttler`, `Lvl_Horror`.

---

## Task 0: Verify editor prerequisites via the bridge

**Files:** none (writes findings into this plan file).

**Interfaces:**
- Produces: confirmed facts that Tasks 6, 9, and Phase 5 depend on — `BP_Scuttler` parent class, which actor owns the `UAIPerceptionComponent`, whether `Lvl_Horror` has a navmesh, how `ST_Scuttler` is executed, and `BP_HorrorLight`'s attenuation radius.

- [ ] **Step 1: Confirm the bridge is up**

Run `ue_health` (rootFolder `C:/Projects/CreepyAtmosphere/SurvivalTemplate`). Expected: `{"connected":true,...}`. If `connected:false`, the Unreal Editor is not running with RiderLink — stop and ask the user to open `SurvivalTemplate.uproject` in the editor.

- [ ] **Step 2: Read the Scuttler assets**

Run `ue_execute_python` with:

```python
import unreal
def dump(path):
    a = unreal.EditorAssetLibrary.load_asset(path)
    if not a:
        print(path, "-> MISSING"); return
    gen = a.get_class() if hasattr(a, "get_class") else None
    print("ASSET", path, "class:", a.get_class().get_name())
    if isinstance(a, unreal.Blueprint):
        print("  parent:", a.parent_class.get_name() if a.parent_class else None)
for p in [
    "/Game/Variant_Horror/Blueprints/BP_Scuttler",
    "/Game/Variant_Horror/Blueprints/AI_Scuttler",
    "/Game/Variant_Horror/Blueprints/ST_Scuttler",
    "/Game/Variant_Horror/Blueprints/BP_HorrorLight",
]:
    dump(p)
# Components on BP_Scuttler / AI_Scuttler CDOs
for p in ["/Game/Variant_Horror/Blueprints/BP_Scuttler", "/Game/Variant_Horror/Blueprints/AI_Scuttler"]:
    bp = unreal.EditorAssetLibrary.load_asset(p)
    gc = unreal.load_object(None, bp.generated_class().get_path_name()) if bp else None
    cdo = unreal.get_default_object(bp.generated_class()) if bp else None
    if cdo:
        comps = [c.get_name() for c in cdo.get_components_by_class(unreal.ActorComponent)]
        print(p, "components:", comps)
```

Record in a new `## Task 0 Findings` section at the bottom of this plan:
- `BP_Scuttler` parent class (Task 6 reparents to `AScuttlerCharacter`; the current parent must be `Character` or a `Character` subclass — if it is a template FP character with logic worth keeping, note it).
- Which actor has `AIPerceptionComponent` (`BP_Scuttler` or `AI_Scuttler`). Task 9 attaches the perception delegate on whichever owns it.
- `BP_HorrorLight` component list and, if a `PointLight`/`SpotLight` component is present, its `AttenuationRadius` (seed for Task 13).

- [ ] **Step 3: Check the navmesh and StateTree execution**

```python
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NavMeshBoundsVolume)
print("NavMeshBoundsVolumes in current level:", len(actors))
rnm = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
print("RecastNavMesh actors:", len(rnm))
```

Record: whether `Lvl_Horror` has nav bounds (Task 15 adds one only if zero). Also open `ST_Scuttler` notes: whether a `StateTreeComponent` / `StateTreeAIComponent` sits on `BP_Scuttler` or `AI_Scuttler` (from Step 2's component dump), and the names of its existing top-level states (needed for Task 14's transition wiring). If the state names are not visible from Python, note "inspect in editor during Task 14".

- [ ] **Step 4: Commit the findings**

```bash
git add docs/superpowers/plans/2026-09-02-scuttler-flee-to-shadow.md
git commit -m "Record editor prerequisite findings for Scuttler flee behavior"
```

---

## Task 1: C++ module scaffold

**Files:**
- Create: `SurvivalTemplate/Source/SurvivalTemplate.Target.cs`
- Create: `SurvivalTemplate/Source/SurvivalTemplateEditor.Target.cs`
- Create: `SurvivalTemplate/Source/SurvivalTemplate/SurvivalTemplate.Build.cs`
- Create: `SurvivalTemplate/Source/SurvivalTemplate/SurvivalTemplate.h`
- Create: `SurvivalTemplate/Source/SurvivalTemplate/SurvivalTemplate.cpp`
- Modify: `SurvivalTemplate/SurvivalTemplate.uproject`

**Interfaces:**
- Produces: a compiling primary game module named `SurvivalTemplate` with public deps `Core`, `CoreUObject`, `Engine`, `InputCore`, `AIModule`, `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `NavigationSystem`, `StateTreeModule`, `GameplayStateTreeModule`. All later tasks add files to this module.

- [ ] **Step 1: Write `SurvivalTemplate.Target.cs`**

```csharp
using UnrealBuildTool;
using System.Collections.Generic;

public class SurvivalTemplateTarget : TargetRules
{
    public SurvivalTemplateTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("SurvivalTemplate");
    }
}
```

- [ ] **Step 2: Write `SurvivalTemplateEditor.Target.cs`**

Same as Step 1 but class `SurvivalTemplateEditorTarget` and `Type = TargetType.Editor;`.

- [ ] **Step 3: Write `SurvivalTemplate.Build.cs`**

```csharp
using UnrealBuildTool;

public class SurvivalTemplate : ModuleRules
{
    public SurvivalTemplate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "AIModule", "NavigationSystem",
            "GameplayAbilities", "GameplayTags", "GameplayTasks",
            "StateTreeModule", "GameplayStateTreeModule"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });
    }
}
```

- [ ] **Step 4: Write the module header/impl**

`SurvivalTemplate.h`:
```cpp
#pragma once
#include "CoreMinimal.h"
```

`SurvivalTemplate.cpp`:
```cpp
#include "SurvivalTemplate.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, SurvivalTemplate, "SurvivalTemplate");
```

- [ ] **Step 5: Add the module to the `.uproject`**

Edit `SurvivalTemplate.uproject` — add a top-level `"Modules"` array before `"Plugins"`:
```json
"Modules": [
    {
        "Name": "SurvivalTemplate",
        "Type": "Runtime",
        "LoadingPhase": "Default"
    }
],
```

- [ ] **Step 6: Generate project files and build**

From the repo, regenerate and build. Either:
- Right-click `SurvivalTemplate.uproject` → "Generate Visual Studio project files", then in Rider build **Development Editor | Win64**; or
- Bridge: `mcp__rider__build_solution_start` then poll `mcp__rider__build_solution_state`.

Expected: build succeeds, `Binaries/Win64/UnrealEditor-SurvivalTemplate.dll` produced.

- [ ] **Step 7: Commit**

```bash
git add SurvivalTemplate/Source SurvivalTemplate/SurvivalTemplate.uproject
git commit -m "Add SurvivalTemplate C++ runtime module scaffold"
```

---

## Task 2: Native gameplay tags

**Files:**
- Create: `Source/SurvivalTemplate/Public/SurvivalTemplateGameplayTags.h`
- Create: `Source/SurvivalTemplate/Private/SurvivalTemplateGameplayTags.cpp`

**Interfaces:**
- Produces: `namespace STGameplayTags { SURVIVALTEMPLATE_API extern const FNativeGameplayTag Ability_Scuttler_FleeToShadow; SURVIVALTEMPLATE_API extern const FNativeGameplayTag State_Scuttler_Fleeing; }`. Consumed by Tasks 8, 10, 14.

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "NativeGameplayTags.h"

namespace STGameplayTags
{
    SURVIVALTEMPLATE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Scuttler_FleeToShadow);
    SURVIVALTEMPLATE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Scuttler_Fleeing);
}
```

- [ ] **Step 2: Write the impl**

```cpp
#include "SurvivalTemplateGameplayTags.h"

namespace STGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(Ability_Scuttler_FleeToShadow, "Ability.Scuttler.FleeToShadow");
    UE_DEFINE_GAMEPLAY_TAG(State_Scuttler_Fleeing, "State.Scuttler.Fleeing");
}
```

- [ ] **Step 3: Build**

Bridge `build_solution_start` / `build_solution_state`, or Rider build. Expected: success.

- [ ] **Step 4: Verify the tags register**

`ue_execute_python`:
```python
import unreal
m = unreal.GameplayTagLibrary
print(m.request_gameplay_tag("Ability.Scuttler.FleeToShadow", False).is_valid())
print(m.request_gameplay_tag("State.Scuttler.Fleeing", False).is_valid())
```
Expected: `True` / `True` (after a hot-reload or editor restart picks up the new binary).

- [ ] **Step 5: Commit**

```bash
git add Source/SurvivalTemplate/Public/SurvivalTemplateGameplayTags.h Source/SurvivalTemplate/Private/SurvivalTemplateGameplayTags.cpp
git commit -m "Add native gameplay tags for Scuttler flee behavior"
```

---

## Task 3: Light registry (subsystem + component)

**Files:**
- Create: `Public/Light/LightSourceComponent.h`, `Private/Light/LightSourceComponent.cpp`
- Create: `Public/Light/LightRegistrySubsystem.h`, `Private/Light/LightRegistrySubsystem.cpp`
- Test: `Private/Tests/LightRegistrySubsystemTest.cpp`

**Interfaces:**
- Produces:
  - `class SURVIVALTEMPLATE_API ULightSourceComponent : public USceneComponent` with `UPROPERTY(EditAnywhere, BlueprintReadWrite) float Radius = 500.f;`, `UPROPERTY(EditAnywhere) bool bStartsRegistered = true;`, `UFUNCTION(BlueprintCallable) void SetLightActive(bool bNewActive);`, `bool IsLightActive() const;`.
  - `class SURVIVALTEMPLATE_API ULightRegistrySubsystem : public UWorldSubsystem` with:
    - `void RegisterLight(ULightSourceComponent* Light);`
    - `void UnregisterLight(ULightSourceComponent* Light);`
    - `bool IsPointLit(const FVector& Point, const AActor* IgnoreActor) const;`
    - `static ULightRegistrySubsystem* Get(const UObject* WorldContext);`
- Consumed by Task 5 (`IsPointLit`), Task 13 (component on `BP_HorrorLight`).

- [ ] **Step 1: Write the failing test**

`Private/Tests/LightRegistrySubsystemTest.cpp`:
```cpp
#include "Misc/AutomationTest.h"
#include "Light/LightRegistrySubsystem.h"
#include "Light/LightSourceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightRegistrySubsystemTest,
    "SurvivalTemplate.Light.IsPointLit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLightRegistrySubsystemTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull("world", World);

    ULightRegistrySubsystem* Reg = World->GetSubsystem<ULightRegistrySubsystem>();
    TestNotNull("subsystem", Reg);

    AActor* LightActor = World->SpawnActor<AActor>(FVector(0, 0, 200), FRotator::ZeroRotator);
    ULightSourceComponent* Light = NewObject<ULightSourceComponent>(LightActor);
    Light->Radius = 500.f;
    Light->RegisterComponent();
    LightActor->SetRootComponent(Light);
    Light->SetWorldLocation(FVector(0, 0, 200));
    Reg->RegisterLight(Light);

    // Inside radius, clear LOS -> lit
    TestTrue("near+clear is lit", Reg->IsPointLit(FVector(100, 0, 200), nullptr));
    // Outside radius -> not lit
    TestFalse("far is not lit", Reg->IsPointLit(FVector(900, 0, 200), nullptr));

    // Wall between a near point and the light -> not lit
    AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(FVector(150, 0, 200), FRotator::ZeroRotator);
    Wall->SetMobility(EComponentMobility::Movable);
    Wall->GetStaticMeshComponent()->SetStaticMesh(
        LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorScale3D(FVector(1.f, 5.f, 5.f));
    Wall->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestFalse("blocked point is not lit", Reg->IsPointLit(FVector(300, 0, 200), nullptr));

    Reg->UnregisterLight(Light);
    TestFalse("after unregister nothing is lit", Reg->IsPointLit(FVector(100, 0, 200), nullptr));

    return true;
}
```

- [ ] **Step 2: Run the test, verify it fails to compile / fails**

In the running editor console (via `ue_execute_python` → `unreal.SystemLibrary.execute_console_command`) or Session Frontend:
```python
import unreal
unreal.SystemLibrary.execute_console_command(
    unreal.EditorLevelLibrary.get_editor_world(),
    "Automation RunTests SurvivalTemplate.Light.IsPointLit")
```
Expected: build error (`LightRegistrySubsystem.h` not found) — the header doesn't exist yet. This is the red state.

- [ ] **Step 3: Write `LightSourceComponent`**

`LightSourceComponent.h`:
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LightSourceComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALTEMPLATE_API ULightSourceComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    ULightSourceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Occlusion")
    float Radius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Occlusion")
    bool bStartsRegistered = true;

    UFUNCTION(BlueprintCallable, Category="Light Occlusion")
    void SetLightActive(bool bNewActive);

    bool IsLightActive() const { return bActive; }

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

private:
    bool bActive = true;
    bool bRegistered = false;
    void AddToRegistry();
    void RemoveFromRegistry();
};
```

`LightSourceComponent.cpp`:
```cpp
#include "Light/LightSourceComponent.h"
#include "Light/LightRegistrySubsystem.h"

ULightSourceComponent::ULightSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void ULightSourceComponent::OnRegister()
{
    Super::OnRegister();
    UWorld* W = GetWorld();
    if (W && W->IsGameWorld())
    {
        bActive = bStartsRegistered;
        if (bStartsRegistered) { AddToRegistry(); }
    }
}

void ULightSourceComponent::OnUnregister()
{
    RemoveFromRegistry();
    Super::OnUnregister();
}

void ULightSourceComponent::SetLightActive(bool bNewActive)
{
    if (bNewActive == bActive) { return; }
    bActive = bNewActive;
    if (bActive) { AddToRegistry(); } else { RemoveFromRegistry(); }
}

void ULightSourceComponent::AddToRegistry()
{
    if (bRegistered) { return; }
    if (ULightRegistrySubsystem* Reg = ULightRegistrySubsystem::Get(this))
    {
        Reg->RegisterLight(this);
        bRegistered = true;
    }
}

void ULightSourceComponent::RemoveFromRegistry()
{
    if (!bRegistered) { return; }
    if (ULightRegistrySubsystem* Reg = ULightRegistrySubsystem::Get(this))
    {
        Reg->UnregisterLight(this);
    }
    bRegistered = false;
}
```

- [ ] **Step 4: Write `LightRegistrySubsystem`**

`LightRegistrySubsystem.h`:
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LightRegistrySubsystem.generated.h"

class ULightSourceComponent;

UCLASS()
class SURVIVALTEMPLATE_API ULightRegistrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterLight(ULightSourceComponent* Light);
    void UnregisterLight(ULightSourceComponent* Light);

    /** True if Point is within any active light's Radius AND has clear line of sight to that light. */
    bool IsPointLit(const FVector& Point, const AActor* IgnoreActor) const;

    static ULightRegistrySubsystem* Get(const UObject* WorldContext);

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<ULightSourceComponent>> RegisteredLights;
};
```

`LightRegistrySubsystem.cpp`:
```cpp
#include "Light/LightRegistrySubsystem.h"
#include "Light/LightSourceComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

void ULightRegistrySubsystem::RegisterLight(ULightSourceComponent* Light)
{
    if (Light) { RegisteredLights.AddUnique(Light); }
}

void ULightRegistrySubsystem::UnregisterLight(ULightSourceComponent* Light)
{
    RegisteredLights.RemoveAll([Light](const TWeakObjectPtr<ULightSourceComponent>& P)
    {
        return !P.IsValid() || P.Get() == Light;
    });
}

bool ULightRegistrySubsystem::IsPointLit(const FVector& Point, const AActor* IgnoreActor) const
{
    const UWorld* W = GetWorld();
    if (!W) { return false; }

    for (const TWeakObjectPtr<ULightSourceComponent>& WeakLight : RegisteredLights)
    {
        const ULightSourceComponent* Light = WeakLight.Get();
        if (!Light || !Light->IsLightActive()) { continue; }

        const FVector LightLoc = Light->GetComponentLocation();
        if (FVector::DistSquared(Point, LightLoc) > FMath::Square(Light->Radius)) { continue; }

        FCollisionQueryParams Params(SCENE_QUERY_STAT(LightRegistryLOS), false);
        if (IgnoreActor) { Params.AddIgnoredActor(IgnoreActor); }
        if (const AActor* LightOwner = Light->GetOwner()) { Params.AddIgnoredActor(LightOwner); }

        const bool bBlocked = W->LineTraceTestByChannel(Point, LightLoc, ECC_Visibility, Params);
        if (!bBlocked) { return true; } // reached the light -> lit
    }
    return false;
}

ULightRegistrySubsystem* ULightRegistrySubsystem::Get(const UObject* WorldContext)
{
    const UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    return W ? W->GetSubsystem<ULightRegistrySubsystem>() : nullptr;
}
```

- [ ] **Step 5: Build**

Bridge build. Expected: success.

- [ ] **Step 6: Run the test, verify it passes**

```python
import unreal
unreal.SystemLibrary.execute_console_command(
    unreal.EditorLevelLibrary.get_editor_world(),
    "Automation RunTests SurvivalTemplate.Light.IsPointLit")
```
Then `ue_get_logs` filtered to category `LogAutomationController` / pattern `IsPointLit`. Expected: `....Test Completed. Result={Success}` and `1 test(s) ... Passed`.

If the "blocked point" assertion is flaky because the cube mesh/collision didn't load in the test world, replace the wall with a programmatic box: spawn an `AActor`, add a `UBoxComponent` with `SetCollisionEnabled(QueryAndPhysics)` and `SetBoxExtent`, positioned between the point and light. Keep the assertion.

- [ ] **Step 7: Commit**

```bash
git add Source/SurvivalTemplate/Public/Light Source/SurvivalTemplate/Private/Light Source/SurvivalTemplate/Private/Tests/LightRegistrySubsystemTest.cpp
git commit -m "Add light registry subsystem and per-light source component"
```

---

## Task 4: EQS player context

**Files:**
- Create: `Public/EQS/EnvQueryContext_Player.h`, `Private/EQS/EnvQueryContext_Player.cpp`

**Interfaces:**
- Produces: `class SURVIVALTEMPLATE_API UEnvQueryContext_Player : public UEnvQueryContext` — provides the player pawn (player 0) as the single context actor. Consumed by `EQS_FleeToShadow` (Task 11).

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_Player.generated.h"

UCLASS()
class SURVIVALTEMPLATE_API UEnvQueryContext_Player : public UEnvQueryContext
{
    GENERATED_BODY()
public:
    virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "EQS/EnvQueryContext_Player.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "Kismet/GameplayStatics.h"

void UEnvQueryContext_Player::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (!QueryOwner) { return; }

    if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(QueryOwner, 0))
    {
        UEnvQueryItemType_Actor::SetContextHelper(ContextData, PlayerPawn);
    }
}
```

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Smoke-check registration**

```python
import unreal
cls = unreal.load_class(None, "/Script/SurvivalTemplate.EnvQueryContext_Player")
print("context class loaded:", cls is not None)
```
Expected: `True`.

- [ ] **Step 5: Commit**

```bash
git add Source/SurvivalTemplate/Public/EQS/EnvQueryContext_Player.h Source/SurvivalTemplate/Private/EQS/EnvQueryContext_Player.cpp
git commit -m "Add EQS player context"
```

---

## Task 5: EQS direct-light test

**Files:**
- Create: `Public/EQS/EnvQueryTest_DirectLight.h`, `Private/EQS/EnvQueryTest_DirectLight.cpp`

**Interfaces:**
- Consumes: `ULightRegistrySubsystem::IsPointLit` (Task 3).
- Produces: `class SURVIVALTEMPLATE_API UEnvQueryTest_DirectLight : public UEnvQueryTest` with `UPROPERTY(EditDefaultsOnly, Category=Test) FAIDataProviderFloatValue TraceHeightOffset;` (default 60). Filters out / scores 0 for lit points; unlit points score 1. Consumed by `EQS_FleeToShadow` (Task 11).

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvQueryTest_DirectLight.generated.h"

UCLASS(meta=(DisplayName="Direct Light"))
class SURVIVALTEMPLATE_API UEnvQueryTest_DirectLight : public UEnvQueryTest
{
    GENERATED_BODY()
public:
    UEnvQueryTest_DirectLight();

    /** Vertical offset applied to each point before the point->light trace, so the floor doesn't self-block. */
    UPROPERTY(EditDefaultsOnly, Category=Test)
    FAIDataProviderFloatValue TraceHeightOffset;

    virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
    virtual FText GetDescriptionTitle() const override;
    virtual FText GetDescriptionDetails() const override;
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "EQS/EnvQueryTest_DirectLight.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Light/LightRegistrySubsystem.h"
#include "Engine/World.h"

UEnvQueryTest_DirectLight::UEnvQueryTest_DirectLight()
{
    Cost = EEnvTestCost::High;
    SetWorkOnFloatValues(false);
    ValidItemType = UEnvQueryItemType_Point::StaticClass();
    TraceHeightOffset.DefaultValue = 60.f;
    // Default to filter+score, keeping items that are NOT lit.
    TestPurpose = EEnvTestPurpose::FilterAndScore;
    FilterType = EEnvTestFilterType::Match;
    BoolValue.DefaultValue = false; // desired "bIsLit" value
}

void UEnvQueryTest_DirectLight::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* Owner = QueryInstance.Owner.Get();
    if (!Owner) { return; }

    UWorld* World = QueryInstance.World;
    ULightRegistrySubsystem* Reg = World ? World->GetSubsystem<ULightRegistrySubsystem>() : nullptr;

    BoolValue.BindData(Owner, QueryInstance.QueryID);
    const bool bWantsLit = BoolValue.GetValue();

    TraceHeightOffset.BindData(Owner, QueryInstance.QueryID);
    const float ZOffset = TraceHeightOffset.GetValue();

    const AActor* IgnoreActor = Cast<AActor>(Owner);

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector Point = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0, 0, ZOffset);
        const bool bIsLit = Reg ? Reg->IsPointLit(Point, IgnoreActor) : false;
        It.SetScore(TestPurpose, FilterType, bIsLit, bWantsLit);
    }

    if (!Reg)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnvQueryTest_DirectLight: no ULightRegistrySubsystem; treating all points as unlit."));
    }
}

FText UEnvQueryTest_DirectLight::GetDescriptionTitle() const
{
    return NSLOCTEXT("SurvivalTemplate", "DirectLightTitle", "Direct Light");
}

FText UEnvQueryTest_DirectLight::GetDescriptionDetails() const
{
    return NSLOCTEXT("SurvivalTemplate", "DirectLightDetails", "Filter/score points by whether a registered light directly illuminates them");
}
```

Note: verify `FEnvQueryInstance::ItemIterator`, `GetItemLocation`, and `It.SetScore(...)` signatures against `EnvironmentQuery/EnvQueryTest.h` in the 5.8 install — the `SetScore` overload that takes `(EEnvTestPurpose, EEnvTestFilterType, bool, bool)` is the bool path. If `GetItemLocation` is protected and takes only the instance+index, that matches; otherwise use `GetItemLocation(QueryInstance, It.GetIndex())` as written.

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Smoke-check the test appears in EQS**

```python
import unreal
cls = unreal.load_class(None, "/Script/SurvivalTemplate.EnvQueryTest_DirectLight")
print("test class loaded:", cls is not None)
```
Expected: `True`. Full behavioral verification happens in Task 16.

- [ ] **Step 5: Commit**

```bash
git add Source/SurvivalTemplate/Public/EQS/EnvQueryTest_DirectLight.h Source/SurvivalTemplate/Private/EQS/EnvQueryTest_DirectLight.cpp
git commit -m "Add Direct Light EQS test"
```

---

## Task 6: Scuttler C++ base with ASC

**Files:**
- Create: `Public/AI/ScuttlerCharacter.h`, `Private/AI/ScuttlerCharacter.cpp`

**Interfaces:**
- Consumes: Task 0 finding (current `BP_Scuttler` parent, perception owner).
- Produces:
  - `class SURVIVALTEMPLATE_API AScuttlerCharacter : public ACharacter, public IAbilitySystemInterface`
  - `virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;`
  - `UPROPERTY(EditDefaultsOnly, Category="Abilities") TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;`
  - `UFUNCTION(BlueprintPure, Category="AI") bool CanSeePlayer() const;` and `UPROPERTY(BlueprintReadOnly) bool bCanSeePlayer;` — the perception signal (populated in Task 9).
  - protected `UPROPERTY() TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;`
- Consumed by Task 8 (avatar cast), Task 9 (adds perception delegate), Task 10 (`GetAbilitySystemComponent`), Task 12 (reparent target).

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ScuttlerCharacter.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

UCLASS()
class SURVIVALTEMPLATE_API AScuttlerCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AScuttlerCharacter();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
    virtual void PossessedBy(AController* NewController) override;

    UFUNCTION(BlueprintPure, Category="AI")
    bool CanSeePlayer() const { return bCanSeePlayer; }

protected:
    UPROPERTY(VisibleAnywhere, Category="Abilities")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(EditDefaultsOnly, Category="Abilities")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    bool bCanSeePlayer = false;

    void GrantDefaultAbilities();

private:
    bool bAbilitiesGranted = false;
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "AI/ScuttlerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

AScuttlerCharacter::AScuttlerCharacter()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AScuttlerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
        GrantDefaultAbilities();
    }
}

void AScuttlerCharacter::GrantDefaultAbilities()
{
    if (bAbilitiesGranted || !HasAuthority() || !AbilitySystemComponent) { return; }
    for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
    {
        if (Ability)
        {
            AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
        }
    }
    bAbilitiesGranted = true;
}
```

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Commit**

```bash
git add Source/SurvivalTemplate/Public/AI/ScuttlerCharacter.h Source/SurvivalTemplate/Private/AI/ScuttlerCharacter.cpp
git commit -m "Add AScuttlerCharacter base with ability system component"
```

---

## Task 7: MoveTo ability task

**Files:**
- Create: `Public/AI/AbilityTask_MoveTo.h`, `Private/AI/AbilityTask_MoveTo.cpp`

**Interfaces:**
- Produces:
  - `class SURVIVALTEMPLATE_API UAbilityTask_MoveTo : public UAbilityTask`
  - `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveToFinishedDelegate, bool, bSuccess);`
  - `UPROPERTY(BlueprintAssignable) FMoveToFinishedDelegate OnFinished;`
  - `static UAbilityTask_MoveTo* MoveTo(UGameplayAbility* OwningAbility, FVector Destination, float AcceptanceRadius);`
- Consumed by Task 8.

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Navigation/PathFollowingComponent.h"
#include "AbilityTask_MoveTo.generated.h"

class AAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveToFinishedDelegate, bool, bSuccess);

UCLASS()
class SURVIVALTEMPLATE_API UAbilityTask_MoveTo : public UAbilityTask
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable)
    FMoveToFinishedDelegate OnFinished;

    UFUNCTION(BlueprintCallable, Category="Ability|Tasks",
        meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="true"))
    static UAbilityTask_MoveTo* MoveTo(UGameplayAbility* OwningAbility, FVector Destination, float AcceptanceRadius);

    virtual void Activate() override;

protected:
    virtual void OnDestroy(bool bInOwnerFinished) override;

private:
    FVector Destination = FVector::ZeroVector;
    float AcceptanceRadius = 60.f;
    FAIRequestID MoveRequestID;
    TWeakObjectPtr<AAIController> CachedAI;

    UFUNCTION()
    void HandleMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    void Finish(bool bSuccess);
    AAIController* ResolveAIController() const;
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "AI/AbilityTask_MoveTo.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

UAbilityTask_MoveTo* UAbilityTask_MoveTo::MoveTo(UGameplayAbility* OwningAbility, FVector InDestination, float InAcceptanceRadius)
{
    UAbilityTask_MoveTo* Task = NewAbilityTask<UAbilityTask_MoveTo>(OwningAbility);
    Task->Destination = InDestination;
    Task->AcceptanceRadius = InAcceptanceRadius;
    return Task;
}

AAIController* UAbilityTask_MoveTo::ResolveAIController() const
{
    const FGameplayAbilityActorInfo* Info = Ability ? Ability->GetCurrentActorInfo() : nullptr;
    if (!Info) { return nullptr; }
    if (APawn* Pawn = Cast<APawn>(Info->AvatarActor.Get()))
    {
        return Cast<AAIController>(Pawn->GetController());
    }
    return nullptr;
}

void UAbilityTask_MoveTo::Activate()
{
    AAIController* AI = ResolveAIController();
    if (!AI)
    {
        Finish(false);
        return;
    }
    CachedAI = AI;

    FAIMoveRequest Req(Destination);
    Req.SetAcceptanceRadius(AcceptanceRadius);
    Req.SetUsePathfinding(true);
    Req.SetAllowPartialPath(true);

    FPathFollowingRequestResult Result = AI->MoveTo(Req);
    switch (Result.Code)
    {
    case EPathFollowingRequestResult::AlreadyAtGoal:
        Finish(true);
        break;
    case EPathFollowingRequestResult::Failed:
        Finish(false);
        break;
    case EPathFollowingRequestResult::RequestSuccessful:
        MoveRequestID = Result.MoveId;
        AI->ReceiveMoveCompleted.AddDynamic(this, &UAbilityTask_MoveTo::HandleMoveCompleted);
        break;
    }
}

void UAbilityTask_MoveTo::HandleMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    if (RequestID != MoveRequestID) { return; }
    Finish(Result == EPathFollowingResult::Success);
}

void UAbilityTask_MoveTo::Finish(bool bSuccess)
{
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        OnFinished.Broadcast(bSuccess);
    }
    EndTask();
}

void UAbilityTask_MoveTo::OnDestroy(bool bInOwnerFinished)
{
    if (AAIController* AI = CachedAI.Get())
    {
        AI->ReceiveMoveCompleted.RemoveDynamic(this, &UAbilityTask_MoveTo::HandleMoveCompleted);
        if (MoveRequestID.IsValid())
        {
            AI->StopMovement();
        }
    }
    Super::OnDestroy(bInOwnerFinished);
}
```

- [ ] **Step 3: Build.** Bridge build; expected success. Confirm `AAIController::ReceiveMoveCompleted` is a `DYNAMIC` multicast (bindable with `AddDynamic`) in the 5.8 header `AIController.h`; if it is a non-dynamic `FAIMoveCompletedSignature`, bind with `FScriptDelegate` / a lambda via `AI->GetPathFollowingComponent()->OnRequestFinished` instead and keep the same `Finish` semantics.

- [ ] **Step 4: Commit**

```bash
git add Source/SurvivalTemplate/Public/AI/AbilityTask_MoveTo.h Source/SurvivalTemplate/Private/AI/AbilityTask_MoveTo.cpp
git commit -m "Add MoveTo ability task"
```

---

## Task 8: FleeToShadow gameplay ability

**Files:**
- Create: `Public/AI/GA_FleeToShadow.h`, `Private/AI/GA_FleeToShadow.cpp`

**Interfaces:**
- Consumes: `STGameplayTags::Ability_Scuttler_FleeToShadow` (Task 2), `UAbilityTask_MoveTo` (Task 7).
- Produces:
  - `class SURVIVALTEMPLATE_API UGA_FleeToShadow : public UGameplayAbility`
  - `UPROPERTY(EditDefaultsOnly, Category="Flee") TObjectPtr<UEnvQuery> FleeQuery;`
  - `UPROPERTY(EditDefaultsOnly, Category="Flee") float AcceptanceRadius = 60.f;`
- Consumed by Task 12 (BP subclass sets `FleeQuery`).

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GA_FleeToShadow.generated.h"

class UEnvQuery;
struct FEnvQueryResult;

UCLASS()
class SURVIVALTEMPLATE_API UGA_FleeToShadow : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_FleeToShadow();

    UPROPERTY(EditDefaultsOnly, Category="Flee")
    TObjectPtr<UEnvQuery> FleeQuery;

    UPROPERTY(EditDefaultsOnly, Category="Flee")
    float AcceptanceRadius = 60.f;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

private:
    void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);

    UFUNCTION()
    void OnMoveFinished(bool bSuccess);
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "AI/GA_FleeToShadow.h"
#include "AI/AbilityTask_MoveTo.h"
#include "SurvivalTemplateGameplayTags.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "AbilitySystemComponent.h"

UGA_FleeToShadow::UGA_FleeToShadow()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    FGameplayTagContainer Tags;
    Tags.AddTag(STGameplayTags::Ability_Scuttler_FleeToShadow);
    SetAssetTags(Tags); // UE 5.5+: replaces the deprecated AbilityTags member
}

void UGA_FleeToShadow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    if (!Avatar || !FleeQuery)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FEnvQueryRequest Request(FleeQuery, Avatar);
    Request.Execute(EEnvQueryRunMode::SingleResult, this, &UGA_FleeToShadow::OnQueryFinished);
}

void UGA_FleeToShadow::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
    const bool bOk = Result.IsValid() && Result->IsSuccessful() && Result->Items.Num() > 0;
    if (!bOk)
    {
        UE_LOG(LogTemp, Verbose, TEXT("GA_FleeToShadow: no viable hiding spot; ending."));
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }

    const FVector Dest = Result->GetItemAsLocation(0);
    UAbilityTask_MoveTo* MoveTask = UAbilityTask_MoveTo::MoveTo(this, Dest, AcceptanceRadius);
    MoveTask->OnFinished.AddDynamic(this, &UGA_FleeToShadow::OnMoveFinished);
    MoveTask->ReadyForActivation();
}

void UGA_FleeToShadow::OnMoveFinished(bool /*bSuccess*/)
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

Verify `FEnvQueryRequest::Execute(EEnvQueryRunMode::Type, UObject*, TMethodPtr)` exists in `EnvQueryManager.h` for 5.8 (it has historically). If only the `FQueryFinishedSignature` delegate overload exists, build the delegate with `FQueryFinishedSignature::CreateUObject(this, &UGA_FleeToShadow::OnQueryFinished)` and pass it.

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Commit**

```bash
git add Source/SurvivalTemplate/Public/AI/GA_FleeToShadow.h Source/SurvivalTemplate/Private/AI/GA_FleeToShadow.cpp
git commit -m "Add FleeToShadow gameplay ability"
```

---

## Task 9: Perception signal + StateTree condition

**Files:**
- Modify: `Public/AI/ScuttlerCharacter.h`, `Private/AI/ScuttlerCharacter.cpp`
- Create: `Public/AI/StateTreeCondition_CanSeePlayer.h`, `Private/AI/StateTreeCondition_CanSeePlayer.cpp`

**Interfaces:**
- Consumes: Task 0 finding (perception component owner), `AScuttlerCharacter` (Task 6).
- Produces:
  - `AScuttlerCharacter` now binds `UAIPerceptionComponent::OnTargetPerceptionUpdated` and maintains `bCanSeePlayer` + `UPROPERTY() TObjectPtr<AActor> SeenPlayer;` + `float LastSeenTime;`.
  - `USTRUCT() struct FStateTreeCondition_CanSeePlayerInstanceData { UPROPERTY(EditAnywhere) bool bInvert = false; };`
  - `USTRUCT(meta=(DisplayName="Scuttler Can See Player")) struct SURVIVALTEMPLATE_API FStateTreeCondition_CanSeePlayer : public FStateTreeConditionCommonBase` with `using FInstanceDataType = FStateTreeCondition_CanSeePlayerInstanceData;` and `virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;`.
- Consumed by Task 14.

- [ ] **Step 1: Add perception handling to `AScuttlerCharacter`**

Header — add:
```cpp
public:
    virtual void BeginPlay() override;
protected:
    UPROPERTY() TObjectPtr<AActor> SeenPlayer = nullptr;
    float LastSeenTime = 0.f;

    UFUNCTION()
    void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
```
Add includes: `#include "Perception/AIPerceptionTypes.h"`.

Impl — add:
```cpp
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"

void AScuttlerCharacter::BeginPlay()
{
    Super::BeginPlay();
    // Task 0 finding: perception component lives on <this pawn | AI controller>.
    // If on the pawn:
    if (UAIPerceptionComponent* Perc = FindComponentByClass<UAIPerceptionComponent>())
    {
        Perc->OnTargetPerceptionUpdated.AddDynamic(this, &AScuttlerCharacter::HandleTargetPerceptionUpdated);
    }
    // If Task 0 found it on AI_Scuttler instead, bind there in the controller BP/C++
    // and have it call a setter on this pawn; note the deviation in the commit.
}

void AScuttlerCharacter::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (Actor != PlayerPawn) { return; }
    if (Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>()) { return; }

    bCanSeePlayer = Stimulus.WasSuccessfullySensed();
    if (bCanSeePlayer)
    {
        SeenPlayer = Actor;
        LastSeenTime = GetWorld()->GetTimeSeconds();
    }
}
```

- [ ] **Step 2: Write the StateTree condition**

`StateTreeCondition_CanSeePlayer.h`:
```cpp
#pragma once
#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeCondition_CanSeePlayer.generated.h"

USTRUCT()
struct FStateTreeCondition_CanSeePlayerInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category=Parameter)
    bool bInvert = false;
};

USTRUCT(meta=(DisplayName="Scuttler Can See Player", Category="Scuttler"))
struct SURVIVALTEMPLATE_API FStateTreeCondition_CanSeePlayer : public FStateTreeConditionCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FStateTreeCondition_CanSeePlayerInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
```

`StateTreeCondition_CanSeePlayer.cpp`:
```cpp
#include "AI/StateTreeCondition_CanSeePlayer.h"
#include "AI/ScuttlerCharacter.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

bool FStateTreeCondition_CanSeePlayer::TestCondition(FStateTreeExecutionContext& Context) const
{
    const FInstanceDataType& Data = Context.GetInstanceData(*this);

    AScuttlerCharacter* Scuttler = Cast<AScuttlerCharacter>(Context.GetOwner());
    if (!Scuttler)
    {
        if (AAIController* AI = Cast<AAIController>(Context.GetOwner()))
        {
            Scuttler = Cast<AScuttlerCharacter>(AI->GetPawn());
        }
    }
    const bool bSees = Scuttler && Scuttler->CanSeePlayer();
    return Data.bInvert ? !bSees : bSees;
}
```

Verify against the 5.8 StateTree headers: base struct name (`FStateTreeConditionCommonBase` vs `FStateTreeConditionBase`), whether `GetInstanceDataType()` is still required or replaced by the `FInstanceDataType` typedef + `IMPLEMENT` macro, and `Context.GetOwner()` (may be `GetOwner()` returning `UObject*`, needing `Cast<AActor>` first). Adjust to match; keep the "resolve `AScuttlerCharacter` from owner-or-its-pawn, return `CanSeePlayer()`" logic.

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Verify the condition appears in StateTree**

Restart/hot-reload the editor. In `ue_execute_python`:
```python
import unreal
s = unreal.load_struct(None, "/Script/SurvivalTemplate.StateTreeCondition_CanSeePlayer")
print("condition struct:", s is not None)
```
Expected: `True`.

- [ ] **Step 5: Commit**

```bash
git add Source/SurvivalTemplate/Public/AI Source/SurvivalTemplate/Private/AI
git commit -m "Add Scuttler perception signal and StateTree can-see-player condition"
```

---

## Task 10: Activate-ability StateTree task

**Files:**
- Create: `Public/AI/STTask_ActivateAbilityByTag.h`, `Private/AI/STTask_ActivateAbilityByTag.cpp`

**Interfaces:**
- Consumes: `IAbilitySystemInterface` / `UAbilitySystemComponent` on the owner (Task 6), `STGameplayTags` (Task 2).
- Produces:
  - `USTRUCT() struct FSTTask_ActivateAbilityByTagInstanceData { UPROPERTY(EditAnywhere) FGameplayTag AbilityTag; UPROPERTY(EditAnywhere) bool bEndAbilityOnExit = true; bool bAbilityEnded = false; FDelegateHandle EndedHandle; TWeakObjectPtr<UAbilitySystemComponent> ASC; };`
  - `USTRUCT(meta=(DisplayName="Activate Ability By Tag")) struct SURVIVALTEMPLATE_API FSTTask_ActivateAbilityByTag : public FStateTreeTaskCommonBase` with `EnterState`, `Tick`, `ExitState` overrides.
- Consumed by Task 14.

- [ ] **Step 1: Write the header**

```cpp
#pragma once
#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "GameplayTagContainer.h"
#include "STTask_ActivateAbilityByTag.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FSTTask_ActivateAbilityByTagInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category=Parameter)
    FGameplayTag AbilityTag;

    UPROPERTY(EditAnywhere, Category=Parameter)
    bool bEndAbilityOnExit = true;

    bool bAbilityEnded = false;
    TWeakObjectPtr<UAbilitySystemComponent> ASC;
    FDelegateHandle EndedHandle;
};

USTRUCT(meta=(DisplayName="Activate Ability By Tag", Category="Scuttler"))
struct SURVIVALTEMPLATE_API FSTTask_ActivateAbilityByTag : public FStateTreeTaskCommonBase
{
    GENERATED_BODY()

    using FInstanceDataType = FSTTask_ActivateAbilityByTagInstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
```

- [ ] **Step 2: Write the impl**

```cpp
#include "AI/STTask_ActivateAbilityByTag.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

static UAbilitySystemComponent* ResolveASC(UObject* Owner)
{
    AActor* AsActor = Cast<AActor>(Owner);
    if (!AsActor)
    {
        return nullptr;
    }
    if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AsActor))
    {
        return ASC;
    }
    if (AAIController* AI = Cast<AAIController>(AsActor))
    {
        return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AI->GetPawn());
    }
    return nullptr;
}

EStateTreeRunStatus FSTTask_ActivateAbilityByTag::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    Data.bAbilityEnded = false;

    UAbilitySystemComponent* ASC = ResolveASC(Context.GetOwner());
    if (!ASC || !Data.AbilityTag.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }
    Data.ASC = ASC;

    const bool bActivated = ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Data.AbilityTag));
    if (!bActivated)
    {
        return EStateTreeRunStatus::Failed;
    }

    Data.EndedHandle = ASC->OnAbilityEnded.AddLambda(
        [&Data](const FAbilityEndedData& EndData)
        {
            if (EndData.AbilityThatEnded &&
                EndData.AbilityThatEnded->GetAssetTags().HasTag(Data.AbilityTag))
            {
                Data.bAbilityEnded = true;
            }
        });

    return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_ActivateAbilityByTag::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    return Data.bAbilityEnded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

void FSTTask_ActivateAbilityByTag::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& Data = Context.GetInstanceData(*this);
    if (UAbilitySystemComponent* ASC = Data.ASC.Get())
    {
        if (Data.EndedHandle.IsValid())
        {
            ASC->OnAbilityEnded.Remove(Data.EndedHandle);
            Data.EndedHandle.Reset();
        }
        if (Data.bEndAbilityOnExit)
        {
            FGameplayTagContainer CancelTags(Data.AbilityTag);
            ASC->CancelAbilities(&CancelTags);
        }
    }
    Data.ASC.Reset();
}
```

Verify: capturing `Data` by reference in the lambda is safe only if the instance data outlives the delegate binding — it does, because `ExitState` removes the handle. If the 5.8 StateTree recreates instance data between ticks, store `bAbilityEnded` on the ASC via a `UPROPERTY` proxy instead; simplest robust alternative is to poll in `Tick`: `return ASC->GetAnimatingAbility()`… no — poll `!ASC->FindAbilitySpecFromClass(...)` active state, or check `ASC->GetActivatableAbilities()` for an active spec whose ability has the tag. Pick whichever matches the engine's instance-data lifetime; keep "Succeeded when the tagged ability is no longer active."

- [ ] **Step 3: Build.** Bridge build; expected success.

- [ ] **Step 4: Verify the task appears in StateTree**

```python
import unreal
s = unreal.load_struct(None, "/Script/SurvivalTemplate.STTask_ActivateAbilityByTag")
print("task struct:", s is not None)
```
Expected: `True`.

- [ ] **Step 5: Commit**

```bash
git add Source/SurvivalTemplate/Public/AI/STTask_ActivateAbilityByTag.h Source/SurvivalTemplate/Private/AI/STTask_ActivateAbilityByTag.cpp
git commit -m "Add activate-ability-by-tag StateTree task"
```

---

## Task 11: EQS query asset

**Files:**
- Create (editor): `/Game/Variant_Horror/AI/EQS_FleeToShadow.uasset`

**Interfaces:**
- Consumes: `UEnvQueryContext_Player` (Task 4), `UEnvQueryTest_DirectLight` (Task 5).
- Produces: an `UEnvQuery` asset referenced by `GA_Scuttler_FleeToShadow` (Task 12).

- [ ] **Step 1: Create the asset**

Preferred: create by hand in the editor (EQS graph editing via Python is unreliable). Via the bridge, create the asset shell then instruct the user, or drive it with `ue_execute_python` using `unreal.AssetToolsHelpers.get_asset_tools().create_asset("EQS_FleeToShadow", "/Game/Variant_Horror/AI", unreal.EnvQuery, unreal.EnvQueryFactory())`. Graph nodes below are set in the EQS editor.

- [ ] **Step 2: Configure the generator**

- Generator: **Points: Donut**
  - Center: `EnvQueryContext_Querier`
  - Inner Radius: `300`
  - Outer Radius: `1200`
  - Number of Rings: `4`
  - Points Per Ring: `8`
  - Arc Direction / Arc Angle: leave full (360) for now; Task 16 may narrow to an arc away from the player.
  - Project points to navigation: **on**.

- [ ] **Step 3: Add the tests (on the generator)**

1. **Distance** — Test to: `EnvQueryContext_Player`. Scoring: `Absolute`. Scoring Factor: `+1.0`. Scoring Equation: `Linear`. (Higher distance from player scores higher.)
2. **Trace** — Context: `EnvQueryContext_Player`, `TraceFromContext = true`, `TraceChannel = Visibility`, `ItemHeightOffset ≈ 60`, `ContextHeightOffset ≈ 60`. Purpose: **Filter Only**. Filter: `Match = false` (keep items with NO clear trace to the player — i.e. line of sight is broken).
3. **Pathfinding** — Context: `EnvQueryContext_Querier`, `TestMode = PathExist`. Purpose: **Filter Only**. Filter: `Match = true` (keep reachable).
4. **Distance** — Test to: `EnvQueryContext_Querier`. Purpose: **Score Only**. Scoring Factor: `-0.3` (nearer cover slightly preferred; tie-breaker).
5. **Direct Light** (`UEnvQueryTest_DirectLight`) — Purpose: **Filter Only** (or Filter and Score). `Bool Value / bIsLit = false` (keep unlit). `TraceHeightOffset ≈ 60`.

- [ ] **Step 4: Query settings**

- Run Mode: **Single Best Item**.
- Save. Compile the EQS asset (the editor does this on save).

- [ ] **Step 5: Sanity-check with the EQS testing pawn (optional, in-editor)**

Drop an `EQSTestingPawn` in `Lvl_Horror`, assign `EQS_FleeToShadow`, and confirm it generates points and the tests don't error. Full behavior verified in Task 16.

- [ ] **Step 6: Commit**

```bash
git add SurvivalTemplate/Content/Variant_Horror/AI/EQS_FleeToShadow.uasset
git commit -m "Add EQS_FleeToShadow query"
```

---

## Task 12: Reparent BP_Scuttler, add ability BP subclass

**Files:**
- Create (editor): `/Game/Variant_Horror/AI/GA_Scuttler_FleeToShadow.uasset`
- Modify (editor): `/Game/Variant_Horror/Blueprints/BP_Scuttler.uasset`

**Interfaces:**
- Consumes: `AScuttlerCharacter` (Task 6), `UGA_FleeToShadow` (Task 8), `EQS_FleeToShadow` (Task 11), Task 0 finding (current parent + any logic to preserve).
- Produces: `BP_Scuttler` reparented to `AScuttlerCharacter` with `DefaultAbilities = [GA_Scuttler_FleeToShadow]`.

- [ ] **Step 1: Create `GA_Scuttler_FleeToShadow`**

```python
import unreal
factory = unreal.BlueprintFactory()
factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/SurvivalTemplate.GA_FleeToShadow"))
at = unreal.AssetToolsHelpers.get_asset_tools()
bp = at.create_asset("GA_Scuttler_FleeToShadow", "/Game/Variant_Horror/AI", unreal.Blueprint, factory)
cdo = unreal.get_default_object(bp.generated_class())
eqs = unreal.load_asset("/Game/Variant_Horror/AI/EQS_FleeToShadow")
cdo.set_editor_property("FleeQuery", eqs)
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
```
Verify `FleeQuery` shows the asset in the ability's Class Defaults.

- [ ] **Step 2: Reparent `BP_Scuttler`**

Preferred in-editor: File → Reparent Blueprint → `AScuttlerCharacter`. Via bridge:
```python
import unreal
bp = unreal.load_asset("/Game/Variant_Horror/Blueprints/BP_Scuttler")
unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.load_class(None, "/Script/SurvivalTemplate.AScuttlerCharacter"))
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
```
If Task 0 found the old parent carried event-graph logic (movement, mesh setup), that logic stays in `BP_Scuttler`'s graph and still runs — only the native base changes. Recompile the Blueprint; fix any nodes that referenced removed parent functions (note them in the commit).

- [ ] **Step 3: Set `DefaultAbilities`**

```python
import unreal
bp = unreal.load_asset("/Game/Variant_Horror/Blueprints/BP_Scuttler")
cdo = unreal.get_default_object(bp.generated_class())
ga = unreal.load_class(None, "/Game/Variant_Horror/AI/GA_Scuttler_FleeToShadow.GA_Scuttler_FleeToShadow_C")
cdo.set_editor_property("DefaultAbilities", [ga])
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
```

- [ ] **Step 4: Verify in PIE that the ability is granted**

`ue_play`, then:
```python
import unreal
w = unreal.EditorLevelLibrary.get_editor_world()  # or PIE world
scuttlers = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.load_class(None, "/Script/SurvivalTemplate.ScuttlerCharacter"))
print("scuttlers:", len(scuttlers))
```
Then `ue_get_logs` — no ASC/ability warnings on possession. Stop PIE.

- [ ] **Step 5: Commit**

```bash
git add SurvivalTemplate/Content/Variant_Horror/AI/GA_Scuttler_FleeToShadow.uasset SurvivalTemplate/Content/Variant_Horror/Blueprints/BP_Scuttler.uasset
git commit -m "Reparent BP_Scuttler to AScuttlerCharacter and grant flee ability"
```

---

## Task 13: Add light source component to BP_HorrorLight

**Files:**
- Modify (editor): `/Game/Variant_Horror/Blueprints/Light/BP_HorrorLight.uasset`

**Interfaces:**
- Consumes: `ULightSourceComponent` (Task 3), Task 0 finding (attenuation radius).
- Produces: every `BP_HorrorLight` instance registers itself with `ULightRegistrySubsystem` at runtime.

- [ ] **Step 1: Add the component**

In-editor: open `BP_HorrorLight`, Add Component → `Light Source`, attach it under the light component (or root) so its world location sits at the bulb. Via bridge, use `unreal.SubobjectDataSubsystem` to add `ULightSourceComponent` — if that API is awkward from Python, do this step by hand in the editor.

- [ ] **Step 2: Set the radius**

Set `Radius` on the component to the `AttenuationRadius` recorded in Task 0 (fall back to `500` if none). Leave `bStartsRegistered = true`.

- [ ] **Step 3: Verify registration in PIE**

`ue_play`, then check the count of registered lights via a temporary console command or by adding a `LogTemp` line — or simply run the Task 16 scenario and confirm the `DirectLight` test culls lit points. Minimal check here:
```python
import unreal
w = unreal.EditorLevelLibrary.get_editor_world()
lights = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.load_class(None, "/Game/Variant_Horror/Blueprints/Light/BP_HorrorLight.BP_HorrorLight_C"))
print("horror lights in level:", len(lights))
```
Stop PIE.

- [ ] **Step 4: Commit**

```bash
git add SurvivalTemplate/Content/Variant_Horror/Blueprints/Light/BP_HorrorLight.uasset
git commit -m "Add light source component to BP_HorrorLight"
```

---

## Task 14: Wire ST_Scuttler

**Files:**
- Modify (editor): `/Game/Variant_Horror/Blueprints/ST_Scuttler.uasset`

**Interfaces:**
- Consumes: `FStateTreeCondition_CanSeePlayer` (Task 9), `FSTTask_ActivateAbilityByTag` (Task 10), `STGameplayTags::Ability_Scuttler_FleeToShadow` (Task 2), Task 0 finding (existing state names, how the tree is run).
- Produces: `ST_Scuttler` enters a `Fleeing` state on sight and returns to normal behavior when the flee ability ends.

- [ ] **Step 1: Add the `Fleeing` state**

Open `ST_Scuttler`. Add a state named `Fleeing` as a child of the root selector, ordered **above** the existing idle/patrol state(s) so it wins selection.

- [ ] **Step 2: Enter condition**

On `Fleeing`, add an **Enter Condition**: `Scuttler Can See Player` (`bInvert = false`).
(If the tree uses a single parent selector with priority order, this is enough — the tree re-selects `Fleeing` whenever the condition is true.)

- [ ] **Step 3: Task**

On `Fleeing`, add task `Activate Ability By Tag`:
- `AbilityTag` = `Ability.Scuttler.FleeToShadow`
- `bEndAbilityOnExit` = `true`

- [ ] **Step 4: Transitions out**

- Add a transition: **On State Completed** (task returned `Succeeded` or `Failed`) → go to the normal state selector / `Tree Succeeded` back to root selection.
- Because the enter condition is re-evaluated on selection, if the player is still visible the tree re-enters `Fleeing` → the ability re-runs the EQS query → the Scuttler moves again. If not visible, it falls through to idle/patrol.

- [ ] **Step 5: (Optional) Fleeing actor tag**

Add a parallel task or state-enter/exit hooks applying/removing the `State.Scuttler.Fleeing` gameplay tag on the owner via the ASC, for other systems to observe. Skip if no consumer exists yet (YAGNI).

- [ ] **Step 6: Compile & save**

Compile `ST_Scuttler`; resolve any binding errors. Save.

- [ ] **Step 7: Commit**

```bash
git add SurvivalTemplate/Content/Variant_Horror/Blueprints/ST_Scuttler.uasset
git commit -m "Add Fleeing state to ST_Scuttler"
```

---

## Task 15: Ensure Lvl_Horror has a navmesh

**Files:**
- Modify (editor): `/Game/Variant_Horror/Lvl_Horror.umap` (+ its OFPA external actors)

**Interfaces:**
- Consumes: Task 0 finding (nav bounds present or not).
- Produces: a built navmesh covering the Scuttler's play area so `MoveTo` paths.

- [ ] **Step 1: Skip if already present**

If Task 0 found ≥1 `NavMeshBoundsVolume` and a `RecastNavMesh`, do nothing — go to Step 4.

- [ ] **Step 2: Add nav bounds**

```python
import unreal
w = unreal.EditorLevelLibrary.get_editor_world()
vol = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0,0,0))
vol.set_actor_scale3d(unreal.Vector(50, 50, 20))  # ~ covers a large room; adjust to level bounds
```
Position/scale the volume to enclose the walkable area (check the level's bounds in the editor and adjust).

- [ ] **Step 3: Build paths**

```python
import unreal
unreal.EditorLevelLibrary.editor_build_paths(unreal.EditorLevelLibrary.get_editor_world())
```
Or in-editor: Build → Build Paths. Confirm a green navmesh renders with the `P` show flag.

- [ ] **Step 4: Save the level**

```python
import unreal
unreal.EditorLevelLibrary.save_current_level()
```

- [ ] **Step 5: Commit**

```bash
git add SurvivalTemplate/Content/Variant_Horror/Lvl_Horror.umap SurvivalTemplate/Content/__ExternalActors__/Variant_Horror SurvivalTemplate/Content/__ExternalObjects__/Variant_Horror
git commit -m "Add navmesh bounds to Lvl_Horror"
```

If Task 0 showed the navmesh already exists, skip this task entirely (no commit).

---

## Task 16: Integration test & tuning

**Files:**
- Create (editor, optional): `/Game/Variant_Horror/AI/Tests/FT_ScuttlerFleeToShadow` (functional test map) — or run the scenario in `Lvl_Horror` via the bridge.
- Possible tuning edits: `EQS_FleeToShadow`, `GA_Scuttler_FleeToShadow` defaults, `ULightSourceComponent.Radius` values.

**Interfaces:**
- Consumes: everything above.

- [ ] **Step 1: Add a decision-trace log category (small code change)**

In `GA_FleeToShadow.cpp`, add `DEFINE_LOG_CATEGORY_STATIC(LogScuttler, Log, All);` and log at: activate (avatar + query name), query finished (item count, chosen location), move finished (success). Rebuild. Commit separately:
```bash
git add Source/SurvivalTemplate/Private/AI/GA_FleeToShadow.cpp
git commit -m "Add LogScuttler decision trace to flee ability"
```

- [ ] **Step 2: Set up the scenario**

In `Lvl_Horror` (via the bridge): ensure there is a `BP_Scuttler`, at least one `BP_HorrorLight` casting on an open area, and some cover geometry. Note the Scuttler's start location and the light positions.

- [ ] **Step 3: Run and observe**

```python
import unreal
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(), "show Navigation")
```
`ue_play`. Walk the player pawn into the Scuttler's sight cone (drive input via `ue_execute_python` setting the pawn location in front of the Scuttler, or play manually). Then `ue_get_logs` filtered to category `LogScuttler`.

Expected sequence in the log:
- `LogScuttler: FleeToShadow activated (avatar=BP_Scuttler_C_0, query=EQS_FleeToShadow)`
- `LogScuttler: query finished, N items, chosen=(X,Y,Z)`
- Scuttler visibly paths to that location
- `LogScuttler: move finished (success)`

- [ ] **Step 4: Verify the acceptance criteria**

At the chosen location, confirm:
1. **No line of sight to the player** — trace player→Scuttler is blocked. Check via:
   ```python
   import unreal
   # positions from the log / actor queries; LineTraceSingle on Visibility
   ```
2. **Not directly lit** — the point is outside every `BP_HorrorLight`'s `Radius`, or occluded from it. Confirm the final point differs from what you'd get with the `DirectLight` test disabled (temporarily raise its filter to a no-op and compare).
3. The ability ended (StateTree left `Fleeing`); moving the player to re-establish sight makes the Scuttler flee again.
4. Breaking sight for good returns the Scuttler to its prior idle/patrol behavior.

- [ ] **Step 5: Tune**

Adjust in-editor only (no code): donut radii/rings, test weights (`Distance` from player vs. from querier), `ULightSourceComponent.Radius`, `AcceptanceRadius`, `TraceHeightOffset`. Re-run Step 3 until the behavior reads right. Commit any asset changes:
```bash
git add SurvivalTemplate/Content/Variant_Horror/AI SurvivalTemplate/Content/Variant_Horror/Blueprints/Light/BP_HorrorLight.uasset
git commit -m "Tune Scuttler flee-to-shadow EQS and light radii"
```

- [ ] **Step 6: Re-run the Task 3 automation test**

```python
import unreal
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),
    "Automation RunTests SurvivalTemplate.Light.IsPointLit")
```
Expected: still passes. Nothing to commit if green.

---

## Self-Review

**1. Spec coverage:**

| Spec section | Task(s) |
|---|---|
| New C++ module | 1 |
| Native gameplay tags | 2 |
| `ULightRegistrySubsystem` / `ULightSourceComponent` | 3, 13 |
| `UEnvQueryContext_Player` | 4, 11 |
| `UEnvQueryTest_DirectLight` | 5, 11 |
| `EQS_FleeToShadow` query + tests | 11 |
| `AScuttlerCharacter` + ASC + `PossessedBy` grant | 6, 12 |
| `UGA_FleeToShadow` | 8, 12 |
| `UAbilityTask_MoveTo` | 7 |
| `GA_Scuttler_FleeToShadow` BP subclass | 12 |
| Perception → `bCanSeePlayer` signal | 9 |
| `FStateTreeCondition_CanSeePlayer` | 9, 14 |
| `FSTTask_ActivateAbilityByTag` | 10, 14 |
| `ST_Scuttler` `Fleeing` state + transitions | 14 |
| Navmesh prerequisite | 0, 15 |
| Prerequisites-to-verify list | 0 |
| Automation spec for `IsPointLit` | 3 |
| Functional / PIE test + LogScuttler | 16 |
| Error-handling table (no AI controller, no query, empty EQS, move fail, missing subsystem, ASC not found) | handled in 5, 7, 8, 10 impls |
| Reparent BP_Scuttler | 12 |
| Tuning parameters | 16 |

No gaps.

**2. Placeholder scan:** No "TBD"/"handle edge cases"-style steps. Every code step has a code block. The "verify against 5.8 headers" notes are explicit engineering instructions with a named fallback, not deferrals. Task 0 writes real findings that Tasks 6/9/12/14/15 consume.

**3. Type consistency:** `IsPointLit(const FVector&, const AActor*)` — same in Task 3 (def), Task 5 (call). `MoveTo(UGameplayAbility*, FVector, float)` and `OnFinished(bool)` — Task 7 (def), Task 8 (call). `CanSeePlayer()` — Task 6 (def), Task 9 (impl body), condition in Task 9. `Ability_Scuttler_FleeToShadow` tag — Task 2 (def), Tasks 8/10/14 (use). `FleeQuery` UPROPERTY — Task 8 (def), Task 12 (set). `DefaultAbilities` — Task 6 (def), Task 12 (set). Consistent.

---

## Task 0 Findings

_(filled in during Task 0)_
