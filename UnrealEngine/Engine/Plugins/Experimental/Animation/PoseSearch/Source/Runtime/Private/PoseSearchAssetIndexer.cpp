// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "AnimationRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDatabaseIndexingContext.h"

namespace UE::PoseSearch
{
	
//////////////////////////////////////////////////////////////////////////
// FSamplingParam helpers
struct FSamplingParam
{
	float WrappedParam = 0.0f;
	int32 NumCycles = 0;

	// If the animation can't loop, WrappedParam contains the clamped value and whatever is left is stored here
	float Extrapolation = 0.0f;
};

static FSamplingParam WrapOrClampSamplingParam(bool bCanWrap, float SamplingParamExtent, float SamplingParam)
{
	// This is a helper function used by both time and distance sampling. A schema may specify time or distance
	// offsets that are multiple cycles of a clip away from the current pose being sampled.
	// And that time or distance offset may before the beginning of the clip (SamplingParam < 0.0f)
	// or after the end of the clip (SamplingParam > SamplingParamExtent). So this function
	// helps determine how many cycles need to be applied and what the wrapped value should be, clamping
	// if necessary.

	FSamplingParam Result;

	Result.WrappedParam = SamplingParam;

	const bool bIsSamplingParamExtentKindaSmall = SamplingParamExtent <= UE_KINDA_SMALL_NUMBER;
	if (!bIsSamplingParamExtentKindaSmall && bCanWrap)
	{
		if (SamplingParam < 0.0f)
		{
			while (Result.WrappedParam < 0.0f)
			{
				Result.WrappedParam += SamplingParamExtent;
				++Result.NumCycles;
			}
		}

		else
		{
			while (Result.WrappedParam > SamplingParamExtent)
			{
				Result.WrappedParam -= SamplingParamExtent;
				++Result.NumCycles;
			}
		}
	}

	const float ParamClamped = FMath::Clamp(Result.WrappedParam, 0.0f, SamplingParamExtent);
	if (ParamClamped != Result.WrappedParam)
	{
		check(bIsSamplingParamExtentKindaSmall || !bCanWrap);
		Result.Extrapolation = Result.WrappedParam - ParamClamped;
		Result.WrappedParam = ParamClamped;
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FAssetIndexer
FAssetIndexer::FAssetIndexer(const FBoneContainer& InBoneContainer, const FSearchIndexAsset& InSearchIndexAsset, 
	const FAssetSamplingContext& InSamplingContext, const UPoseSearchSchema& InSchema, const FAnimationAssetSampler& InAssetSampler)
: BoneContainer(InBoneContainer)
, CachedEntries()
, SearchIndexAsset(InSearchIndexAsset)
, SamplingContext(InSamplingContext)
, Schema(InSchema)
, AssetSampler(InAssetSampler)
{
}

void FAssetIndexer::AssignWorkingData(TArrayView<float> InOutFeatureVectorTable, TArrayView<FPoseMetadata> InOutPoseMetadata)
{
	FeatureVectorTable = InOutFeatureVectorTable;
	PoseMetadata = InOutPoseMetadata;
}

void FAssetIndexer::Process(int32 AssetIdx)
{
	check(Schema.IsValid());

	FMemMark Mark(FMemStack::Get());

	// Generate pose metadata
	const float SequenceLength = AssetSampler.GetPlayLength();
	for (int32 SampleIdx = GetBeginSampleIdx(); SampleIdx != GetEndSampleIdx(); ++SampleIdx)
	{
		const float SampleTime = FMath::Min(CalculateSampleTime(SampleIdx), SequenceLength);
		float CostAddend = Schema.BaseCostBias;
		float ContinuingPoseCostAddend = Schema.ContinuingPoseCostBias;
		bool bBlockTransition = false;

		TArray<UAnimNotifyState_PoseSearchBase*> NotifyStates;
		AssetSampler.ExtractPoseSearchNotifyStates(SampleTime, NotifyStates);
		for (const UAnimNotifyState_PoseSearchBase* PoseSearchNotify : NotifyStates)
		{
			if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchBlockTransition>())
			{
				bBlockTransition = true;
			}
			else if (const UAnimNotifyState_PoseSearchModifyCost* ModifyCostNotify = Cast<const UAnimNotifyState_PoseSearchModifyCost>(PoseSearchNotify))
			{
				CostAddend = ModifyCostNotify->CostAddend;
			}
		}

		if (AssetSampler.IsLoopable())
		{
			CostAddend += Schema.LoopingCostBias;
		}

		PoseMetadata[GetVectorIdx(SampleIdx)].Init(AssetIdx, bBlockTransition, CostAddend);
	}

	// Generate pose features data
	if (Schema.SchemaCardinality > 0)
	{
		for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Schema.GetChannels())
		{
			ChannelPtr->IndexAsset(*this);
		}
	}

	// Computing stats
	ComputeStats();
}

void FAssetIndexer::ComputeStats()
{
	Stats = FStats();

	check(SamplingContext.FiniteDelta > UE_KINDA_SMALL_NUMBER);

	for (int32 SampleIdx = GetBeginSampleIdx(); SampleIdx != GetEndSampleIdx(); ++SampleIdx)
	{
		const float SampleTime = FMath::Min(CalculateSampleTime(SampleIdx), AssetSampler.GetPlayLength());

		bool AnyClamped = false;
		const FTransform TrajTransformsPast = GetTransform(SampleTime - SamplingContext.FiniteDelta, AnyClamped);
		if (!AnyClamped)
		{
			const FTransform TrajTransformsPresent = GetTransform(SampleTime, AnyClamped);
			if (!AnyClamped)
			{
				const FTransform TrajTransformsFuture = GetTransform(SampleTime + SamplingContext.FiniteDelta, AnyClamped);
				if (!AnyClamped)
				{
					// if any transform is clamped we just skip the sample entirely
					const FVector LinearVelocityPresent = (TrajTransformsPresent.GetTranslation() - TrajTransformsPast.GetTranslation()) / SamplingContext.FiniteDelta;
					const FVector LinearVelocityFuture = (TrajTransformsFuture.GetTranslation() - TrajTransformsPresent.GetTranslation()) / SamplingContext.FiniteDelta;
					const FVector LinearAcceleration = (LinearVelocityFuture - LinearVelocityPresent) / SamplingContext.FiniteDelta;

					const float Speed = LinearVelocityPresent.Length();
					const float Acceleration = LinearAcceleration.Length();

					Stats.AccumulatedSpeed += Speed;
					Stats.MaxSpeed = FMath::Max(Stats.MaxSpeed, Speed);

					Stats.AccumulatedAcceleration += Acceleration;
					Stats.MaxAcceleration = FMath::Max(Stats.MaxAcceleration, Acceleration);

					++Stats.NumAccumulatedSamples;
				}
			}
		}
	}
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfo(float SampleTime) const
{
	FSampleInfo Sample;

	const float PlayLength = AssetSampler.GetPlayLength();
	const bool bCanWrap = AssetSampler.IsLoopable();

	float MainRelativeTime = SampleTime;
	if (SampleTime < 0.0f && bCanWrap)
	{
		// In this case we're sampling a loop backwards, so MainRelativeTime must adjust so the number of cycles is
		// counted correctly.
		MainRelativeTime += PlayLength;
	}

	const FSamplingParam SamplingParam = WrapOrClampSamplingParam(bCanWrap, PlayLength, MainRelativeTime);

	if (FMath::Abs(SamplingParam.Extrapolation) > SMALL_NUMBER)
	{
		Sample.bClamped = true;
		Sample.ClipTime = SamplingParam.WrappedParam + SamplingParam.Extrapolation;
		Sample.RootTransform = AssetSampler.ExtractRootTransform(Sample.ClipTime);
	}
	else
	{
		Sample.ClipTime = SamplingParam.WrappedParam;
		Sample.RootTransform = FTransform::Identity;

		// Find the remaining motion deltas after wrapping
		FTransform RootMotionRemainder = AssetSampler.ExtractRootTransform(Sample.ClipTime);

		const bool bNegativeSampleTime = SampleTime < 0.f;
		if (SamplingParam.NumCycles > 0 || bNegativeSampleTime)
		{
			const FTransform RootMotionLast = AssetSampler.GetTotalRootTransform();

			// Determine how to accumulate motion for every cycle of the anim. If the sample
			// had to be clamped, this motion will end up not getting applied below.
			// Also invert the accumulation direction if the requested sample was wrapped backwards.
			FTransform RootMotionPerCycle = RootMotionLast;

			if (bNegativeSampleTime)
			{
				RootMotionPerCycle = RootMotionPerCycle.Inverse();
			}
			
			// Invert motion deltas if we wrapped backwards
			if (bNegativeSampleTime)
			{
				RootMotionRemainder.SetToRelativeTransform(RootMotionLast);
			}

			// Note if the sample was clamped, no motion will be applied here because NumCycles will be zero
			int32 CyclesRemaining = SamplingParam.NumCycles;
			while (CyclesRemaining--)
			{
				Sample.RootTransform = RootMotionPerCycle * Sample.RootTransform;
			}
		}

		Sample.RootTransform = RootMotionRemainder * Sample.RootTransform;
	}

	return Sample;
}

FTransform FAssetIndexer::MirrorTransform(const FTransform& Transform) const
{
	return SearchIndexAsset.bMirrored ? SamplingContext.MirrorTransform(Transform) : Transform;
}

FAssetIndexer::CachedEntry& FAssetIndexer::GetEntry(float SampleTime)
{
	using namespace UE::Anim;

	CachedEntry* Entry = CachedEntries.Find(SampleTime);
	if (!Entry)
	{
		Entry = &CachedEntries.Add(SampleTime);
		Entry->SampleTime = SampleTime;

		if (!BoneContainer.IsValid())
		{
			UE_LOG(LogPoseSearch,
				Warning,
				TEXT("Invalid BoneContainer encountered in FAssetIndexer::GetEntry. Asset: %s. Schema: %s. BoneContainerAsset: %s. NumBoneIndices: %d"),
				*GetNameSafe(AssetSampler.GetAsset()),
				*GetNameSafe(&Schema),
				*GetNameSafe(BoneContainer.GetAsset()),
				BoneContainer.GetCompactPoseNumBones());
		}

		const FAssetIndexer::FSampleInfo Sample = GetSampleInfo(SampleTime);
		float CurrentTime = Sample.ClipTime;

		const bool bLoopable = AssetSampler.IsLoopable();
		const float PlayLength = AssetSampler.GetPlayLength();

		if (!bLoopable)
		{
			CurrentTime = FMath::Clamp(CurrentTime, 0.f, PlayLength);
		}

		FCompactPose Pose;
		Pose.SetBoneContainer(&BoneContainer);
		AssetSampler.ExtractPose(CurrentTime, Pose);
		Pose[FCompactPoseBoneIndex(RootBoneIndexType)].SetIdentity();

		if (SearchIndexAsset.bMirrored && Schema.MirrorDataTable)
		{
			FAnimationRuntime::MirrorPose(
				Pose,
				Schema.MirrorDataTable->MirrorAxis,
				SamplingContext.CompactPoseMirrorBones,
				SamplingContext.ComponentSpaceRefRotations
			);
			// Note curves and attributes are not used during the indexing process and therefore don't need to be mirrored
		}

		Entry->ComponentSpacePose.InitPose(MoveTemp(Pose));
		Entry->RootTransform = Sample.RootTransform;
		Entry->bClamped = Sample.bClamped;
	}

	return *Entry;
}

// returns the transform in component space for the bone indexed by Schema->BoneReferences[SchemaBoneIdx] at SampleTime seconds
FTransform FAssetIndexer::GetComponentSpaceTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx)
{
	CachedEntry& Entry = GetEntry(SampleTime);
	bClamped = Entry.bClamped;

	if (!Schema.IsRootBone(SchemaBoneIdx))
	{
		return CalculateComponentSpaceTransform(Entry, SchemaBoneIdx);
	}

	return FTransform::Identity;
}

// returns the transform in animation space for the bone indexed by Schema->BoneReferences[SchemaBoneIdx] at SampleTime seconds
FTransform FAssetIndexer::GetTransform(float SampleTime, bool& bClamped, int8 SchemaBoneIdx)
{
	CachedEntry& Entry = GetEntry(SampleTime);
	bClamped = Entry.bClamped;

	const FTransform MirroredRootTransform = MirrorTransform(Entry.RootTransform);
	if (!Schema.IsRootBone(SchemaBoneIdx))
	{
		return CalculateComponentSpaceTransform(Entry, SchemaBoneIdx) * MirroredRootTransform;
	}

	return MirroredRootTransform;
}

FTransform FAssetIndexer::CalculateComponentSpaceTransform(FAssetIndexer::CachedEntry& Entry, int8 SchemaBoneIdx)
{
	const FBoneReference& BoneReference = Schema.BoneReferences[SchemaBoneIdx];
	const FCompactPoseBoneIndex CompactBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneReference.BoneIndex));
	return Entry.ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIndex);
}

float FAssetIndexer::CalculateSampleTime(int32 SampleIdx) const
{
	return SampleIdx / float(Schema.SampleRate);
}

FQuat FAssetIndexer::GetSampleRotation(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + PermutationOriginTimeOffset;

	// @todo: add support for SchemaSampleBoneIdx
	if (!Schema.IsRootBone(SchemaOriginBoneIdx))
	{
		UE_LOG(LogPoseSearch,
			Error,
			TEXT("FAssetIndexer::GetSampleRotation: support for non root origin bones not implemented (bone: '%s', schema: '%s'"), 
			*Schema.BoneReferences[SchemaOriginBoneIdx].BoneName.ToString(),
			*GetNameSafe(&Schema));
	}

	bool bUnused;
	if (SampleTime == OriginTime)
	{
		return GetComponentSpaceTransform(SampleTime, bUnused, SchemaSampleBoneIdx).GetRotation();
	}

	const FTransform RootBoneTransform = GetTransform(OriginTime, bUnused, RootSchemaBoneIdx);
	FTransform BoneTransform = GetTransform(SampleTime, bUnused, SchemaSampleBoneIdx);
	BoneTransform.SetToRelativeTransform(RootBoneTransform);
	return BoneTransform.GetRotation();
}

FVector FAssetIndexer::GetSamplePosition(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, EPermutationTimeType PermutationTimeType)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + PermutationOriginTimeOffset;
	
	bool bUnused;
	return GetSamplePositionInternal(SampleTime, OriginTime, bUnused, SchemaSampleBoneIdx, SchemaOriginBoneIdx);
}

FVector FAssetIndexer::GetSamplePositionInternal(float SampleTime, float OriginTime, bool& bClamped, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx)
{
	if (SampleTime == OriginTime)
	{
		if (Schema.IsRootBone(SchemaOriginBoneIdx))
		{
			return GetComponentSpaceTransform(SampleTime, bClamped, SchemaSampleBoneIdx).GetTranslation();
		}

		bool bOriginClamped;
		const FVector SampleBonePosition = GetComponentSpaceTransform(SampleTime, bClamped, SchemaSampleBoneIdx).GetTranslation();
		const FVector OriginBonePosition = GetComponentSpaceTransform(OriginTime, bOriginClamped, SchemaOriginBoneIdx).GetTranslation();
		bClamped |= bOriginClamped;
		return SampleBonePosition - OriginBonePosition;
	}

	bool Unused;
	const FTransform RootBoneTransform = GetTransform(OriginTime, Unused, RootSchemaBoneIdx);
	const FTransform SampleBoneTransform = GetTransform(SampleTime, bClamped, SchemaSampleBoneIdx);
	if (Schema.IsRootBone(SchemaOriginBoneIdx))
	{
		return RootBoneTransform.InverseTransformPosition(SampleBoneTransform.GetTranslation());
	}

	bool bOriginClamped;
	const FTransform OriginBoneTransform = GetTransform(OriginTime, bOriginClamped, SchemaOriginBoneIdx);
	bClamped |= bOriginClamped;
	const FVector DeltaBoneTranslation = SampleBoneTransform.GetTranslation() - OriginBoneTransform.GetTranslation();
	return RootBoneTransform.InverseTransformVector(DeltaBoneTranslation);
}

FVector FAssetIndexer::GetSampleVelocity(float SampleTimeOffset, int32 SampleIdx, int8 SchemaSampleBoneIdx, int8 SchemaOriginBoneIdx, bool bUseCharacterSpaceVelocities, EPermutationTimeType PermutationTimeType)
{
	float PermutationSampleTimeOffset = 0.f;
	float PermutationOriginTimeOffset = 0.f;
	UPoseSearchFeatureChannel::GetPermutationTimeOffsets(PermutationTimeType, CalculatePermutationTimeOffset(), PermutationSampleTimeOffset, PermutationOriginTimeOffset);

	const float Time = CalculateSampleTime(SampleIdx);
	const float SampleTime = Time + SampleTimeOffset + PermutationSampleTimeOffset;
	const float OriginTime = Time + PermutationOriginTimeOffset;
	const float FiniteDelta = SamplingContext.FiniteDelta;

	bool bClampedPast, bClampedPresent;
	const FVector BonePositionPast = GetSamplePositionInternal(SampleTime - FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime - FiniteDelta : OriginTime, bClampedPast, SchemaSampleBoneIdx, SchemaOriginBoneIdx);
	const FVector BonePositionPresent = GetSamplePositionInternal(SampleTime, OriginTime, bClampedPresent, SchemaSampleBoneIdx, SchemaOriginBoneIdx);

	FVector LinearVelocity;
	if (!bClampedPast)
	{
		LinearVelocity = (BonePositionPresent - BonePositionPast) / FiniteDelta;
	}
	else
	{
		bool Unused;
		const FVector BonePositionFuture = GetSamplePositionInternal(SampleTime + FiniteDelta, bUseCharacterSpaceVelocities ? OriginTime + FiniteDelta : OriginTime, Unused, SchemaSampleBoneIdx, SchemaOriginBoneIdx);
		LinearVelocity = (BonePositionFuture - BonePositionPresent) / FiniteDelta;
	}
	return LinearVelocity;
}

int32 FAssetIndexer::GetBeginSampleIdx() const
{
	return SearchIndexAsset.GetBeginSampleIdx();
}

int32 FAssetIndexer::GetEndSampleIdx() const
{
	return SearchIndexAsset.GetEndSampleIdx();
}

int32 FAssetIndexer::GetNumIndexedPoses() const
{
	return SearchIndexAsset.GetNumPoses();
}

int32 FAssetIndexer::GetVectorIdx(int32 SampleIdx) const
{
	return SampleIdx - GetBeginSampleIdx();
}

TArrayView<float> FAssetIndexer::GetPoseVector(int32 SampleIdx) const
{
	return MakeArrayView(&FeatureVectorTable[GetVectorIdx(SampleIdx) * Schema.SchemaCardinality], Schema.SchemaCardinality);
}

const UPoseSearchSchema* FAssetIndexer::GetSchema() const
{
	return &Schema;
}

float FAssetIndexer::CalculatePermutationTimeOffset() const
{
	check(Schema.PermutationsSampleRate > 0 && SearchIndexAsset.IsInitialized());
	const float PermutationTimeOffset = Schema.PermutationsTimeOffset + SearchIndexAsset.PermutationIdx / float(Schema.PermutationsSampleRate);
	return PermutationTimeOffset;
}

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
