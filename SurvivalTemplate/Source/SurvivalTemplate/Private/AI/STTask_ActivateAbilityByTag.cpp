// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/STTask_ActivateAbilityByTag.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"

namespace
{
	UAbilitySystemComponent* ResolveASC(UObject* Owner)
	{
		AActor* OwnerActor = Cast<AActor>(Owner);
		if (OwnerActor == nullptr)
		{
			return nullptr;
		}

		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			return ASC;
		}

		if (const AAIController* AIController = Cast<AAIController>(OwnerActor))
		{
			return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AIController->GetPawn());
		}

		return nullptr;
	}

	/** True if the ASC currently has an active ability spec whose asset tags contain AbilityTag. */
	bool IsAbilityWithTagActive(const UAbilitySystemComponent& ASC, const FGameplayTag& AbilityTag)
	{
		for (const FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability != nullptr && Spec.Ability->GetAssetTags().HasTag(AbilityTag))
			{
				return true;
			}
		}
		return false;
	}
}

EStateTreeRunStatus FSTTask_ActivateAbilityByTag::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ASC.Reset();

	if (!Data.AbilityTag.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UAbilitySystemComponent* ASC = ResolveASC(Context.GetOwner());
	if (ASC == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(Data.AbilityTag)))
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.ASC = ASC;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSTTask_ActivateAbilityByTag::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);

	const UAbilitySystemComponent* ASC = Data.ASC.Get();
	if (ASC == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	return IsAbilityWithTagActive(*ASC, Data.AbilityTag) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FSTTask_ActivateAbilityByTag::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);

	if (Data.bEndAbilityOnExit)
	{
		if (UAbilitySystemComponent* ASC = Data.ASC.Get())
		{
			FGameplayTagContainer CancelTags(Data.AbilityTag);
			ASC->CancelAbilities(&CancelTags);
		}
	}

	Data.ASC.Reset();
}
