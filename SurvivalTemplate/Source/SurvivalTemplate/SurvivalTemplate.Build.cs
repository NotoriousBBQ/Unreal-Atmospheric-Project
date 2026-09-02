// Fill out your copyright notice in the Description page of Project Settings.

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

		if (Target.bBuildEditor)
		{
		    // Editor-only: automation-test helpers used by Private/Tests/.
		    PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
