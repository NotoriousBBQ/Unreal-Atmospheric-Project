// Fill out your copyright notice in the Description page of Project Settings.

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
