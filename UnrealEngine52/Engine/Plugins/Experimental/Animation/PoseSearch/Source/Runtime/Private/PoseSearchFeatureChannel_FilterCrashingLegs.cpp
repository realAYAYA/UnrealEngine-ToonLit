// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_FilterCrashingLegs.h"
#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"

static void ComputeThighsSideAndForward(const FVector& RightThighPos, const FVector& LeftThighPos, FVector& ThighsSide, FVector& ThighsForward)
{
	ThighsSide = (RightThighPos - LeftThighPos).GetSafeNormal2D(UE_KINDA_SMALL_NUMBER, FVector::LeftVector);
	ThighsForward = (ThighsSide ^ FVector::UpVector).GetSafeNormal2D(UE_KINDA_SMALL_NUMBER, FVector::ForwardVector);
}

static float ComputeCrashingLegsValue(const FVector& RightThighPos, const FVector& LeftThighPos, const FVector& RightFootPos, const FVector& LeftFootPos)
{
	FVector ThighsSide;
	FVector ThighsForward;
	ComputeThighsSideAndForward(RightThighPos, LeftThighPos, ThighsSide, ThighsForward);

	const FVector FeetDir = (RightFootPos - LeftFootPos).GetSafeNormal2D(UE_KINDA_SMALL_NUMBER, FVector::LeftVector);
	const float SideDot = FeetDir | ThighsSide;
	const float ForwardDot = FeetDir | ThighsForward;
	const float CrashingLegsValue = FMath::Atan2(ForwardDot, SideDot) / UE_PI;
	return CrashingLegsValue;
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::Finalize(UPoseSearchSchema* Schema)
{
	ChannelDataOffset = Schema->SchemaCardinality;
	ChannelCardinality = UE::PoseSearch::FFeatureVectorHelper::EncodeFloatCardinality;
	Schema->SchemaCardinality += ChannelCardinality;

	LeftThighIdx = Schema->AddBoneReference(LeftThigh);
	RightThighIdx = Schema->AddBoneReference(RightThigh);
	LeftFootIdx = Schema->AddBoneReference(LeftFoot);
	RightFootIdx = Schema->AddBoneReference(RightFoot);
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::FillWeights(TArray<float>& Weights) const
{
	check(ChannelCardinality == UE::PoseSearch::FFeatureVectorHelper::EncodeFloatCardinality);
	Weights[ChannelDataOffset] = Weight;
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const
{
	using namespace UE::PoseSearch;

	const FAssetIndexingContext& IndexingContext = Indexer.GetIndexingContext();

	for (int32 SampleIdx = IndexingContext.BeginSampleIdx; SampleIdx != IndexingContext.EndSampleIdx; ++SampleIdx)
	{
		const float SubsampleTime = SampleIdx * IndexingContext.Schema->GetSamplingInterval();

		bool bUnused;
		const FTransform RightThighTransform = Indexer.GetTransformAndCacheResults(SubsampleTime, SubsampleTime, RightThighIdx, bUnused);
		const FTransform LeftThighTransform = Indexer.GetTransformAndCacheResults(SubsampleTime, SubsampleTime, LeftThighIdx, bUnused);
		const FTransform RightFootTransform = Indexer.GetTransformAndCacheResults(SubsampleTime, SubsampleTime, RightFootIdx, bUnused);
		const FTransform LeftFootTransform = Indexer.GetTransformAndCacheResults(SubsampleTime, SubsampleTime, LeftFootIdx, bUnused);

		const float CrashingLegsValue = ComputeCrashingLegsValue(RightThighTransform.GetTranslation(), LeftThighTransform.GetTranslation(), RightFootTransform.GetTranslation(), LeftFootTransform.GetTranslation());

		int32 DataOffset = ChannelDataOffset;
		const int32 VectorIdx = SampleIdx - IndexingContext.BeginSampleIdx;
		FFeatureVectorHelper::EncodeFloat(IndexingContext.GetPoseVector(VectorIdx, FeatureVectorTable), DataOffset, CrashingLegsValue);
	}
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	const bool bIsCurrentResultValid = SearchContext.CurrentResult.IsValid();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid && SearchContext.CurrentResult.Database->Schema == InOutQuery.GetSchema();
	
	int32 DataOffset = ChannelDataOffset;
	if (bSkip || !SearchContext.History)
	{
		if (bIsCurrentResultValid)
		{
			const float LerpValue = InputQueryPose == EInputQueryPose::UseInterpolatedContinuingPose ? SearchContext.CurrentResult.LerpValue : 0.f;
			FFeatureVectorHelper::EncodeFloat(InOutQuery.EditValues(), DataOffset, SearchContext.GetCurrentResultPrevPoseVector(), SearchContext.GetCurrentResultPoseVector(), SearchContext.GetCurrentResultNextPoseVector(), LerpValue);
		}
		// else leave the InOutQuery set to zero since the SearchContext.History is invalid and it'll fail if we continue
	}
	else
	{
		const float SampleTime = 0.f;
		const FTransform LeftThighTransform = SearchContext.TryGetTransformAndCacheResults(SampleTime, InOutQuery.GetSchema(), LeftThighIdx);
		const FTransform RightThighTransform = SearchContext.TryGetTransformAndCacheResults(SampleTime, InOutQuery.GetSchema(), RightThighIdx);
		const FTransform LeftFootTransform = SearchContext.TryGetTransformAndCacheResults(SampleTime, InOutQuery.GetSchema(), LeftFootIdx);
		const FTransform RightFootTransform = SearchContext.TryGetTransformAndCacheResults(SampleTime, InOutQuery.GetSchema(), RightFootIdx);

		const float CrashingLegsValue = ComputeCrashingLegsValue(RightThighTransform.GetTranslation(), LeftThighTransform.GetTranslation(), RightFootTransform.GetTranslation(), LeftFootTransform.GetTranslation());
		FFeatureVectorHelper::EncodeFloat(InOutQuery.EditValues(), DataOffset, CrashingLegsValue);
	}
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
#if ENABLE_DRAW_DEBUG
	using namespace UE::PoseSearch;

	const float CrashingLegsValue = FFeatureVectorHelper::DecodeFloatAtOffset(PoseVector, ChannelDataOffset);

	const float LifeTime = DrawParams.DefaultLifeTime;
	const uint8 DepthPriority = ESceneDepthPriorityGroup::SDPG_Foreground + 2;
	const bool bPersistent = EnumHasAnyFlags(DrawParams.Flags, EDebugDrawFlags::Persistent);

	const FVector LeftThighPosition = DrawParams.GetCachedPosition(0.f, LeftThighIdx);
	const FVector RightThighPosition = DrawParams.GetCachedPosition(0.f, RightThighIdx);
	const FVector LeftFootPosition = DrawParams.GetCachedPosition(0.f, LeftFootIdx);
	const FVector RightFootPosition = DrawParams.GetCachedPosition(0.f, RightFootIdx);

	const float FeetDistance = (RightFootPosition - LeftFootPosition).Length();

	FVector ThighsSide;
	FVector ThighsForward;
	ComputeThighsSideAndForward(RightThighPosition, LeftThighPosition, ThighsSide, ThighsForward);
	const FVector CrossingLegsVector = (FMath::Cos(CrashingLegsValue * UE_PI) * ThighsSide + FMath::Sin(CrashingLegsValue * UE_PI) * ThighsForward) * FeetDistance;

	auto LerpColor = [](FColor A, FColor B, float T) -> FColor
	{
		return FColor(
			FMath::RoundToInt(float(A.R) * (1.f - T) + float(B.R) * T),
			FMath::RoundToInt(float(A.G) * (1.f - T) + float(B.G) * T),
			FMath::RoundToInt(float(A.B) * (1.f - T) + float(B.B) * T),
			FMath::RoundToInt(float(A.A) * (1.f - T) + float(B.A) * T));
	};

	// when CrossingLegsValue is greater than .5 or lesser than -.5 we draw in Red or Orange
	const float LerpValue = FMath::Min(CrashingLegsValue * 2.f, 1.f);
	const FColor Color = LerpValue >= 0.f ?
		LerpColor(FColor::Green, FColor::Red, LerpValue) :
		LerpColor(FColor::Green, FColor::Orange, -LerpValue);

	DrawDebugLine(DrawParams.World, LeftFootPosition, LeftFootPosition + CrossingLegsVector, Color, bPersistent, LifeTime, DepthPriority);
	DrawDebugLine(DrawParams.World, RightFootPosition, RightFootPosition - CrossingLegsVector, Color, bPersistent, LifeTime, DepthPriority);
#endif // ENABLE_DRAW_DEBUG
}

// IPoseFilter interface
bool UPoseSearchFeatureChannel_FilterCrashingLegs::IsPoseFilterActive() const
{
	return true;
}

bool UPoseSearchFeatureChannel_FilterCrashingLegs::IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const
{
	using namespace UE::PoseSearch;

	const float PoseValue = FFeatureVectorHelper::DecodeFloatAtOffset(PoseValues, ChannelDataOffset);
	const float QueryValue = FFeatureVectorHelper::DecodeFloatAtOffset(QueryValues, ChannelDataOffset);

	bool bIsPoseValid = true;
	if (FMath::Abs(QueryValue - PoseValue) > AllowedTolerance)
	{
		bIsPoseValid = false;
	}

	return bIsPoseValid;
}

#if WITH_EDITOR
FString UPoseSearchFeatureChannel_FilterCrashingLegs::GetLabel() const
{
	return Super::GetLabel();
}
#endif