// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSnapshotsFilteringLibrary.h"

#include "Selection/ApplySnapshotFilter.h"
#include "ConstantFilter.h"
#include "LevelSnapshot.h"
#include "LevelSnapshotFilters.h"
#include "Selection/SnapshotSelectionBuilder.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"

FPropertySelectionMap ULevelSnapshotsFilteringLibrary::DiffAndFilterSnapshot(UWorld* InWorld, ULevelSnapshot* InSnapshot, const ULevelSnapshotFilter* Filter)
{
	return UE::LevelSnapshots::Private::FSnapshotSelectionBuilder::DiffAndFilterSnapshot(InWorld, InSnapshot, Filter);
}

void ULevelSnapshotsFilteringLibrary::ApplyFilterToFindSelectedProperties(ULevelSnapshot* Snapshot, FPropertySelectionMap& MapToAddTo, AActor* WorldActor, AActor* DeserializedSnapshotActor, const ULevelSnapshotFilter* Filter)
{
	if (Filter == nullptr)
	{
		Filter = GetMutableDefault<UConstantFilter>();
	}
	
	UE::LevelSnapshots::Private::FApplySnapshotFilter::Make(Snapshot, DeserializedSnapshotActor, WorldActor, Filter)
		.ApplyFilterToFindSelectedProperties(MapToAddTo);
}
