// Fill out your copyright notice in the Description page of Project Settings.

// Core header, safe for every target; also defines WITH_AUTOMATION_TESTS used by the guard below.
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_AUTOMATION_TESTS

#include "Light/LightRegistrySubsystem.h"
#include "Light/LightSourceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLightRegistrySubsystemTest,
    "SurvivalTemplate.Light.IsPointLit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLightRegistrySubsystemTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    TestNotNull("world", World);

    ULightRegistrySubsystem* Reg = World->GetSubsystem<ULightRegistrySubsystem>();
    TestNotNull("subsystem", Reg);

    AActor* LightActor = World->SpawnActor<AActor>(FVector(0, 0, 200), FRotator::ZeroRotator);
    ULightSourceComponent* Light = NewObject<ULightSourceComponent>(LightActor);
    Light->Radius = 500.f;
    Light->RegisterComponent();
    LightActor->SetRootComponent(Light);
    Light->SetWorldLocation(FVector(0, 0, 200));
    Reg->RegisterLight(Light);

    // Inside radius, clear LOS -> lit
    TestTrue("near+clear is lit", Reg->IsPointLit(FVector(100, 0, 200), nullptr));
    // Outside radius -> not lit
    TestFalse("far is not lit", Reg->IsPointLit(FVector(900, 0, 200), nullptr));

    // Wall between a near point and the light -> not lit
    AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(FVector(150, 0, 200), FRotator::ZeroRotator);
    Wall->SetMobility(EComponentMobility::Movable);
    Wall->GetStaticMeshComponent()->SetStaticMesh(
        LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorScale3D(FVector(1.f, 5.f, 5.f));
    Wall->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TestFalse("blocked point is not lit", Reg->IsPointLit(FVector(300, 0, 200), nullptr));

    Reg->UnregisterLight(Light);
    TestFalse("after unregister nothing is lit", Reg->IsPointLit(FVector(100, 0, 200), nullptr));

    return true;
}

#endif // WITH_EDITOR && WITH_AUTOMATION_TESTS
