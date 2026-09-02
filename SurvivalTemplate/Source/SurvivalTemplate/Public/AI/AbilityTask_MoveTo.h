// Fill out your copyright notice in the Description page of Project Settings.

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
