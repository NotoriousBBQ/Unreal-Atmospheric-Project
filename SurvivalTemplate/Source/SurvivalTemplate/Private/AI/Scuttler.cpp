// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Scuttler.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

AScuttler::AScuttler()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
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
