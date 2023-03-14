// Copyright Epic Games, Inc. All Rights Reserved.

#include "SnapshotTestRunner.h"

#include "LevelSnapshotsFilteringLibrary.h"
#include "Data/LevelSnapshot.h"
#include "LevelSnapshotsFunctionLibrary.h"

#include "PreviewScene.h"

FName UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::DefaultSnapshotId = FName("DefaultSnapshotId");

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::FSnapshotTestRunner()
{
	TestWorld = MakeShared<FPreviewScene>(
		FPreviewScene::ConstructionValues()
			.SetEditor(true)
			);
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::FSnapshotTestRunner(UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner&& Other)
{
	TestWorld = Other.TestWorld;
	Snapshots = Other.Snapshots;
	// Prevent Other's destructor from calling RemoveFromRoot
	Other.Snapshots.Empty();
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::~FSnapshotTestRunner()
{
	for (auto SnapshotIt = Snapshots.CreateIterator(); SnapshotIt; ++SnapshotIt)
	{
		SnapshotIt->Value->RemoveFromRoot();
	}
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::ModifyWorld(TFunction<void(UWorld*)> Callback)
{
	Callback(TestWorld->GetWorld());
	return *this;
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::TakeSnapshot(FName SnapshotId)
{
	if (ULevelSnapshot** ExistingSnapshot = Snapshots.Find(SnapshotId))
	{
		(*ExistingSnapshot)->SnapshotWorld(TestWorld->GetWorld());
	}
	else
	{
		ULevelSnapshot* NewSnapshot = ULevelSnapshotsFunctionLibrary::TakeLevelSnapshot(TestWorld->GetWorld(), SnapshotId);
		// Executed tests might (indirectly) trigger a manual garbage collection 
		NewSnapshot->AddToRoot();
		
		Snapshots.Add(
			SnapshotId,
			NewSnapshot
			);
	}
	
	return *this;
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::AccessSnapshot(TFunction<void (ULevelSnapshot*)> Callback, FName SnapshotId)
{
	if (ULevelSnapshot** ExistingSnapshot = Snapshots.Find(SnapshotId))
	{
		Callback(*ExistingSnapshot);
	}
	else
	{
		checkNoEntry();
	}

	return *this;
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::AccessSnapshotAndWorld(TFunction<void(ULevelSnapshot*, UWorld*)> Callback, FName SnapshotId)
{
	return AccessSnapshot([this, &Callback](ULevelSnapshot* Snapshot)
	{
		Callback(Snapshot, TestWorld->GetWorld());
	}, SnapshotId);
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::ApplySnapshot(TFunction<ULevelSnapshotFilter*()> Callback, FName SnapshotId)
{
	return ApplySnapshot(Callback(), SnapshotId);
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::ApplySnapshot(ULevelSnapshotFilter* Filter, FName SnapshotId)
{
	if (ULevelSnapshot** ExistingSnapshot = Snapshots.Find(SnapshotId))
	{
		ULevelSnapshotsFunctionLibrary::ApplySnapshotToWorld(TestWorld->GetWorld(), *ExistingSnapshot, Filter);
	}
	else
	{
		checkNoEntry();
	}
	
	return *this;
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::ApplySnapshot(TFunction<FPropertySelectionMap()> Callback, FName SnapshotId)
{
	return ApplySnapshot(Callback(), SnapshotId);
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::ApplySnapshot(const FPropertySelectionMap& SelectionSet, FName SnapshotId)
{
	if (ULevelSnapshot** ExistingSnapshot = Snapshots.Find(SnapshotId))
	{
		(*ExistingSnapshot)->ApplySnapshotToWorld(TestWorld->GetWorld(), SelectionSet);
	}
	else
	{
		checkNoEntry();
	}
	
	return *this;
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::FilterProperties(AActor* OriginalActor, TFunction<void(const FPropertySelectionMap&)> Callback, const ULevelSnapshotFilter* Filter, FName SnapshotId)
{
	return AccessSnapshot([OriginalActor, &Callback, Filter](ULevelSnapshot* Snapshot)
	{
		const TOptional<TNonNullPtr<AActor>> SnapshotCounterpart = Snapshot->GetDeserializedActor(OriginalActor);
		if (ensure(SnapshotCounterpart))
		{
			FPropertySelectionMap SelectedProperties;
			ULevelSnapshotsFilteringLibrary::ApplyFilterToFindSelectedProperties(Snapshot, SelectedProperties, OriginalActor, SnapshotCounterpart.GetValue(), Filter);
			Callback(SelectedProperties);
		}
	}, SnapshotId);
}

UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner& UE::LevelSnapshots::Private::Tests::FSnapshotTestRunner::RunTest(TFunction<void()> Callback)
{
	Callback();
	return *this;
}
