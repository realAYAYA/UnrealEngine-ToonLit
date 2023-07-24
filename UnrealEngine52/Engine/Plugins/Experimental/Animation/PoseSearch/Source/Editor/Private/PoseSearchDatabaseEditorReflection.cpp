// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorReflection.h"
#include "InstancedStruct.h"
#include "PoseSearchDatabaseAssetTree.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearch/PoseSearchSchema.h"

#define LOCTEXT_NAMESPACE "UPoseSearchDatabaseReflection"

#if WITH_EDITOR

void UPoseSearchDatabaseReflectionBase::SetSourceLink(
	const TWeakPtr<UE::PoseSearch::FDatabaseAssetTreeNode>& InWeakAssetTreeNode,
	const TSharedPtr<UE::PoseSearch::SDatabaseAssetTree>& InAssetTreeWidget)
{
	WeakAssetTreeNode = InWeakAssetTreeNode;
	AssetTreeWidget = InAssetTreeWidget;
}

void UPoseSearchDatabaseSequenceReflection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	check(WeakAssetTreeNode.Pin()->SourceAssetType == ESearchIndexAssetType::Sequence);

	if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = WeakAssetTreeNode.Pin()->EditorViewModel.Pin())
	{
		UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
		if (IsValid(Database))
		{
			FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(WeakAssetTreeNode.Pin()->SourceAssetIdx);
			if (FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseSequence>())
			{
				*DatabaseSequence = Sequence;
				Database->MarkPackageDirty();
		
				AssetTreeWidget->FinalizeTreeChanges(true);
			}
		}
	}
}

void UPoseSearchDatabaseBlendSpaceReflection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	check(WeakAssetTreeNode.Pin()->SourceAssetType == ESearchIndexAssetType::BlendSpace);

	if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = WeakAssetTreeNode.Pin()->EditorViewModel.Pin())
	{
		UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
		if (IsValid(Database))
		{
			FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(WeakAssetTreeNode.Pin()->SourceAssetIdx);
			if (FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseBlendSpace>())
			{
				*DatabaseBlendSpace = BlendSpace;
				Database->MarkPackageDirty();

				AssetTreeWidget->FinalizeTreeChanges(true);
			}
		}
	}
}

void UPoseSearchDatabaseAnimCompositeReflection::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	check(WeakAssetTreeNode.Pin()->SourceAssetType == ESearchIndexAssetType::AnimComposite);

	if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = WeakAssetTreeNode.Pin()->EditorViewModel.Pin())
	{
		UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
		if (IsValid(Database))
		{
			FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(WeakAssetTreeNode.Pin()->SourceAssetIdx);
			if (FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseAnimComposite>())
			{
				*DatabaseAnimComposite = AnimComposite;
				Database->MarkPackageDirty();

				AssetTreeWidget->FinalizeTreeChanges(true);
			}
		}
	}
}

#endif // WITH_EDITOR

void UPoseSearchDatabaseStatistics::Initialize(const UPoseSearchDatabase* PoseSearchDatabase)
{
	static FText TimeFormat = LOCTEXT("TimeFormat", "{0} {0}|plural(one=Second,other=Seconds)");
	
	if (PoseSearchDatabase)
	{
		const FPoseSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		// General information
	
		AnimationSequences = PoseSearchDatabase->AnimationAssets.Num();
			
		const int32 SampleRate = FMath::Max(1, PoseSearchDatabase->Schema->SampleRate);
		TotalAnimationPosesInFrames = SearchIndex.NumPoses;
		TotalAnimationPosesInTime = FText::Format(TimeFormat, static_cast<double>(SearchIndex.NumPoses) / SampleRate);
			
		{
			uint32 NumOfSearchablePoses = 0;
			for (const FPoseSearchPoseMetadata & PoseMetadata : SearchIndex.PoseMetadata)
			{
				NumOfSearchablePoses += PoseMetadata.Flags != EPoseSearchPoseFlags::BlockTransition;
			}
				
			SearchableFrames = NumOfSearchablePoses;
			SearchableTime = FText::Format(TimeFormat, static_cast<double>(NumOfSearchablePoses) / SampleRate);
		}
			
		// Velocity information
	
		// TODO: Set values once they can be queried from the PoseSearchIndex.
			
		// Principal Component Analysis
			
		ExplainedVariance = SearchIndex.PCAExplainedVariance;
			
		// Memory information
			
		{
			const uint32 ValuesBytesSize = SearchIndex.Values.GetAllocatedSize();
			const uint32 PCAValuesBytesSize = SearchIndex.PCAValues.GetAllocatedSize();
			const uint32 KDTreeBytesSize = SearchIndex.KDTree.GetAllocatedSize();
			const uint32 PoseMetadataBytesSize = SearchIndex.PoseMetadata.GetAllocatedSize();
			const uint32 AssetsBytesSize = SearchIndex.Assets.GetAllocatedSize();
			const uint32 OtherBytesSize = SearchIndex.PCAProjectionMatrix.GetAllocatedSize() + SearchIndex.Mean.GetAllocatedSize() + SearchIndex.WeightsSqrt.GetAllocatedSize();
			const uint32 EstimatedDatabaseBytesSize = ValuesBytesSize + PCAValuesBytesSize + KDTreeBytesSize + PoseMetadataBytesSize + AssetsBytesSize + OtherBytesSize;
				
			ValuesSize = FText::AsMemory(ValuesBytesSize);
			PCAValuesSize = FText::AsMemory(PCAValuesBytesSize);
			KDTreeSize = FText::AsMemory(KDTreeBytesSize);
			PoseMetadataSize = FText::AsMemory(PoseMetadataBytesSize);
			AssetsSize = FText::AsMemory(AssetsBytesSize);
			EstimatedDatabaseSize = FText::AsMemory(EstimatedDatabaseBytesSize);
		}
	}
}

#undef LOCTEXT_NAMESPACE
