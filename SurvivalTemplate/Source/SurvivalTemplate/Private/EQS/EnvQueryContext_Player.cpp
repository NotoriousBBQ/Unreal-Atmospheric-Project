// Fill out your copyright notice in the Description page of Project Settings.

#include "EQS/EnvQueryContext_Player.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "Kismet/GameplayStatics.h"

void UEnvQueryContext_Player::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (!QueryOwner) { return; }

    if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(QueryOwner, 0))
    {
        UEnvQueryItemType_Actor::SetContextHelper(ContextData, PlayerPawn);
    }
}
