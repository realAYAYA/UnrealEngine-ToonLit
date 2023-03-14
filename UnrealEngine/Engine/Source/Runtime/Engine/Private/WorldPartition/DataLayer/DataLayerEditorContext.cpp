// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerEditorContext.h"

#if WITH_EDITOR

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#include "Engine/World.h"

/*
 * FDataLayerEditorContext
 */
FDataLayerEditorContext::FDataLayerEditorContext(UWorld* InWorld, const TArray<FName>& InDataLayerInstances)
	: Hash(FDataLayerEditorContext::EmptyHash)
{
	UDataLayerSubsystem* DataLayerSubsystem = UWorld::GetSubsystem<UDataLayerSubsystem>(InWorld);
	if (!DataLayerSubsystem)
	{
		return;
	}

	for (const FName& DataLayerInstanceName : InDataLayerInstances)
	{
		if (const UDataLayerInstance* DataLayerInstance = DataLayerSubsystem->GetDataLayerInstance(DataLayerInstanceName))
		{
			DataLayerInstances.AddUnique(DataLayerInstance->GetDataLayerFName());
		}
	}

	if (DataLayerInstances.Num())
	{
		DataLayerInstances.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
		for (FName InstanceName : DataLayerInstances)
		{
			Hash = FCrc::StrCrc32(*InstanceName.ToString(), Hash);
		}
		check(Hash != FDataLayerEditorContext::EmptyHash);
	}
}

#endif