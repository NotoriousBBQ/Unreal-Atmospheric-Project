// Fill out your copyright notice in the Description page of Project Settings.

#include "Light/LightSourceComponent.h"
#include "Light/LightRegistrySubsystem.h"
#include "Engine/World.h"

ULightSourceComponent::ULightSourceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void ULightSourceComponent::OnRegister()
{
    Super::OnRegister();
    UWorld* W = GetWorld();
    if (W && W->IsGameWorld())
    {
        bActive = bStartsRegistered;
        if (bStartsRegistered) { AddToRegistry(); }
    }
}

void ULightSourceComponent::OnUnregister()
{
    RemoveFromRegistry();
    Super::OnUnregister();
}

void ULightSourceComponent::SetLightActive(bool bNewActive)
{
    if (bNewActive == bActive) { return; }
    bActive = bNewActive;
    if (bActive) { AddToRegistry(); } else { RemoveFromRegistry(); }
}

void ULightSourceComponent::AddToRegistry()
{
    if (bRegistered) { return; }
    if (ULightRegistrySubsystem* Reg = ULightRegistrySubsystem::Get(this))
    {
        Reg->RegisterLight(this);
        bRegistered = true;
    }
}

void ULightSourceComponent::RemoveFromRegistry()
{
    if (!bRegistered) { return; }
    if (ULightRegistrySubsystem* Reg = ULightRegistrySubsystem::Get(this))
    {
        Reg->UnregisterLight(this);
    }
    bRegistered = false;
}
