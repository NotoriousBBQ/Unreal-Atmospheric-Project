// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Scuttler.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Kismet/GameplayStatics.h"

AScuttler::AScuttler()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AScuttler::BeginPlay()
{
	Super::BeginPlay();

	// Primary path: perception component on this pawn.
	if (UAIPerceptionComponent* Perception = FindComponentByClass<UAIPerceptionComponent>())
	{
		Perception->OnTargetPerceptionUpdated.AddDynamic(this, &AScuttler::HandleTargetPerceptionUpdated);
	}
	// Fallback: if perception lives on AI_Scuttler instead, that controller binds the
	// delegate and calls SetCanSeePlayer() on this pawn.
}

void AScuttler::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Actor == nullptr || Actor != PlayerPawn)
	{
		return;
	}
	if (Stimulus.Type != UAISense::GetSenseID<UAISense_Sight>())
	{
		return;
	}
	SetCanSeePlayer(Stimulus.WasSuccessfullySensed(), Actor);
}

void AScuttler::SetCanSeePlayer(bool bNewValue, AActor* InSeenPlayer)
{
	bCanSeePlayer = bNewValue;
	if (bCanSeePlayer)
	{
		if (InSeenPlayer)
		{
			SeenPlayer = InSeenPlayer;
		}
		if (const UWorld* World = GetWorld())
		{
			LastSeenTime = World->GetTimeSeconds();
		}
	}
}

void AScuttler::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		GrantDefaultAbilities();
	}
}

void AScuttler::GrantDefaultAbilities()
{
	if (bAbilitiesGranted || !HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& Ability : DefaultAbilities)
	{
		if (Ability)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
		}
	}

	bAbilitiesGranted = true;
}
