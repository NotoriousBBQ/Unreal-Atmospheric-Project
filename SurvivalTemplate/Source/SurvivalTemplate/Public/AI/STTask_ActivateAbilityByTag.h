// Fill out your copyright notice in the Description page of Project Settings.

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

	/** Asset tag identifying the ability (or abilities) to activate. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag AbilityTag;

	/** When true, ExitState cancels any still-running ability matching AbilityTag. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bEndAbilityOnExit = true;

	/** Resolved ability system component, cached for ExitState. */
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
};

/**
 * StateTree task that activates a GAS ability by asset tag on EnterState and stays
 * Running until no ability matching that tag is active any more, then Succeeds.
 *
 * Resolves a UAbilitySystemComponent from the execution context owner, or from the
 * owner's pawn when the owner is an AIController. Fails immediately if no ASC is
 * found, the tag is invalid, or TryActivateAbilitiesByTag activates nothing.
 *
 * Completion is detected by polling the ASC's activatable ability specs in Tick
 * rather than by binding OnAbilityEnded: StateTree instance data structs may be
 * relocated in memory between ticks, so a delegate lambda cannot safely capture a
 * reference to this task's instance data.
 */
USTRUCT(DisplayName = "Activate Ability By Tag", Category = "Scuttler")
struct SURVIVALTEMPLATE_API FSTTask_ActivateAbilityByTag : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FSTTask_ActivateAbilityByTagInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
