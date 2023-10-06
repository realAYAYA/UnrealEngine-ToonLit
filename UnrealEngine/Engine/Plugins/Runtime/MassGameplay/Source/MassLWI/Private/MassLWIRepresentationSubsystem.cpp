// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWIRepresentationSubsystem.h"
#include "Engine/World.h"
#include "MassLWIClientActorSpawnerSubsystem.h"


//-----------------------------------------------------------------------------
// UMassLWIRepresentationSubsystem
//-----------------------------------------------------------------------------
void UMassLWIRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	UWorld* World = GetWorld();
	check(World);

	if (World->GetNetMode() == NM_Client)
	{
		ActorSpawnerSubsystem = World->GetSubsystem<UMassLWIClientActorSpawnerSubsystem>();
	}
}

