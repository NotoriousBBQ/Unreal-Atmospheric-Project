// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LightRegistrySubsystem.generated.h"

class ULightSourceComponent;

UCLASS()
class SURVIVALTEMPLATE_API ULightRegistrySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RegisterLight(ULightSourceComponent* Light);
    void UnregisterLight(ULightSourceComponent* Light);

    /** True if Point is within any active light's Radius AND has clear line of sight to that light. */
    bool IsPointLit(const FVector& Point, const AActor* IgnoreActor) const;

    static ULightRegistrySubsystem* Get(const UObject* WorldContext);

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<ULightSourceComponent>> RegisteredLights;
};
