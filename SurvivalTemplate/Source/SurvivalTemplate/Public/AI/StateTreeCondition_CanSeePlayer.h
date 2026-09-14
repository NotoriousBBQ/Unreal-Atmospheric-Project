// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeCondition_CanSeePlayer.generated.h"

USTRUCT()
struct FStateTreeCondition_CanSeePlayerInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

/**
 * StateTree condition returning whether the Scuttler currently sees the player.
 *
 * Resolves an AScuttler from the execution context owner, or from the owner's pawn
 * when the owner is an AIController, then returns CanSeePlayer() (optionally inverted).
 */
USTRUCT(DisplayName = "Scuttler Can See Player", Category = "Scuttler")
struct SURVIVALTEMPLATE_API FStateTreeCondition_CanSeePlayer : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeCondition_CanSeePlayerInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
