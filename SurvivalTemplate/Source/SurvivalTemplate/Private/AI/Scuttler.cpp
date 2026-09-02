// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Scuttler.h"

// Sets default values
AScuttler::AScuttler()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AScuttler::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AScuttler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AScuttler::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

