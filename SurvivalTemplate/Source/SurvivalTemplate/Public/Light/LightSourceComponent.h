// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "LightSourceComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALTEMPLATE_API ULightSourceComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    ULightSourceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Occlusion")
    float Radius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Light Occlusion")
    bool bStartsRegistered = true;

    UFUNCTION(BlueprintCallable, Category="Light Occlusion")
    void SetLightActive(bool bNewActive);

    bool IsLightActive() const { return bActive; }

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

private:
    bool bActive = true;
    bool bRegistered = false;
    void AddToRegistry();
    void RemoveFromRegistry();
};
