// Fill out your copyright notice in the Description page of Project Settings.

#include "Light/LightRegistrySubsystem.h"
#include "Light/LightSourceComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "CollisionQueryParams.h"

void ULightRegistrySubsystem::RegisterLight(ULightSourceComponent* Light)
{
    if (Light) { RegisteredLights.AddUnique(Light); }
}

void ULightRegistrySubsystem::UnregisterLight(ULightSourceComponent* Light)
{
    RegisteredLights.RemoveAll([Light](const TWeakObjectPtr<ULightSourceComponent>& P)
    {
        return !P.IsValid() || P.Get() == Light;
    });
}

bool ULightRegistrySubsystem::IsPointLit(const FVector& Point, const AActor* IgnoreActor) const
{
    const UWorld* W = GetWorld();
    if (!W) { return false; }

    for (const TWeakObjectPtr<ULightSourceComponent>& WeakLight : RegisteredLights)
    {
        const ULightSourceComponent* Light = WeakLight.Get();
        if (!Light || !Light->IsLightActive()) { continue; }

        const FVector LightLoc = Light->GetComponentLocation();
        if (FVector::DistSquared(Point, LightLoc) > FMath::Square(Light->Radius)) { continue; }

        FCollisionQueryParams Params(SCENE_QUERY_STAT(LightRegistryLOS), false);
        if (IgnoreActor) { Params.AddIgnoredActor(IgnoreActor); }
        if (const AActor* LightOwner = Light->GetOwner()) { Params.AddIgnoredActor(LightOwner); }

        const bool bBlocked = W->LineTraceTestByChannel(Point, LightLoc, ECC_Visibility, Params);
        if (!bBlocked) { return true; } // reached the light -> lit
    }
    return false;
}

ULightRegistrySubsystem* ULightRegistrySubsystem::Get(const UObject* WorldContext)
{
    const UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
    return W ? W->GetSubsystem<ULightRegistrySubsystem>() : nullptr;
}
