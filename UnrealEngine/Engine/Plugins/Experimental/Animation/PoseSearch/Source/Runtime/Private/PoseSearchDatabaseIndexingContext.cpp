// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearchDatabaseIndexingContext.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/MirrorDataTable.h"
#include "DerivedDataRequestOwner.h"
#include "InstancedStruct.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// FAssetSamplingContext
void FAssetSamplingContext::Init(const UMirrorDataTable* InMirrorDataTable, const FBoneContainer& BoneContainer)
{
	MirrorDataTable = InMirrorDataTable;

	if (InMirrorDataTable)
	{
		InMirrorDataTable->FillCompactPoseAndComponentRefRotations(BoneContainer, CompactPoseMirrorBones, ComponentSpaceRefRotations);
	}
	else
	{
		CompactPoseMirrorBones.Reset();
		ComponentSpaceRefRotations.Reset();
	}
}

FTransform FAssetSamplingContext::MirrorTransform(const FTransform& InTransform) const
{
	return UE::PoseSearch::MirrorTransform(InTransform, MirrorDataTable->MirrorAxis, ComponentSpaceRefRotations[FCompactPoseBoneIndex(RootBoneIndexType)]);
}

//////////////////////////////////////////////////////////////////////////
// FDatabaseIndexingContext
bool FDatabaseIndexingContext::IndexDatabase(FSearchIndexBase& SearchIndexBase, const UPoseSearchDatabase& Database, UE::DerivedData::FRequestOwner& Owner)
{
	const UPoseSearchSchema* Schema = Database.Schema;
	check(Schema);

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(Schema->BoneIndicesWithParents, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *Schema->Skeleton);

	SamplingContext.Init(Schema->MirrorDataTable, BoneContainer);

	if (Owner.IsCanceled())
	{
		return false;
	}

	// Prepare samplers for all animation assets.
	Samplers.Reset();
	TMap<TPair<const UAnimationAsset*, FVector>, int32> SamplerMap;
	for (const FInstancedStruct& DatabaseAssetStruct : Database.AnimationAssets)
	{
		if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			if (DatabaseBlendSpace->BlendSpace)
			{
				int32 HorizontalBlendNum, VerticalBlendNum;
				DatabaseBlendSpace->GetBlendSpaceParameterSampleRanges(HorizontalBlendNum, VerticalBlendNum);

				for (int32 HorizontalIndex = 0; HorizontalIndex < HorizontalBlendNum; HorizontalIndex++)
				{
					for (int32 VerticalIndex = 0; VerticalIndex < VerticalBlendNum; VerticalIndex++)
					{
						const FVector BlendParameters = DatabaseBlendSpace->BlendParameterForSampleRanges(HorizontalIndex, VerticalIndex);

						if (!SamplerMap.Contains({ DatabaseBlendSpace->BlendSpace, BlendParameters }))
						{
							SamplerMap.Add({ DatabaseBlendSpace->BlendSpace, BlendParameters }, Samplers.Num());
							Samplers.Emplace(DatabaseBlendSpace->BlendSpace, BlendParameters);
						}
					}
				}
			}
		}
		else if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseAnimationAssetBase>())
		{
			if (const UAnimationAsset* AnimationAsset = DatabaseAnimationAssetBase->GetAnimationAsset())
			{
				if (!SamplerMap.Contains({ AnimationAsset, FVector::ZeroVector }))
				{
					SamplerMap.Add({ AnimationAsset, FVector::ZeroVector }, Samplers.Num());
					Samplers.Emplace(AnimationAsset);
				}
			}
		}
	}

	// capturing BoneContainer by copy, since it has mutable properties
	ParallelFor(Samplers.Num(), [this, BoneContainer](int32 SamplerIdx) { Samplers[SamplerIdx].Process(BoneContainer); }, ParallelForFlags);

	if (Owner.IsCanceled())
	{
		return false;
	}

	// prepare indexers
	Indexers.Reserve(SearchIndexBase.Assets.Num());

	int32 TotalPoses = 0;
	for (int32 AssetIdx = 0; AssetIdx != SearchIndexBase.Assets.Num(); ++AssetIdx)
	{
		FSearchIndexAsset& SearchIndexAsset = SearchIndexBase.Assets[AssetIdx];
		check(SearchIndexAsset.FirstPoseIdx == TotalPoses);

		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database.GetAnimationAssetStruct(SearchIndexAsset).GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
		check(DatabaseAnimationAssetBase && DatabaseAnimationAssetBase->GetAnimationAsset());
		const FAnimationAssetSampler& AssetSampler = Samplers[SamplerMap[{ DatabaseAnimationAssetBase->GetAnimationAsset(), SearchIndexAsset.BlendParameters }]];

		Indexers.Emplace(BoneContainer, SearchIndexAsset, SamplingContext, *Schema, AssetSampler);
		TotalPoses += SearchIndexAsset.GetNumPoses();
	}

	// allocating Values and PoseMetadata
	SearchIndexBase.Values.Reset();
	SearchIndexBase.PoseMetadata.Reset();

	SearchIndexBase.Values.SetNumZeroed(Schema->SchemaCardinality * TotalPoses);
	SearchIndexBase.PoseMetadata.SetNumZeroed(TotalPoses);

	// assigning local data to each Indexer
	TotalPoses = 0;
	for (int32 AssetIdx = 0; AssetIdx != SearchIndexBase.Assets.Num(); ++AssetIdx)
	{
		const int32 NumIndexedPoses = Indexers[AssetIdx].GetNumIndexedPoses();
		Indexers[AssetIdx].AssignWorkingData(
			MakeArrayView(SearchIndexBase.Values.GetData() + Schema->SchemaCardinality * TotalPoses, Schema->SchemaCardinality * NumIndexedPoses),
			MakeArrayView(SearchIndexBase.PoseMetadata.GetData() + TotalPoses, NumIndexedPoses));
		TotalPoses += NumIndexedPoses;
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	// Index asset data
	ParallelFor(Indexers.Num(), [this](int32 AssetIdx) { Indexers[AssetIdx].Process(AssetIdx); }, ParallelForFlags);

	if (Owner.IsCanceled())
	{
		return false;
	}

	// Joining Metadata.Flags into OverallFlags
	SearchIndexBase.bAnyBlockTransition = false;
	for (const FPoseMetadata& Metadata : SearchIndexBase.PoseMetadata)
	{
		if (Metadata.IsBlockTransition())
		{
			SearchIndexBase.bAnyBlockTransition = true;
			break;
		}
	}

	// Joining Stats
	int32 NumAccumulatedSamples = 0;
	SearchIndexBase.Stats = FSearchStats();
	for (int32 AssetIdx = 0; AssetIdx != SearchIndexBase.Assets.Num(); ++AssetIdx)
	{
		const FAssetIndexer::FStats& Stats = Indexers[AssetIdx].GetStats();
		SearchIndexBase.Stats.AverageSpeed += Stats.AccumulatedSpeed;
		SearchIndexBase.Stats.MaxSpeed = FMath::Max(SearchIndexBase.Stats.MaxSpeed, Stats.MaxSpeed);
		SearchIndexBase.Stats.AverageAcceleration += Stats.AccumulatedAcceleration;
		SearchIndexBase.Stats.MaxAcceleration = FMath::Max(SearchIndexBase.Stats.MaxAcceleration, Stats.MaxAcceleration);

		NumAccumulatedSamples += Stats.NumAccumulatedSamples;
	}

	if (NumAccumulatedSamples > 0)
	{
		const float Denom = 1.f / float(NumAccumulatedSamples);
		SearchIndexBase.Stats.AverageSpeed *= Denom;
		SearchIndexBase.Stats.AverageAcceleration *= Denom;
	}

	// Calculate Min Cost Addend
	SearchIndexBase.MinCostAddend = 0.f;
	if (!SearchIndexBase.PoseMetadata.IsEmpty())
	{
		SearchIndexBase.MinCostAddend = MAX_FLT;
		for (const FPoseMetadata& PoseMetadata : SearchIndexBase.PoseMetadata)
		{
			if (PoseMetadata.GetCostAddend() < SearchIndexBase.MinCostAddend)
			{
				SearchIndexBase.MinCostAddend = PoseMetadata.GetCostAddend();
			}
		}
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	return true;
}

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
