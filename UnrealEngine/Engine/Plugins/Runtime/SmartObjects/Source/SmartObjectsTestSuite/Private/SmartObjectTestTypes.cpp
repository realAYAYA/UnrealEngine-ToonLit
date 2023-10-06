// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectTestTypes.h"

//----------------------------------------------------------------------//
// USmartObjectTestSubsystem
//----------------------------------------------------------------------//
USmartObjectTestSubsystem::USmartObjectTestSubsystem(const FObjectInitializer& ObjectInitializer)
{
	// for testing we want to directly control the moment and parameters of InitializeRuntime call
	bAutoInitializeEditorInstances = false;
}

void USmartObjectTestSubsystem::RebuildAndInitializeForTesting()
{
	UWorld& World = GetWorldRef();
	// note that the following call won't result in InitializeRuntime call, even in the editor - due to bAutoInitializeEditorInstances
	OnWorldComponentsUpdated(World);

	InitializeRuntime();
}

//----------------------------------------------------------------------//
// ASmartObjectTestCollection
//----------------------------------------------------------------------//
bool ASmartObjectTestCollection::RegisterWithSubsystem(const FString & Context)
{
	return false;
}

bool ASmartObjectTestCollection::UnregisterWithSubsystem(const FString& Context)
{
	return false;
}
