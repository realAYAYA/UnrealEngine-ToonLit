// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeWorldObjectSnapshotArchive.h"

#include "LevelSnapshotsLog.h"
#include "ObjectSnapshotData.h"
#include "WorldSnapshotData.h"

#include "Serialization/ArchiveSerializedPropertyChain.h"
#include "UObject/UnrealType.h"

void UE::LevelSnapshots::Private::FTakeWorldObjectSnapshotArchive::TakeSnapshot(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InOriginalObject)
{
	check(InOriginalObject);
	
	FTakeWorldObjectSnapshotArchive Archive(InObjectData, InSharedData, InOriginalObject);
	InOriginalObject->Serialize(Archive);
	InObjectData.ObjectFlags = InOriginalObject->GetFlags();
}

UE::LevelSnapshots::Private::FTakeWorldObjectSnapshotArchive::FTakeWorldObjectSnapshotArchive(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InOriginalObject)
	: Super(InObjectData, InSharedData, false, InOriginalObject)
	, Archetype(InOriginalObject->GetArchetype())
{
#if UE_BUILD_DEBUG
	UE_LOG(LogLevelSnapshots, VeryVerbose, TEXT("FTakeWorldObjectSnapshotArchive: %s (%s)"), *InOriginalObject->GetPathName(), *InOriginalObject->GetClass()->GetPathName());
#endif
}

bool UE::LevelSnapshots::Private::FTakeWorldObjectSnapshotArchive::ShouldSkipProperty(const FProperty* InProperty) const
{
	const bool bSuperWantsToSkip = Super::ShouldSkipProperty(InProperty);

	if (!bSuperWantsToSkip)
	{
		const bool bIsRootProperty = [this]()
		{
			const FArchiveSerializedPropertyChain* PropertyChain = GetSerializedPropertyChain();
			return PropertyChain == nullptr || PropertyChain->GetNumProperties() == 0;
		}();
		if (!bIsRootProperty)
		{
			// We are within a struct property and we have already checked that the struct property does not have equal values
			// Simply allow all properties but only ones which are not deprecated.
			// TODO: We could save more disk space here by checking the subproperties
			return InProperty->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient);
		}
		
		UObject* OriginalContainer = GetSerializedObject();
		UObject* ClassDefaultContainer = Archetype;
		for (int32 ArrayDim = 0; ArrayDim < InProperty->ArrayDim; ++ArrayDim)
		{
			if (!InProperty->Identical_InContainer(OriginalContainer, ClassDefaultContainer, ArrayDim))
			{
				return false; 
			}
		}
	}
	
	return true;
}
