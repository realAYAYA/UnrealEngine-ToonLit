// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassActorEditorSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityEditorSubsystem.h"


//----------------------------------------------------------------------//
//  UMassActorEditorSubsystem
//----------------------------------------------------------------------//
void UMassActorEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UMassEntityEditorSubsystem* MassEditorEditorSubsystem = Collection.InitializeDependency<UMassEntityEditorSubsystem>();
	check(MassEditorEditorSubsystem);
	FMassEntityManager& MassEntityManager = MassEditorEditorSubsystem->GetMutableEntityManager();
	ActorManager = MakeShareable(new FMassActorManager(MassEntityManager.AsShared()));
}
