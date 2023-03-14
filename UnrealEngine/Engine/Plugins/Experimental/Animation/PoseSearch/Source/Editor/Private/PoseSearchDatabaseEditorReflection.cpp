// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorReflection.h"
#include "SPoseSearchDatabaseAssetList.h"
#include "PoseSearchDatabaseViewModel.h"

#define LOCTEXT_NAMESPACE "UPoseSearchDatabaseReflection"

#if WITH_EDITOR

void UPoseSearchDatabaseReflectionBase::SetSourceLink(
	const TWeakPtr<UE::PoseSearch::FDatabaseAssetTreeNode>& InWeakAssetTreeNode,
	const TSharedPtr<UE::PoseSearch::SDatabaseAssetTree>& InAssetTreeWidget)
{
	WeakAssetTreeNode = InWeakAssetTreeNode;
	AssetTreeWidget = InAssetTreeWidget;
}

void UPoseSearchDatabaseSequenceReflection::PostEditChangeProperty(
	struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	check(WeakAssetTreeNode.Pin()->SourceAssetType == ESearchIndexAssetType::Sequence);

	UPoseSearchDatabase* Database = WeakAssetTreeNode.Pin()->EditorViewModel.Pin()->GetPoseSearchDatabase();
	if (IsValid(Database))
	{
		Database->Sequences[WeakAssetTreeNode.Pin()->SourceAssetIdx] = Sequence;
		AssetTreeWidget->FinalizeTreeChanges(true);
	}
}

void UPoseSearchDatabaseBlendSpaceReflection::PostEditChangeProperty(
	struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	check(WeakAssetTreeNode.Pin()->SourceAssetType == ESearchIndexAssetType::BlendSpace);

	UPoseSearchDatabase* Database = WeakAssetTreeNode.Pin()->EditorViewModel.Pin()->GetPoseSearchDatabase();
	if (IsValid(Database))
	{
		Database->BlendSpaces[WeakAssetTreeNode.Pin()->SourceAssetIdx] = BlendSpace;
		AssetTreeWidget->FinalizeTreeChanges(true);
	}
}

#endif // WITH_EDITOR

FText FPoseSearchDatabaseMemoryStats::ToMemoryBudgetText(int32 Size)
{
	if (Size < 1024)
	{
		return FText::Format(LOCTEXT("FPoseSearchDatabaseMemoryStats_Bytes", "{0} Bytes"), Size);
	}

	if (Size < 1024 * 1024)
	{
		return FText::Format(LOCTEXT("FPoseSearchDatabaseMemoryStats_KiloBytes", "{0} Kb"), Size / float(1024));
	}

	return FText::Format(LOCTEXT("FPoseSearchDatabaseMemoryStats_MegaBytes", "{0} Mb"), Size / float(1024 * 1024));
}

void FPoseSearchDatabaseMemoryStats::Initialize(const UPoseSearchDatabase* PoseSearchDatabase)
{
	const FPoseSearchIndex* SearchIndexSafe = PoseSearchDatabase->GetSearchIndexSafe();
	check(SearchIndexSafe);

	const int32 ValuesBytesSize = SearchIndexSafe->Values.GetAllocatedSize();
	const int32 PCAValuesBytesSize = SearchIndexSafe->PCAValues.GetAllocatedSize();
	const int32 KDTreeBytesSize = SearchIndexSafe->KDTree.GetAllocatedSize();
	const int32 PoseMetadataBytesSize = SearchIndexSafe->PoseMetadata.GetAllocatedSize();
	const int32 AssetsBytesSize = SearchIndexSafe->Assets.GetAllocatedSize();

	const int32 OtherBytesSize = SearchIndexSafe->PCAProjectionMatrix.GetAllocatedSize() +
							SearchIndexSafe->Mean.GetAllocatedSize() +
							SearchIndexSafe->WeightsSqrt.GetAllocatedSize();
	
	const int32 EstimatedDatabaseBytesSize = ValuesBytesSize + PCAValuesBytesSize + KDTreeBytesSize + PoseMetadataBytesSize + AssetsBytesSize + OtherBytesSize;

	ValuesSize = ToMemoryBudgetText(ValuesBytesSize);
	PCAValuesSize = ToMemoryBudgetText(PCAValuesBytesSize);
	KDTreeSize = ToMemoryBudgetText(KDTreeBytesSize);
	PoseMetadataSize = ToMemoryBudgetText(PoseMetadataBytesSize);
	AssetsSize = ToMemoryBudgetText(AssetsBytesSize);
	EstimatedDatabaseSize = ToMemoryBudgetText(EstimatedDatabaseBytesSize);
}

void UPoseSearchDatabaseReflection::Initialize(const UPoseSearchDatabase* PoseSearchDatabase)
{
	if (PoseSearchDatabase)
	{
		const FPoseSearchIndex* SearchIndexSafe = PoseSearchDatabase->GetSearchIndexSafe();
		if (SearchIndexSafe)
		{
			SearchIndex = *SearchIndexSafe;
			MemoryStats.Initialize(PoseSearchDatabase);
		}
		else
		{
			SearchIndex = FPoseSearchIndex();
			MemoryStats = FPoseSearchDatabaseMemoryStats();
		}
	}
}

#undef LOCTEXT_NAMESPACE
