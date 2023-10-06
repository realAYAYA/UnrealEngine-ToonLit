// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchFeatureChannel_FilterCrashingLegs.h"
#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Position.h"

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
	ChannelCardinality = 1;
	Schema->SchemaCardinality += ChannelCardinality;

	LeftThighIdx = Schema->AddBoneReference(LeftThigh);
	RightThighIdx = Schema->AddBoneReference(RightThigh);
	LeftFootIdx = Schema->AddBoneReference(LeftFoot);
	RightFootIdx = Schema->AddBoneReference(RightFoot);
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::AddDependentChannels(UPoseSearchSchema* Schema) const
{
	if (Schema->bInjectAdditionalDebugChannels)
	{
		UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, LeftThigh.BoneName);
		UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, RightThigh.BoneName);
		UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, LeftFoot.BoneName);
		UPoseSearchFeatureChannel_Position::FindOrAddToSchema(Schema, 0.f, RightFoot.BoneName);
	}
}
void UPoseSearchFeatureChannel_FilterCrashingLegs::BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const
{
	using namespace UE::PoseSearch;

	const bool bIsCurrentResultValid = SearchContext.GetCurrentResult().IsValid() && SearchContext.GetCurrentResult().Database->Schema == InOutQuery.GetSchema();
	const bool bSkip = InputQueryPose != EInputQueryPose::UseCharacterPose && bIsCurrentResultValid;
	
	if (bSkip || !SearchContext.IsHistoryValid())
	{
		if (bIsCurrentResultValid)
		{
			FFeatureVectorHelper::Copy(InOutQuery.EditValues(), ChannelDataOffset, ChannelCardinality, SearchContext.GetCurrentResultPoseVector());
		}
		else
		{
			// we leave the InOutQuery set to zero since the SearchContext.History is invalid and it'll fail if we continue
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchFeatureChannel_FilterCrashingLegs::BuildQuery - Failed because Pose History Node is missing."));
		}
	}
	else
	{
		const FVector RightThighPosition = SearchContext.GetSamplePosition(0.f, InOutQuery.GetSchema(), RightThighIdx);
		const FVector LeftThighPosition = SearchContext.GetSamplePosition(0.f, InOutQuery.GetSchema(), LeftThighIdx);
		const FVector RightFootPosition = SearchContext.GetSamplePosition(0.f, InOutQuery.GetSchema(), RightFootIdx);
		const FVector LeftFootPosition = SearchContext.GetSamplePosition(0.f, InOutQuery.GetSchema(), LeftFootIdx);

		const float CrashingLegsValue = ComputeCrashingLegsValue(RightThighPosition, LeftThighPosition, RightFootPosition, LeftFootPosition);

		FFeatureVectorHelper::EncodeFloat(InOutQuery.EditValues(), ChannelDataOffset, CrashingLegsValue);
	}
}

#if ENABLE_DRAW_DEBUG
void UPoseSearchFeatureChannel_FilterCrashingLegs::DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const
{
	using namespace UE::PoseSearch;

	const float CrashingLegsValue = FFeatureVectorHelper::DecodeFloat(PoseVector, ChannelDataOffset);

	const FVector LeftThighPosition = DrawParams.ExtractPosition(PoseVector, 0.f, LeftThighIdx);
	const FVector RightThighPosition = DrawParams.ExtractPosition(PoseVector, 0.f, RightThighIdx);
	const FVector LeftFootPosition = DrawParams.ExtractPosition(PoseVector, 0.f, LeftFootIdx);
	const FVector RightFootPosition = DrawParams.ExtractPosition(PoseVector, 0.f, RightFootIdx);

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
	const FColor Color = LerpValue >= 0.f ? LerpColor(FColor::Green, FColor::Red, LerpValue) : LerpColor(FColor::Green, FColor::Orange, -LerpValue);

	DrawParams.DrawLine(LeftFootPosition, LeftFootPosition + CrossingLegsVector, Color);
	DrawParams.DrawLine(RightFootPosition, RightFootPosition - CrossingLegsVector, Color);
}
#endif // ENABLE_DRAW_DEBUG

// IPoseSearchFilter interface
bool UPoseSearchFeatureChannel_FilterCrashingLegs::IsFilterActive() const
{
	return true;
}

bool UPoseSearchFeatureChannel_FilterCrashingLegs::IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const UE::PoseSearch::FPoseMetadata& Metadata) const
{
	using namespace UE::PoseSearch;

	const float PoseValue = FFeatureVectorHelper::DecodeFloat(PoseValues, ChannelDataOffset);
	const float QueryValue = FFeatureVectorHelper::DecodeFloat(QueryValues, ChannelDataOffset);

	bool bIsPoseValid = true;
	if (FMath::Abs(QueryValue - PoseValue) > AllowedTolerance)
	{
		bIsPoseValid = false;
	}

	return bIsPoseValid;
}

#if WITH_EDITOR
void UPoseSearchFeatureChannel_FilterCrashingLegs::FillWeights(TArrayView<float> Weights) const
{
	for (int32 i = 0; i < ChannelCardinality; ++i)
	{
		Weights[ChannelDataOffset + i] = Weight;
	}
}

void UPoseSearchFeatureChannel_FilterCrashingLegs::IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const
{
	using namespace UE::PoseSearch;

	for (int32 SampleIdx = Indexer.GetBeginSampleIdx(); SampleIdx != Indexer.GetEndSampleIdx(); ++SampleIdx)
	{
		const FVector RightThighPosition = Indexer.GetSamplePosition(0.f, SampleIdx, RightThighIdx);
		const FVector LeftThighPosition = Indexer.GetSamplePosition(0.f, SampleIdx, LeftThighIdx);
		const FVector RightFootPosition = Indexer.GetSamplePosition(0.f, SampleIdx, RightFootIdx);
		const FVector LeftFootPosition = Indexer.GetSamplePosition(0.f, SampleIdx, LeftFootIdx);

		const float CrashingLegsValue = ComputeCrashingLegsValue(RightThighPosition, LeftThighPosition, RightFootPosition, LeftFootPosition);

		FFeatureVectorHelper::EncodeFloat(Indexer.GetPoseVector(SampleIdx), ChannelDataOffset, CrashingLegsValue);
	}
}

FString UPoseSearchFeatureChannel_FilterCrashingLegs::GetLabel() const
{
	return Super::GetLabel();
}
#endif