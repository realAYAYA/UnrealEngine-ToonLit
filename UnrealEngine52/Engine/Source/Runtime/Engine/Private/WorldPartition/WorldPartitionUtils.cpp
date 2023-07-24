// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "WorldPartition/WorldPartitionUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageContext.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/WorldPartitionLog.h"

#define LOCTEXT_NAMESPACE "WorldPartition"

FWorldPartitionUtils::FSimulateCookedSession::FSimulateCookedSession(UWorld* InWorld)
: bIsValid(false)
{
	UE_CLOG(!InWorld, LogWorldPartition, Error, TEXT("FSimulateCookedSession was given an invalid world."));
	UE_CLOG(InWorld && !InWorld->IsPartitionedWorld(), LogWorldPartition, Warning, TEXT("FSimulateCookedSession was given a non-partitioned world."));
	
	if (InWorld)
	{
		WorldPartition = InWorld->GetWorldPartition();
		bIsValid = WorldPartition.IsValid() ? SimulateCook() : false;
	}

	UE_CLOG(!bIsValid, LogWorldPartition, Warning, TEXT("FSimulateCookedSession failed to generate streaming."));
}

bool FWorldPartitionUtils::FSimulateCookedSession::SimulateCook()
{
	check(WorldPartition.IsValid());
	if (!WorldPartition->CanGenerateStreaming() || IsRunningGame() || WorldPartition->GetWorld()->IsGameWorld() || IsRunningCookCommandlet())
	{
		UE_CLOG(!WorldPartition->CanGenerateStreaming(), LogWorldPartition, Warning, TEXT("FSimulateCookedSession cannot generate streaming, world partition is already generating streaming."));
		UE_CLOG(IsRunningGame() || WorldPartition->GetWorld()->IsGameWorld(), LogWorldPartition, Warning, TEXT("FSimulateCookedSession cannot generate streaming while running PIE/game."));
		UE_CLOG(IsRunningCookCommandlet(), LogWorldPartition, Warning, TEXT("FSimulateCookedSession cannot generate streaming while cooking."));
		return false;
	}

	// Simulate cook
	FWorldPartitionCookPackageContext CookContext;
	WorldPartition->BeginCook(CookContext);

	if (!CookContext.GatherPackagesToCook())
	{
		return false;
	}

	// Simulate Content Bundles injection
	TArray<TSharedPtr<FContentBundleEditor>> ContentBundles;
	WorldPartition->GetWorld()->ContentBundleManager->GetEditorContentBundle(ContentBundles);
	for (TSharedPtr<FContentBundleEditor>& ContentBundle : ContentBundles)
	{
		if (ContentBundle->HasCookedContent())
		{
			WorldPartition->RuntimeHash->InjectExternalStreamingObject(ContentBundle->GetStreamingObject());
		}
	}

	return true;
}

FWorldPartitionUtils::FSimulateCookedSession::~FSimulateCookedSession()
{
	if (WorldPartition.IsValid())
	{
		// Simulate Content Bundles injection
		TArray<TSharedPtr<FContentBundleEditor>> ContentBundles;
		WorldPartition->GetWorld()->ContentBundleManager->GetEditorContentBundle(ContentBundles);
		for (TSharedPtr<FContentBundleEditor>& ContentBundle : ContentBundles)
		{
			if (ContentBundle->HasCookedContent())
			{
				URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject = ContentBundle->GetStreamingObject();
				WorldPartition->RuntimeHash->RemoveExternalStreamingObject(ExternalStreamingObject);
				// Trash external streaming object
				ExternalStreamingObject->Rename(*MakeUniqueObjectName(GetTransientPackage(), URuntimeHashExternalStreamingObjectBase::StaticClass()).ToString(), GetTransientPackage(), REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_DoNotDirty);
			}
		}

		WorldPartition->FlushStreaming();
	}
}

bool FWorldPartitionUtils::FSimulateCookedSession::ForEachStreamingCells(TFunctionRef<void(const IWorldPartitionCell*)> Func)
{
	if (!WorldPartition.IsValid() || !WorldPartition->RuntimeHash)
	{
		return false;
	}

	WorldPartition->RuntimeHash->ForEachStreamingCells([Func](const UWorldPartitionRuntimeCell* Cell)
	{
		if (Cell)
		{
			Func(Cell);
		}
		return true;
	});

	return true;
}

bool FWorldPartitionUtils::FSimulateCookedSession::GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells)
{
	if (WorldPartition.IsValid())
	{
		return WorldPartition->GetIntersectingCells(InSources, OutCells);
	}
	return false;
}
	
#undef LOCTEXT_NAMESPACE
	
#endif
