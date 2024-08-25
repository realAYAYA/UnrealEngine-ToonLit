// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorReflection.h"
#include "InstancedStruct.h"
#include "PoseSearchDatabaseAssetTree.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearch/PoseSearchDerivedData.h"
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

void UPoseSearchDatabaseReflectionBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Skip changes during EPropertyChangeType::Interactive since they are always followed by a PostEditChangeProperty() call
	// with EPropertyChangeType::ValueSet holding the final values.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}
	
	if (ApplyChanges())
	{
		const bool bShouldRefreshView = (PropertyChangedEvent.Property == nullptr) || !PropertyChangedEvent.Property->IsA(FFloatProperty::StaticClass());
		AssetTreeWidget->FinalizeTreeChanges(true, bShouldRefreshView);
	}
}

bool UPoseSearchDatabaseSequenceReflection::ApplyChanges() 
{
	if (const TSharedPtr<UE::PoseSearch::FDatabaseAssetTreeNode> AssetTreeNode = WeakAssetTreeNode.Pin())
	{
		if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = AssetTreeNode->EditorViewModel.Pin())
		{
			UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
			if (IsValid(Database))
			{
				FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(AssetTreeNode->SourceAssetIdx);
				if (FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseSequence>())
				{
					Sequence.SamplingRange.Min = FMath::Clamp(Sequence.SamplingRange.Min, 0.0f, Sequence.GetPlayLength()); 
					Sequence.SamplingRange.Max = FMath::Clamp(Sequence.SamplingRange.Max, 0.0f, Sequence.GetPlayLength()); 
					
					*DatabaseSequence = Sequence;
					Database->MarkPackageDirty();

					return true;
				}
			}
		}
	}

	return false;
}

bool UPoseSearchDatabaseBlendSpaceReflection::ApplyChanges()
{
	if (const TSharedPtr<UE::PoseSearch::FDatabaseAssetTreeNode> AssetTreeNode = WeakAssetTreeNode.Pin())
	{
		if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = AssetTreeNode->EditorViewModel.Pin())
		{
			UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
			if (IsValid(Database))
			{
				FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(AssetTreeNode->SourceAssetIdx);
				if (FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseBlendSpace>())
				{
					*DatabaseBlendSpace = BlendSpace;
					Database->MarkPackageDirty();

					return true;
				}
			}
		}
	}

	return false;
}

bool UPoseSearchDatabaseAnimCompositeReflection::ApplyChanges()
{
	if (const TSharedPtr<UE::PoseSearch::FDatabaseAssetTreeNode> AssetTreeNode = WeakAssetTreeNode.Pin())
	{
		if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = AssetTreeNode->EditorViewModel.Pin())
		{
			UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
			if (IsValid(Database))
			{
				FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(AssetTreeNode->SourceAssetIdx);
				if (FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseAnimComposite>())
				{
					AnimComposite.SamplingRange.Min = FMath::Clamp(AnimComposite.SamplingRange.Min, 0.0f, AnimComposite.GetPlayLength());
					AnimComposite.SamplingRange.Max = FMath::Clamp(AnimComposite.SamplingRange.Max, 0.0f, AnimComposite.GetPlayLength());
					
					*DatabaseAnimComposite = AnimComposite;

					Database->MarkPackageDirty();

					return true;
				}
			}
		}
	}

	return false;
}

bool UPoseSearchDatabaseAnimMontageReflection::ApplyChanges()
{
	if (const TSharedPtr<UE::PoseSearch::FDatabaseAssetTreeNode> AssetTreeNode = WeakAssetTreeNode.Pin())
	{
		if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = AssetTreeNode->EditorViewModel.Pin())
		{
			UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
			if (IsValid(Database))
			{
				FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(AssetTreeNode->SourceAssetIdx);
				if (FPoseSearchDatabaseAnimMontage* DatabaseAnimMontage = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseAnimMontage>())
				{
					AnimMontage.SamplingRange.Min = FMath::Clamp(AnimMontage.SamplingRange.Min, 0.0f, AnimMontage.GetPlayLength()); 
					AnimMontage.SamplingRange.Max = FMath::Clamp(AnimMontage.SamplingRange.Max, 0.0f, AnimMontage.GetPlayLength());
					
					*DatabaseAnimMontage = AnimMontage;

					Database->MarkPackageDirty();

					return true;
				}
			}
		}
	}

	return false;
}

bool UPoseSearchDatabaseMultiSequenceReflection::ApplyChanges()
{
	if (const TSharedPtr<UE::PoseSearch::FDatabaseAssetTreeNode> AssetTreeNode = WeakAssetTreeNode.Pin())
	{
		if (const TSharedPtr<UE::PoseSearch::FDatabaseViewModel> ViewModel = AssetTreeNode->EditorViewModel.Pin())
		{
			UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
			if (IsValid(Database))
			{
				FInstancedStruct& DatabaseAsset = Database->GetMutableAnimationAssetStruct(AssetTreeNode->SourceAssetIdx);
				if (FPoseSearchDatabaseMultiSequence* DatabaseMultiSequence = DatabaseAsset.GetMutablePtr<FPoseSearchDatabaseMultiSequence>())
				{
					*DatabaseMultiSequence = MultiSequence;
					Database->MarkPackageDirty();

					return true;
				}
			}
		}
	}

	return false;
}

#endif // WITH_EDITOR

void UPoseSearchDatabaseStatistics::Initialize(const UPoseSearchDatabase* PoseSearchDatabase)
{
	static FText TimeFormat = LOCTEXT("TimeFormat", "{0} {0}|plural(one=Second,other=Seconds)");
	
	if (IsValid(PoseSearchDatabase))
	{
		const UE::PoseSearch::FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		// General Information
	
		AnimationSequences = PoseSearchDatabase->GetAnimationAssets().Num();
			
		const int32 SampleRate = FMath::Max(1, PoseSearchDatabase->Schema->SampleRate);
		TotalAnimationPosesInFrames = SearchIndex.GetNumPoses();
		TotalAnimationPosesInTime = FText::Format(TimeFormat, static_cast<double>(TotalAnimationPosesInFrames) / SampleRate);
			
		uint32 NumOfSearchablePoses = 0;
		for (const UE::PoseSearch::FPoseMetadata& PoseMetadata : SearchIndex.PoseMetadata)
		{
			if (!PoseMetadata.IsBlockTransition())
			{
				++NumOfSearchablePoses;
			}
		}
		SearchableFrames = NumOfSearchablePoses;
		SearchableTime = FText::Format(TimeFormat, static_cast<double>(NumOfSearchablePoses) / SampleRate);

		SchemaCardinality = PoseSearchDatabase->Schema->SchemaCardinality;

		if (SchemaCardinality > 0)
		{
			const int32 TotalAnimationFeatureVectors = SearchIndex.GetNumValuesVectors(SchemaCardinality);
			PrunedFrames = TotalAnimationPosesInFrames - TotalAnimationFeatureVectors;
		}
		else
		{
			PrunedFrames = 0;
		}

		if (PoseSearchDatabase->GetNumberOfPrincipalComponents() > 0)
		{
			const int32 TotalAnimationPCAFeatureVectors = SearchIndex.GetNumPCAValuesVectors(PoseSearchDatabase->GetNumberOfPrincipalComponents());
			PrunedPCAFrames = TotalAnimationPosesInFrames - TotalAnimationPCAFeatureVectors;
		}
		else
		{
			PrunedPCAFrames = 0;
		}

		// Kinematic Information
	
		// using FText instead of meta = (ForceUnits = "cm/s") to keep properties consistent
		AverageSpeed = FText::Format(LOCTEXT("StatsAverageSpeed", "{0} cm/s"), SearchIndex.Stats.AverageSpeed);
		MaxSpeed = FText::Format(LOCTEXT("StatsMaxSpeed", "{0} cm/s"), SearchIndex.Stats.MaxSpeed);
		AverageAcceleration = FText::Format(LOCTEXT("StatsAverageAcceleration", "{0} cm/s²"), SearchIndex.Stats.AverageAcceleration);
		MaxAcceleration = FText::Format(LOCTEXT("StatsMaxAcceleration", "{0} cm/s²"), SearchIndex.Stats.MaxAcceleration);

		// Principal Component Analysis
			
		ExplainedVariance = SearchIndex.PCAExplainedVariance * 100.f;
			
		// Memory Information
			
		{
			uint32 SourceAnimAssetsSizeCookedEstimateInBytes = 0;
			TSet<const UObject*> Analyzed;
			Analyzed.Reserve(PoseSearchDatabase->GetAnimationAssets().Num());
			for (const FInstancedStruct& AnimAsset : PoseSearchDatabase->GetAnimationAssets())
			{
				if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = AnimAsset.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
				{
					bool bAlreadyAnalyzed = false;
					Analyzed.Add(DatabaseAnimationAssetBase->GetAnimationAsset(), &bAlreadyAnalyzed);
					if (!bAlreadyAnalyzed && DatabaseAnimationAssetBase->GetAnimationAsset())
					{
						SourceAnimAssetsSizeCookedEstimateInBytes += DatabaseAnimationAssetBase->GetApproxCookedSize();
					}
				}
			}

			const uint32 ValuesBytesSize = SearchIndex.Values.GetAllocatedSize();
			const uint32 PCAValuesBytesSize = SearchIndex.PCAValues.GetAllocatedSize();
			const uint32 KDTreeBytesSize = SearchIndex.KDTree.GetAllocatedSize();
			const uint32 VPTreeBytesSize = SearchIndex.VPTree.GetAllocatedSize();
			const uint32 ValuesVectorToPoseIndexesBytesSize = SearchIndex.ValuesVectorToPoseIndexes.GetAllocatedSize();
			const uint32 PCAValuesVectorToPoseIndexesBytesSize = SearchIndex.PCAValuesVectorToPoseIndexes.GetAllocatedSize();

			const uint32 PoseMetadataBytesSize = SearchIndex.PoseMetadata.GetAllocatedSize();
			const uint32 AssetsBytesSize = SearchIndex.Assets.GetAllocatedSize();
			const uint32 OtherBytesSize = SearchIndex.PCAProjectionMatrix.GetAllocatedSize() + SearchIndex.Mean.GetAllocatedSize() + SearchIndex.WeightsSqrt.GetAllocatedSize();
			const uint32 EstimatedDatabaseBytesSize = ValuesBytesSize + PCAValuesBytesSize + KDTreeBytesSize + VPTreeBytesSize + ValuesVectorToPoseIndexesBytesSize + PCAValuesVectorToPoseIndexesBytesSize + PoseMetadataBytesSize + AssetsBytesSize + OtherBytesSize + SourceAnimAssetsSizeCookedEstimateInBytes;
				
			ValuesSize = FText::AsMemory(ValuesBytesSize);
			PCAValuesSize = FText::AsMemory(PCAValuesBytesSize);
			KDTreeSize = FText::AsMemory(KDTreeBytesSize);
			VPTreeSize = FText::AsMemory(VPTreeBytesSize);
			PoseMetadataSize = FText::AsMemory(PoseMetadataBytesSize);
			AssetsSize = FText::AsMemory(AssetsBytesSize);
			EstimatedDatabaseSize = FText::AsMemory(EstimatedDatabaseBytesSize);
			SourceAnimAssetsSizeCookedEstimate = FText::AsMemory(SourceAnimAssetsSizeCookedEstimateInBytes);
		}
	}
}

#undef LOCTEXT_NAMESPACE
