# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Unreal Engine **5.8.1** game project. The repo is `CreepyAtmosphere` (README title: "Unreal-Atmospheric-Project"); the actual UE project lives in `SurvivalTemplate/` and opens from `SurvivalTemplate/SurvivalTemplate.uproject`.

This is a **Blueprint-only project** — there is no `Source/` directory and no C++ modules. All gameplay logic lives in `.uasset` Blueprints, edited in the Unreal Editor, not in this repo as text. `+ActiveGameNameRedirects` in `DefaultEngine.ini` still maps the old `TP_FirstPersonBP` module name to `/Script/SurvivalTemplate` for asset compatibility.

It started from Epic's **First Person BP** template and adds a **Horror** variant (`Content/Variant_Horror/`), which is the active game.

## Working with this project

- **Open the project:** launch `SurvivalTemplate/SurvivalTemplate.uproject` with UE 5.8 (the editor, or via Rider/the Epic launcher). There is no command-line build, lint, or test step — nothing compiles from this checkout because there is no C++.
- **Rider MCP + Unreal bridge:** the `mcp__rider__ue_*` tools (`ue_status`, `ue_play`, `ue_get_logs`, `ue_execute_python`, `ue_export_blueprint_nodes`, `ue_import_blueprint_nodes`, etc.) drive a *running* editor instance. Use `ue_health` / `ue_status` first to check the editor is up. Prefer these over trying to parse `.uasset` binaries.
- **Editor logs:** `SurvivalTemplate/Saved/Logs/SurvivalTemplate.log`. Note `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/` are gitignored and machine-generated (one stale `Saved/Logs` file predates the ignore rule).
- **Binary assets:** `.uasset` / `.umap` are binary and cannot be meaningfully diffed or edited as text. Describe intended changes for the user to make in-editor, or use the `ue_*` Blueprint node tools.

## Enabled plugins (drive the architecture)

- **GameplayAbilities (GAS)** — ability/attribute system.
- **GameplayStateTree** — AI and general state logic (the Scuttler enemy uses it).
- **ModelingToolsEditorMode** — editor-only mesh tooling.

## Map / GameMode wiring

- Startup and default map: `Content/Variant_Horror/Lvl_Horror.umap`.
- `GlobalDefaultGameMode` in `DefaultEngine.ini` is `BP_FirstPersonGameMode`, **but** `Lvl_Horror` overrides it per-map with `BP_HorrorGameMode`. Check the map's World Settings, not just the ini, to know which GameMode/Pawn/Controller is live.
- `Content/FirstPerson/` holds the base template classes (`BP_FirstPersonCharacter`, `BP_FirstPersonGameMode`, `BP_FirstPersonPlayerController`, `Lvl_FirstPerson`); `Content/Variant_Horror/Blueprints/` holds the Horror equivalents that are actually used.

## Content structure

- `Content/Variant_Horror/Blueprints/` — the live gameplay:
  - `BP_HorrorCharacter`, `BP_HorrorGameMode`, `BP_HorrorPlayerController`
  - `BP_Scuttler` (AI enemy) + `AI_Scuttler` (controller) + `ST_Scuttler` (StateTree brain)
  - `BP_HorrorLight` — flickering-light actor with dust-mote Niagara (`Light/Assets/`)
  - `UI/UI_Horror` — in-game HUD/widget
- `Content/Variant_Horror/Input/` — Horror-specific Enhanced Input: `IMC_Horror`, `IA_Sprint`.
- `Content/Input/` — base Enhanced Input: `IMC_Default`, `IMC_MouseLook`, `IA_Move`, `IA_Look`, `IA_MouseLook`, `IA_Jump`, plus mobile touch UI in `Input/Touch/`.
- `Content/LevelPrototyping/Interactable/` — reusable interactables: `BP_DoorFrame`, `BP_JumpPad`, `BP_WobbleTarget`.
- `Content/Characters/Mannequins/` — Epic Manny/Quinn skeletal meshes, control rigs, and the pistol/rifle/unarmed animation sets.
- `Content/__ExternalActors__/` and `__ExternalObjects__/` — One File Per Actor (OFPA) data for the World Partition maps; these are the per-actor `.uasset`s for level edits and must stay in sync with their `.umap`.

## Rendering setup (matters for the "atmospheric" goal)

`DefaultEngine.ini` configures a fully dynamic pipeline: **Lumen** GI + reflections, **Substrate** materials, **hardware ray tracing** on, **Virtual Shadow Maps** on, `r.AllowStaticLighting=False` (no baked lighting — do not add lightmass/static lights), Local Exposure tuned down for contrast. Target RHI is **D3D12 SM6**. Lighting and post-process work should assume everything is real-time.

## Collision

Custom `Projectile` collision profile and an `ECC_GameTraceChannel1` = "Projectile" trace channel are defined in `DefaultEngine.ini`; the `Trigger` profile is edited to ignore projectiles.
