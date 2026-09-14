// Fill out your copyright notice in the Description page of Project Settings.

#include "EQS/EnvQueryTest_DirectLight.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Light/LightRegistrySubsystem.h"
#include "Engine/World.h"

UEnvQueryTest_DirectLight::UEnvQueryTest_DirectLight(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	SetWorkOnFloatValues(false);
	ValidItemType = UEnvQueryItemType_Point::StaticClass();

	TraceHeightOffset.DefaultValue = 60.f;

	// Default to filter+score, keeping items whose "is lit" value matches BoolValue (false == not lit).
	TestPurpose = EEnvTestPurpose::FilterAndScore;
	FilterType = EEnvTestFilterType::Match;
	BoolValue.DefaultValue = false; // desired "bIsLit" value
}

void UEnvQueryTest_DirectLight::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* Owner = QueryInstance.Owner.Get();
	if (!Owner)
	{
		return;
	}

	UWorld* World = QueryInstance.World;
	ULightRegistrySubsystem* Reg = World ? World->GetSubsystem<ULightRegistrySubsystem>() : nullptr;

	BoolValue.BindData(Owner, QueryInstance.QueryID);
	const bool bWantsLit = BoolValue.GetValue();

	TraceHeightOffset.BindData(Owner, QueryInstance.QueryID);
	const float ZOffset = TraceHeightOffset.GetValue();

	const AActor* IgnoreActor = Cast<AActor>(Owner);

	if (!Reg)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnvQueryTest_DirectLight: no ULightRegistrySubsystem; treating all points as unlit."));
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector Point = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0.f, 0.f, ZOffset);
		const bool bIsLit = Reg ? Reg->IsPointLit(Point, IgnoreActor) : false;
		It.SetScore(TestPurpose, FilterType, bIsLit, bWantsLit);
	}
}

FText UEnvQueryTest_DirectLight::GetDescriptionTitle() const
{
	return NSLOCTEXT("SurvivalTemplate", "DirectLightTitle", "Direct Light");
}

FText UEnvQueryTest_DirectLight::GetDescriptionDetails() const
{
	return NSLOCTEXT("SurvivalTemplate", "DirectLightDetails", "Filter/score points by whether a registered light directly illuminates them");
}
