// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchAssetIndexer.h"

#if WITH_EDITOR

#include "AnimationRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchSchema.h"

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
void FAssetIndexer::Reset()
{
	Output.FirstIndexedSample = 0;
	Output.LastIndexedSample = 0;
	Output.NumIndexedPoses = 0;

	Output.FeatureVectorTable.Reset(0);
	Output.PoseMetadata.Reset(0);
	Output.AllFeaturesNotAdded.Reset();
}

void FAssetIndexer::Init(const FAssetIndexingContext& InIndexingContext, const FBoneContainer& InBoneContainer)
{
	check(InIndexingContext.Schema);
	check(InIndexingContext.Schema->IsValid());
	check(InIndexingContext.AssetSampler);

	BoneContainer = InBoneContainer;
	IndexingContext = InIndexingContext;

	Reset();

	Output.FirstIndexedSample = FMath::FloorToInt(IndexingContext.RequestedSamplingRange.Min * IndexingContext.Schema->SampleRate);
	Output.LastIndexedSample = FMath::Max(0, FMath::CeilToInt(IndexingContext.RequestedSamplingRange.Max * IndexingContext.Schema->SampleRate));
	Output.NumIndexedPoses = Output.LastIndexedSample - Output.FirstIndexedSample + 1;

	Output.FeatureVectorTable.SetNumZeroed(IndexingContext.Schema->SchemaCardinality * Output.NumIndexedPoses);
	Output.PoseMetadata.SetNum(Output.NumIndexedPoses);
}

bool FAssetIndexer::Process()
{
	check(IndexingContext.Schema);
	check(IndexingContext.Schema->IsValid());
	check(IndexingContext.AssetSampler);

	FMemMark Mark(FMemStack::Get());

	IndexingContext.BeginSampleIdx = Output.FirstIndexedSample;
	IndexingContext.EndSampleIdx = Output.LastIndexedSample + 1;

	if (IndexingContext.Schema->SchemaCardinality > 0)
	{
		// Index each channel
		for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : IndexingContext.Schema->Channels)
		{
			if (ChannelPtr)
			{
				ChannelPtr->IndexAsset(*this, Output.FeatureVectorTable);
			}
		}
	}

	// Generate pose metadata
	for (int32 SampleIdx = IndexingContext.BeginSampleIdx; SampleIdx != IndexingContext.EndSampleIdx; ++SampleIdx)
	{
		const int32 PoseIdx = SampleIdx - Output.FirstIndexedSample;
		FPoseSearchPoseMetadata& OutputPoseMetadata = Output.PoseMetadata[PoseIdx];
		OutputPoseMetadata = GetMetadata(SampleIdx);
	}

	return true;
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfo(float SampleTime) const
{
	FSampleInfo Sample;

	check(IndexingContext.AssetSampler);

	const float PlayLength = IndexingContext.AssetSampler->GetPlayLength();
	const bool bCanWrap = IndexingContext.AssetSampler->IsLoopable();

	float MainRelativeTime = SampleTime;
	if (SampleTime < 0.0f && bCanWrap)
	{
		// In this case we're sampling a loop backwards, so MainRelativeTime must adjust so the number of cycles is
		// counted correctly.
		MainRelativeTime += PlayLength;
	}

	const FSamplingParam SamplingParam = WrapOrClampSamplingParam(bCanWrap, PlayLength, MainRelativeTime);

	Sample.Clip = IndexingContext.AssetSampler;

	if (FMath::Abs(SamplingParam.Extrapolation) > SMALL_NUMBER)
	{
		Sample.bClamped = true;
		Sample.ClipTime = SamplingParam.WrappedParam + SamplingParam.Extrapolation;
		const FTransform ClipRootMotion = Sample.Clip->ExtractRootTransform(Sample.ClipTime);
		const float ClipDistance = Sample.Clip->ExtractRootDistance(Sample.ClipTime);

		Sample.RootTransform = ClipRootMotion;
		Sample.RootDistance = ClipDistance;
	}
	else
	{
		Sample.ClipTime = SamplingParam.WrappedParam;

		const FTransform RootMotionLast = IndexingContext.AssetSampler->GetTotalRootTransform();
		float RootDistanceLast = IndexingContext.AssetSampler->GetTotalRootDistance();

		// Determine how to accumulate motion for every cycle of the anim. If the sample
		// had to be clamped, this motion will end up not getting applied below.
		// Also invert the accumulation direction if the requested sample was wrapped backwards.
		FTransform RootMotionPerCycle = RootMotionLast;
		float RootDistancePerCycle = RootDistanceLast;
		if (SampleTime < 0.0f)
		{
			RootMotionPerCycle = RootMotionPerCycle.Inverse();
			RootDistancePerCycle *= -1.f;
		}

		// Find the remaining motion deltas after wrapping
		FTransform RootMotionRemainder = Sample.Clip->ExtractRootTransform(Sample.ClipTime);
		float RootDistanceRemainder = Sample.Clip->ExtractRootDistance(Sample.ClipTime);

		// Invert motion deltas if we wrapped backwards
		if (SampleTime < 0.0f)
		{
			RootMotionRemainder.SetToRelativeTransform(RootMotionLast);
			RootDistanceRemainder = -(RootDistanceLast - RootDistanceRemainder);
		}

		Sample.RootTransform = FTransform::Identity;
		Sample.RootDistance = 0.f;

		// Note if the sample was clamped, no motion will be applied here because NumCycles will be zero
		int32 CyclesRemaining = SamplingParam.NumCycles;
		while (CyclesRemaining--)
		{
			Sample.RootTransform = RootMotionPerCycle * Sample.RootTransform;
			Sample.RootDistance += RootDistancePerCycle;
		}

		Sample.RootTransform = RootMotionRemainder * Sample.RootTransform;
		Sample.RootDistance += RootDistanceRemainder;
	}

	return Sample;
}

FAssetIndexer::FSampleInfo FAssetIndexer::GetSampleInfoRelative(float SampleTime, const FSampleInfo& Origin) const
{
	FSampleInfo Sample = GetSampleInfo(SampleTime);
	Sample.RootTransform.SetToRelativeTransform(Origin.RootTransform);
	Sample.RootDistance = Origin.RootDistance - Sample.RootDistance;
	return Sample;
}

FTransform FAssetIndexer::MirrorTransform(const FTransform& Transform) const
{
	return IndexingContext.bMirrored ? IndexingContext.SamplingContext->MirrorTransform(Transform) : Transform;
}

FPoseSearchPoseMetadata FAssetIndexer::GetMetadata(int32 SampleIdx) const
{
	const float SequenceLength = IndexingContext.AssetSampler->GetPlayLength();
	const float SampleTime = FMath::Min(SampleIdx * IndexingContext.Schema->GetSamplingInterval(), SequenceLength);

	FPoseSearchPoseMetadata Metadata;
	Metadata.CostAddend = IndexingContext.Schema->BaseCostBias;
	Metadata.ContinuingPoseCostAddend = IndexingContext.Schema->ContinuingPoseCostBias;

	TArray<UAnimNotifyState_PoseSearchBase*> NotifyStates;
	IndexingContext.AssetSampler->ExtractPoseSearchNotifyStates(SampleTime, NotifyStates);
	for (const UAnimNotifyState_PoseSearchBase* PoseSearchNotify : NotifyStates)
	{
		if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchBlockTransition>())
		{
			EnumAddFlags(Metadata.Flags, EPoseSearchPoseFlags::BlockTransition);
		}
		else if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchModifyCost>())
		{
			const UAnimNotifyState_PoseSearchModifyCost* ModifyCostNotify =
				Cast<const UAnimNotifyState_PoseSearchModifyCost>(PoseSearchNotify);
			Metadata.CostAddend = ModifyCostNotify->CostAddend;
		}
		else if (PoseSearchNotify->GetClass()->IsChildOf<UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>())
		{
			const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* ContinuingPoseCostBias =
				Cast<const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias>(PoseSearchNotify);
			Metadata.ContinuingPoseCostAddend = ContinuingPoseCostBias->CostAddend;
		}
	}
	return Metadata;
}

// returns the transform in character space for the bone indexed by Schema->BoneReferences[SchemaBoneIdx] 
// at SampleTime-OriginTime seconds ahead (or behind) the pose at OriginTime
FTransform FAssetIndexer::GetTransformAndCacheResults(float SampleTime, float OriginTime, int8 SchemaBoneIdx, bool& Clamped)
{
	// @todo: use an hashmap if we end up having too many entries
	CachedEntry* Entry = CachedEntries.FindByPredicate([SampleTime, OriginTime](const FAssetIndexer::CachedEntry& Entry)
		{
			return Entry.SampleTime == SampleTime && Entry.OriginTime == OriginTime;
		});

	const FAssetSamplingContext* SamplingContext = IndexingContext.SamplingContext;

	if (!Entry)
	{
		Entry = &CachedEntries[CachedEntries.AddDefaulted()];

		Entry->SampleTime = SampleTime;
		Entry->OriginTime = OriginTime;

		if (!BoneContainer.IsValid())
		{
			UE_LOG(LogPoseSearch,
				Warning,
				TEXT("Invalid BoneContainer encountered in FAssetIndexer::GetTransformAndCacheResults. Asset: %s. Schema: %s. BoneContainerAsset: %s. NumBoneIndices: %d"),
				*GetNameSafe(IndexingContext.AssetSampler->GetAsset()),
				*GetNameSafe(IndexingContext.Schema),
				*GetNameSafe(BoneContainer.GetAsset()),
				BoneContainer.GetCompactPoseNumBones());
		}

		Entry->Pose.SetBoneContainer(&BoneContainer);
		Entry->UnusedCurve.InitFrom(BoneContainer);

		IAssetIndexer::FSampleInfo Origin = GetSampleInfo(OriginTime);
		IAssetIndexer::FSampleInfo Sample = GetSampleInfoRelative(SampleTime, Origin);

		float CurrentTime = Sample.ClipTime;
		float PreviousTime = CurrentTime - SamplingContext->FiniteDelta;
		
		const bool bLoopable = Sample.Clip->IsLoopable();
		const float PlayLength = Sample.Clip->GetPlayLength();
		if (!bLoopable)
		{
			// if not loopable we clamp the pose at time zero or PlayLength
			if (PreviousTime < 0.f)
			{
				PreviousTime = 0.f;
				CurrentTime = FMath::Min(SamplingContext->FiniteDelta, PlayLength);
			}
			else if (CurrentTime > PlayLength)
			{
				CurrentTime = PlayLength;
				PreviousTime = FMath::Max(PlayLength - SamplingContext->FiniteDelta, 0.f);
			}
		}

		FDeltaTimeRecord DeltaTimeRecord;
		DeltaTimeRecord.Set(PreviousTime, CurrentTime - PreviousTime);
		// no need to extract root motion here, since we use the precalculated Sample.RootTransform as root transform for the Entry
		FAnimExtractContext ExtractionCtx(static_cast<double>(CurrentTime), false, DeltaTimeRecord, bLoopable);

		Sample.Clip->ExtractPose(ExtractionCtx, Entry->AnimPoseData);

		if (IndexingContext.bMirrored)
		{
			FAnimationRuntime::MirrorPose(
				Entry->AnimPoseData.GetPose(),
				IndexingContext.Schema->MirrorDataTable->MirrorAxis,
				SamplingContext->CompactPoseMirrorBones,
				SamplingContext->ComponentSpaceRefRotations
			);
			// Note curves and attributes are not used during the indexing process and therefore don't need to be mirrored
		}

		Entry->ComponentSpacePose.InitPose(Entry->Pose);
		Entry->RootTransform = Sample.RootTransform;
		Entry->Clamped = Sample.bClamped;
	}

	FTransform BoneTransform;
	const FBoneReference& BoneReference = IndexingContext.Schema->BoneReferences[SchemaBoneIdx];
	if (BoneReference.HasValidSetup())
	{
		FCompactPoseBoneIndex CompactBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneReference.BoneIndex));
		BoneTransform = Entry->ComponentSpacePose.GetComponentSpaceTransform(CompactBoneIndex) * MirrorTransform(Entry->RootTransform);
	}
	else
	{
		BoneTransform = MirrorTransform(Entry->RootTransform);
	}

	Clamped = Entry->Clamped;

	return BoneTransform;
}

} // namespace UE::PoseSearch
#endif // WITH_EDITOR
