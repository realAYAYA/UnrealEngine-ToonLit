// Copyright Epic Games, Inc. All Rights Reserved.


#include "XRCreativeToolActor.h"


// Sets default values
AXRCreativeToolActor::AXRCreativeToolActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AXRCreativeToolActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AXRCreativeToolActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

