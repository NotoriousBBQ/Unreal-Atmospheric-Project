// Fill out your copyright notice in the Description page of Project Settings.

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
