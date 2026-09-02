// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTreeCondition_CanSeePlayer.h"
#include "AI/Scuttler.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

bool FStateTreeCondition_CanSeePlayer::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AScuttler* Scuttler = Cast<AScuttler>(Context.GetOwner());
	if (Scuttler == nullptr)
	{
		if (const AAIController* AIController = Cast<AAIController>(Context.GetOwner()))
		{
			Scuttler = Cast<AScuttler>(AIController->GetPawn());
		}
	}

	const bool bSees = Scuttler != nullptr && Scuttler->CanSeePlayer();
	return InstanceData.bInvert ? !bSees : bSees;
}
