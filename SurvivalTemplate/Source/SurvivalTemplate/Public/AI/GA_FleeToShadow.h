// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GA_FleeToShadow.generated.h"

class UEnvQuery;
struct FEnvQueryResult;

/**
 * ServerOnly ability: on activate, runs the FleeQuery EQS against the avatar and moves
 * the pawn to the single best (darkest) result via UAbilityTask_MoveTo, ending when the
 * move finishes. No avatar / no query / no viable result ends the ability immediately.
 */
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
