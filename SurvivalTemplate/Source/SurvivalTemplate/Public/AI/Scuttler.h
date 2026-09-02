// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Perception/AIPerceptionTypes.h"
#include "Scuttler.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

UCLASS()
class SURVIVALTEMPLATE_API AScuttler : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AScuttler();

	virtual void BeginPlay() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystemComponent; }
	virtual void PossessedBy(AController* NewController) override;

	UFUNCTION(BlueprintPure, Category="AI")
	bool CanSeePlayer() const { return bCanSeePlayer; }

	/** Pushed by whichever actor owns the AIPerception delegate (pawn or controller). */
	UFUNCTION(BlueprintCallable, Category="AI")
	void SetCanSeePlayer(bool bNewValue, AActor* InSeenPlayer = nullptr);

protected:
	UPROPERTY(VisibleAnywhere, Category="Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly, Category="Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(BlueprintReadOnly, Category="AI")
	bool bCanSeePlayer = false;

	UPROPERTY()
	TObjectPtr<AActor> SeenPlayer = nullptr;

	float LastSeenTime = 0.f;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void GrantDefaultAbilities();

private:
	bool bAbilitiesGranted = false;
};
