// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvQueryTest_DirectLight.generated.h"

/**
 * EQS test that filters/scores candidate points by whether a registered light directly
 * illuminates them (via ULightRegistrySubsystem::IsPointLit). Unlit points pass the filter
 * and score 1; lit points are filtered out and score 0.
 */
UCLASS(meta = (DisplayName = "Direct Light"))
class SURVIVALTEMPLATE_API UEnvQueryTest_DirectLight : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEnvQueryTest_DirectLight(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Vertical offset applied to each point before the point->light trace, so the floor doesn't self-block. */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	FAIDataProviderFloatValue TraceHeightOffset;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
