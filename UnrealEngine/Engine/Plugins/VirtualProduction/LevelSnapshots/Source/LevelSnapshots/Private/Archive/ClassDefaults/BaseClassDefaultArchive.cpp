// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseClassDefaultArchive.h"

#include "WorldSnapshotData.h"

UE::LevelSnapshots::Private::FBaseClassDefaultArchive::FBaseClassDefaultArchive(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, bool bIsLoading, UObject* InObjectToRestore)
	:
	Super(InObjectData, InSharedData, bIsLoading, InObjectToRestore)
{
	// Description of CPF_Transient: "Property is transient: shouldn't be saved or loaded, except for Blueprint CDOs."
	ExcludedPropertyFlags = CPF_BlueprintAssignable | CPF_Deprecated
		// Do not save any instanced references when serialising CDOs
		| CPF_ContainsInstancedReference | CPF_InstancedReference | CPF_PersistentInstance;
	
	// Otherwise we are not allowed to serialize transient properties
	ArSerializingDefaults = true;
}