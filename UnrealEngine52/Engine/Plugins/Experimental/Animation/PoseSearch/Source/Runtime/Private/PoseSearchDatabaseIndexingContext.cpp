// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseIndexingContext.h"

#if WITH_EDITOR

#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "InstancedStruct.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{

void FDatabaseIndexingContext::Prepare(const UPoseSearchDatabase* Database)
{
	const UPoseSearchSchema* Schema = Database->Schema;
	check(Schema);

	FBoneContainer BoneContainer;
	BoneContainer.InitializeTo(Schema->BoneIndicesWithParents, FCurveEvaluationOption(false), *Schema->Skeleton);

	TMap<const UAnimSequenceBase*, int32> SequenceSamplerMap;
	TMap<TPair<const UBlendSpace*, FVector>, int32> BlendSpaceSamplerMap;

	SamplingContext.Init(Schema->MirrorDataTable, BoneContainer);

	// Prepare samplers for all animation assets.
	for (const FInstancedStruct& DatabaseAssetStruct : Database->AnimationAssets)
	{
		auto AddSequenceBaseSampler = [&](const UAnimSequenceBase* Sequence)
		{
			if (Sequence && !SequenceSamplerMap.Contains(Sequence))
			{
				int32 SequenceSamplerIdx = SequenceSamplers.AddDefaulted();
				SequenceSamplerMap.Add(Sequence, SequenceSamplerIdx);

				FSequenceBaseSampler::FInput Input;
				Input.ExtrapolationParameters = Database->ExtrapolationParameters;
				Input.SequenceBase = Sequence;
				SequenceSamplers[SequenceSamplerIdx].Init(Input);
			}
		};

		if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseSequence>())
		{
			AddSequenceBaseSampler(DatabaseSequence->Sequence);
		}
		else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseAnimComposite>())
		{
			AddSequenceBaseSampler(DatabaseAnimComposite->AnimComposite);
		}
		else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAssetStruct.GetPtr<FPoseSearchDatabaseBlendSpace>())
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

						if (!BlendSpaceSamplerMap.Contains({ DatabaseBlendSpace->BlendSpace, BlendParameters }))
						{
							int32 BlendSpaceSamplerIdx = BlendSpaceSamplers.AddDefaulted();
							BlendSpaceSamplerMap.Add({ DatabaseBlendSpace->BlendSpace, BlendParameters }, BlendSpaceSamplerIdx);

							FBlendSpaceSampler::FInput Input;
							Input.BoneContainer = BoneContainer;
							Input.ExtrapolationParameters = Database->ExtrapolationParameters;
							Input.BlendSpace = DatabaseBlendSpace->BlendSpace;
							Input.BlendParameters = BlendParameters;

							BlendSpaceSamplers[BlendSpaceSamplerIdx].Init(Input);
						}
					}
				}
			}
		}
	}

	TArray<IAssetSampler*, TInlineAllocator<512>> AssetSampler;
	AssetSampler.SetNumUninitialized(SequenceSamplers.Num() + BlendSpaceSamplers.Num());

	for (int i = 0; i < SequenceSamplers.Num(); ++i)
	{
		AssetSampler[i] = &SequenceSamplers[i];
	}
	for (int i = 0; i < BlendSpaceSamplers.Num(); ++i)
	{
		AssetSampler[i + SequenceSamplers.Num()] = &BlendSpaceSamplers[i];
	}

	ParallelFor(AssetSampler.Num(), [AssetSampler](int32 SamplerIdx) { AssetSampler[SamplerIdx]->Process(); }, ParallelForFlags);

	// prepare indexers
	Indexers.Reserve(SearchIndexBase->Assets.Num());

	auto GetSequenceBaseSampler = [&](const UAnimSequenceBase* Sequence) -> const FSequenceBaseSampler*
	{
		return Sequence ? &SequenceSamplers[SequenceSamplerMap[Sequence]] : nullptr;
	};

	auto GetBlendSpaceSampler = [&](const UBlendSpace* BlendSpace, const FVector BlendParameters) -> const FBlendSpaceSampler*
	{
		return BlendSpace ? &BlendSpaceSamplers[BlendSpaceSamplerMap[{BlendSpace, BlendParameters}]] : nullptr;
	};

	Indexers.Reserve(SearchIndexBase->Assets.Num());

	for (int32 AssetIdx = 0; AssetIdx != SearchIndexBase->Assets.Num(); ++AssetIdx)
	{
		const FPoseSearchIndexAsset& SearchIndexAsset = SearchIndexBase->Assets[AssetIdx];

		FAssetIndexingContext IndexerContext;
		IndexerContext.SamplingContext = &SamplingContext;
		IndexerContext.Schema = Schema;
		IndexerContext.RequestedSamplingRange = SearchIndexAsset.SamplingInterval;
		IndexerContext.bMirrored = SearchIndexAsset.bMirrored;

		const FInstancedStruct& DatabaseAsset = Database->GetAnimationAssetStruct(SearchIndexAsset.SourceAssetIdx);
		if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>())
		{
			const float SequenceLength = DatabaseSequence->Sequence->GetPlayLength();
			IndexerContext.AssetSampler = GetSequenceBaseSampler(DatabaseSequence->Sequence);
		}
		else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
		{
			IndexerContext.AssetSampler = GetSequenceBaseSampler(DatabaseAnimComposite->AnimComposite);
		}
		else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			IndexerContext.AssetSampler = GetBlendSpaceSampler(DatabaseBlendSpace->BlendSpace, SearchIndexAsset.BlendParameters);
		}

		FAssetIndexer& Indexer = Indexers.AddDefaulted_GetRef();
		Indexer.Init(IndexerContext, BoneContainer);
	}
}

bool FDatabaseIndexingContext::IndexAssets()
{
	// Index asset data
	ParallelFor(Indexers.Num(), [this](int32 AssetIdx) { Indexers[AssetIdx].Process(); }, ParallelForFlags);
	return true;
}

float FDatabaseIndexingContext::CalculateMinCostAddend() const
{
	float MinCostAddend = 0.f;

	check(SearchIndexBase);
	if (!SearchIndexBase->PoseMetadata.IsEmpty())
	{
		MinCostAddend = MAX_FLT;
		for (const FPoseSearchPoseMetadata& PoseMetadata : SearchIndexBase->PoseMetadata)
		{
			if (PoseMetadata.CostAddend < MinCostAddend)
			{
				MinCostAddend = PoseMetadata.CostAddend;
			}
		}
	}
	return MinCostAddend;
}

void FDatabaseIndexingContext::JoinIndex()
{
	// Write index info to asset and count up total poses and storage required
	int32 TotalPoses = 0;
	int32 TotalFloats = 0;

	check(SearchIndexBase);

	// Join animation data into a single search index
	SearchIndexBase->Values.Reset();
	SearchIndexBase->PoseMetadata.Reset();
	SearchIndexBase->OverallFlags = EPoseSearchPoseFlags::None;

	for (int32 AssetIdx = 0; AssetIdx != SearchIndexBase->Assets.Num(); ++AssetIdx)
	{
		const FAssetIndexer::FOutput& Output = Indexers[AssetIdx].Output;

		FPoseSearchIndexAsset& SearchIndexAsset = SearchIndexBase->Assets[AssetIdx];
		SearchIndexAsset.NumPoses = Output.NumIndexedPoses;
		SearchIndexAsset.FirstPoseIdx = TotalPoses;

		const int32 PoseMetadataStartIdx = SearchIndexBase->PoseMetadata.Num();
		const int32 PoseMetadataEndIdx = PoseMetadataStartIdx + Output.PoseMetadata.Num();

		SearchIndexBase->Values.Append(Output.FeatureVectorTable.GetData(), Output.FeatureVectorTable.Num());
		SearchIndexBase->PoseMetadata.Append(Output.PoseMetadata);

		for (int32 i = PoseMetadataStartIdx; i < PoseMetadataEndIdx; ++i)
		{
			SearchIndexBase->PoseMetadata[i].AssetIndex = AssetIdx;
			SearchIndexBase->OverallFlags |= SearchIndexBase->PoseMetadata[i].Flags;
		}

		TotalPoses += Output.NumIndexedPoses;
		TotalFloats += Output.FeatureVectorTable.Num();
	}

	SearchIndexBase->NumPoses = TotalPoses;
	SearchIndexBase->MinCostAddend = CalculateMinCostAddend();
}

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
