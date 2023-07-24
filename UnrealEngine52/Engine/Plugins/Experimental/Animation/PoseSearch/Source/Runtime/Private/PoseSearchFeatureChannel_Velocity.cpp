// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_Velocity.h"
#include "Animation/MotionTrajectoryTypes.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"

void UPoseSearchFeatureChannel_Velocity::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = UE::PoseSearch::FFeatureVectorHelper::EncodeVectorCardinality;
	Schema->SchemaCardinality += ChannelCardinality;

	SchemaBoneIdx = Schema->AddBoneReference(Bone);
}

void UPoseSearchFeatureChannel_Velocity::FillWeights(TArray<float>& Weights) const
{
	using namespace UE::PoseSearch;

	for (int32 i = 0; i != FFeatureVectorHelper::EncodeVectorCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = Weight;
	}
}

void UPoseSearchFeatureChannel_Velocity::IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const
{
	using namespace UE::PoseSearch;

	const FAssetIndexingContext& IndexingContext = Indexer.GetIndexingContext();
	const FAssetSamplingContext* SamplingContext = IndexingContext.SamplingContext;
	check(SamplingContext->FiniteDelta > UE_KINDA_SMALL_NUMBER);

	for (int32 SampleIdx = IndexingContext.BeginSampleIdx; SampleIdx != IndexingContext.EndSampleIdx; ++SampleIdx)
	{
		const int32 VectorIdx = SampleIdx - IndexingContext.BeginSampleIdx;

		const float OriginSampleTime = FMath::Min(SampleIdx * IndexingContext.Schema->GetSamplingInterval(), IndexingContext.AssetSampler->GetPlayLength());
		const float SubsampleTime = OriginSampleTime + SampleTimeOffset;

		bool ClampedPast, ClampedPresent, ClampedFuture;
		const FTransform BoneTransformsPast = Indexer.GetTransformAndCacheResults(SubsampleTime - SamplingContext->FiniteDelta, bUseCharacterSpaceVelocities ? OriginSampleTime - SamplingContext->FiniteDelta : OriginSampleTime, SchemaBoneIdx, ClampedPast);
		const FTransform BoneTransformsPresent = Indexer.GetTransformAndCacheResults(SubsampleTime, OriginSampleTime, SchemaBoneIdx, ClampedPresent);
		const FTransform BoneTransformsFuture = Indexer.GetTransformAndCacheResults(SubsampleTime + SamplingContext->FiniteDelta, bUseCharacterSpaceVelocities ? OriginSampleTime + SamplingContext->FiniteDelta : OriginSampleTime, SchemaBoneIdx, ClampedFuture);

		// We can get a better finite difference if we ignore samples that have
		// been clamped at either side of the clip. However, if the central sample 
		// itself is clamped, or there are no samples that are clamped, we can just 
		// use the central difference as normal.
		FVector LinearVelocity;
		if (ClampedPast && !ClampedPresent && !ClampedFuture)
		{
			LinearVelocity = (BoneTransformsFuture.GetTranslation() - BoneTransformsPresent.GetTranslation()) / SamplingContext->FiniteDelta;
		}
		else if (ClampedFuture && !ClampedPresent && !ClampedPast)
		{
			LinearVelocity = (BoneTransformsPresent.GetTranslation() - BoneTransformsPast.GetTranslation()) / SamplingContext->FiniteDelta;
		}
		else
		{
			LinearVelocity = (BoneTransformsFuture.GetTranslation() - BoneTransformsPast.GetTranslation()) / (SamplingContext->FiniteDelta * 2.f);
		}

		if (bNormalize)
		{
			LinearVelocity = LinearVelocity.GetClampedToMaxSize(1.f);
		}

		int32 DataOffset = ChannelDataOffset;
		FFeatureVectorHelper::EncodeVector(IndexingContext.GetPoseVector(VectorIdx, FeatureVectorTable), DataOffset, LinearVelocity);
	}
}

void UPoseSearchFeatureChannel_Velocity::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	const bool bIsCurrentResultValid = SearchContext.CurrentResult.IsValid();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid && SearchContext.CurrentResult.Database->Schema == InOutQuery.GetSchema();
	if (bSkip || !SearchContext.History)
	{
		if (bIsCurrentResultValid)
		{
			const float LerpValue = InputQueryPose == EInputQueryPose::UseInterpolatedContinuingPose ? SearchContext.CurrentResult.LerpValue : 0.f;
			int32 DataOffset = ChannelDataOffset;
			FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), DataOffset, SearchContext.GetCurrentResultPrevPoseVector(), SearchContext.GetCurrentResultPoseVector(), SearchContext.GetCurrentResultNextPoseVector(), LerpValue);
		}
		// else leave the InOutQuery set to zero since the SearchContext.History is invalid and it'll fail if we continue
	}
	else
	{
		FVector LinearVelocity;
		if (InOutQuery.GetSchema()->BoneReferences[SchemaBoneIdx].HasValidSetup())
		{
			check(SearchContext.History);
			const float HistorySameplInterval = SearchContext.History->GetSampleTimeInterval();
			check(HistorySameplInterval > UE_KINDA_SMALL_NUMBER);

			// @todo: optimize this code for SchemaBoneIdx == FSearchContext::SchemaRootBoneIdx (!InOutQuery.GetSchema()->BoneReferences[SchemaBoneIdx].HasValidSetup())
			// calculating the Transforms in component space for the bone indexed by SchemaBoneIdx
			FTransform TransformCurrent = SearchContext.TryGetTransformAndCacheResults(SampleTimeOffset, InOutQuery.GetSchema(), SchemaBoneIdx);
			FTransform TransformPrevious = SearchContext.TryGetTransformAndCacheResults(SampleTimeOffset - HistorySameplInterval, InOutQuery.GetSchema(), SchemaBoneIdx);

			// if we want the velocity not in character space we need to calculate the root offset delta between the SampleTimeOffset and the SampleTimeOffset - HistorySameplInterval to apply to the TransformPrevious
			if (!bUseCharacterSpaceVelocities)
			{
				const FTransform RootTransform = SearchContext.TryGetTransformAndCacheResults(SampleTimeOffset, InOutQuery.GetSchema(), FSearchContext::SchemaRootBoneIdx);
				const FTransform RootTransformPrev = SearchContext.TryGetTransformAndCacheResults(SampleTimeOffset - HistorySameplInterval, InOutQuery.GetSchema(), FSearchContext::SchemaRootBoneIdx);
				TransformPrevious = TransformPrevious * (RootTransformPrev * RootTransform.Inverse());
			}

			LinearVelocity = (TransformCurrent.GetTranslation() - TransformPrevious.GetTranslation()) / HistorySameplInterval;
		}
		else
		{
			check(SearchContext.Trajectory);
			// @todo: make this call consistent with Transform = SearchContext.TryGetTransformAndCacheResults(SampleTimeOffset, InOutQuery.GetSchema(), FSearchContext::SchemaRootBoneIdx);
			const FTrajectorySample TrajectorySample = SearchContext.Trajectory->GetSampleAtTime(SampleTimeOffset);
			LinearVelocity = TrajectorySample.LinearVelocity;
		}

		if (bNormalize)
		{
			LinearVelocity = LinearVelocity.GetClampedToMaxSize(1.f);
		}

		int32 DataOffset = ChannelDataOffset;
		FFeatureVectorHelper::EncodeVector(InOutQuery.EditValues(), DataOffset, LinearVelocity);
	}
}

void UPoseSearchFeatureChannel_Velocity::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
#if ENABLE_DRAW_DEBUG
	using namespace UE::PoseSearch;

	const float LifeTime = DrawParams.DefaultLifeTime;
	const uint8 DepthPriority = ESceneDepthPriorityGroup::SDPG_Foreground + 2;
	const bool bPersistent = EnumHasAnyFlags(DrawParams.Flags, EDebugDrawFlags::Persistent);
	const FColor Color = DrawParams.GetColor(ColorPresetIndex);
	const float LinearVelocityScale = bNormalize ? 15.f : 0.08f;

	const FVector LinearVelocity = DrawParams.RootTransform.TransformVector(FFeatureVectorHelper::DecodeVectorAtOffset(PoseVector, ChannelDataOffset));
	const FVector BoneVelDirection = LinearVelocity.GetSafeNormal();
	const FVector BonePos = DrawParams.GetCachedPosition(SampleTimeOffset, SchemaBoneIdx);

	if (EnumHasAnyFlags(DrawParams.Flags, EDebugDrawFlags::DrawSearchIndex))
	{
		DrawDebugLine(DrawParams.World, BonePos, BonePos + LinearVelocity * LinearVelocityScale, Color, bPersistent, LifeTime, DepthPriority);
	}
	else
	{
		const float AdjustedThickness = EnumHasAnyFlags(DrawParams.Flags, EDebugDrawFlags::DrawFast) ? 0.f : 1.f;
		DrawDebugLine(DrawParams.World, BonePos + BoneVelDirection * 2.f, BonePos + LinearVelocity * LinearVelocityScale, Color, bPersistent, LifeTime, DepthPriority, AdjustedThickness);
	}
#endif // ENABLE_DRAW_DEBUG
}

#if WITH_EDITOR
FString UPoseSearchFeatureChannel_Velocity::GetLabel() const
{
	TStringBuilder<256> Label;
	if (const UPoseSearchFeatureChannel* OuterChannel = Cast<UPoseSearchFeatureChannel>(GetOuter()))
	{
		Label.Append(OuterChannel->GetLabel());
		Label.Append(TEXT("_"));
	}

	Label.Append(TEXT("Vel"));
	if (bNormalize)
	{
		Label.Append(TEXT("Dir"));
	}

	const FBoneReference& BoneReference = GetSchema()->BoneReferences[SchemaBoneIdx];
	if (BoneReference.HasValidSetup())
	{
		Label.Append(TEXT("_"));
		Label.Append(BoneReference.BoneName.ToString());
	}

	Label.Appendf(TEXT(" %.1f"), SampleTimeOffset);
	return Label.ToString();
}
#endif