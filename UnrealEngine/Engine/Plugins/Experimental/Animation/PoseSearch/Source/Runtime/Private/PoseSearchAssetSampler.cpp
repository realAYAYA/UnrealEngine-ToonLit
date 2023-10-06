// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchAssetSampler.h"
#include "AnimationRuntime.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/BlendSpace.h"
#include "Animation/MirrorDataTable.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchDefines.h"

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// Root motion extrapolation

static FTransform ExtrapolateRootMotion(FTransform SampleToExtrapolate, float SampleStart, float SampleEnd, float ExtrapolationTime)
{
	const float SampleDelta = SampleEnd - SampleStart;
	check(!FMath::IsNearlyZero(SampleDelta));

	// converting ExtrapolationTime to a positive number to avoid dealing with the negative extrapolation and inverting
	// transforms later on.
	const float AbsExtrapolationTime = FMath::Abs(ExtrapolationTime);
	const float AbsSampleDelta = FMath::Abs(SampleDelta);
	const FTransform AbsTimeSampleToExtrapolate = ExtrapolationTime >= 0.0f ? SampleToExtrapolate : SampleToExtrapolate.Inverse();

	// because we're extrapolating rotation, the extrapolation must be integrated over time
	const float SampleMultiplier = AbsExtrapolationTime / AbsSampleDelta;
	float IntegralNumSamples;
	float RemainingSampleFraction = FMath::Modf(SampleMultiplier, &IntegralNumSamples);
	int32 NumSamples = (int32)IntegralNumSamples;

	// adding full samples to the extrapolated root motion
	FTransform ExtrapolatedRootMotion = FTransform::Identity;
	for (int i = 0; i < NumSamples; ++i)
	{
		ExtrapolatedRootMotion = AbsTimeSampleToExtrapolate * ExtrapolatedRootMotion;
	}

	// and a blend with identity for whatever is left
	FTransform RemainingExtrapolatedRootMotion;
	RemainingExtrapolatedRootMotion.Blend(
		FTransform::Identity,
		AbsTimeSampleToExtrapolate,
		RemainingSampleFraction);

	ExtrapolatedRootMotion = RemainingExtrapolatedRootMotion * ExtrapolatedRootMotion;
	return ExtrapolatedRootMotion;
}

static FTransform ExtractRootTransformInternal(const UAnimMontage* AnimMontage, float StartTime, float EndTime)
{
	// @todo: add support for SlotName / multiple SlotAnimTracks
	if (AnimMontage->SlotAnimTracks.Num() != 1)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("ExtractRootTransformInternal: so far we support only montages with one SlotAnimTracks. %s has %d"), *AnimMontage->GetName(), AnimMontage->SlotAnimTracks.Num());
		return FTransform::Identity;
	}

	const FAnimTrack& RootMotionAnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
	TArray<FRootMotionExtractionStep> RootMotionExtractionSteps;
	RootMotionAnimTrack.GetRootMotionExtractionStepsForTrackRange(RootMotionExtractionSteps, StartTime, EndTime);
	FRootMotionMovementParams AccumulatedRootMotionParams;
	for (const FRootMotionExtractionStep& CurStep : RootMotionExtractionSteps)
	{
		if (CurStep.AnimSequence)
		{
			AccumulatedRootMotionParams.Accumulate(CurStep.AnimSequence->ExtractRootMotionFromRange(CurStep.StartPosition, CurStep.EndPosition));
		}
	}
	return AccumulatedRootMotionParams.GetRootMotionTransform();
}

static FTransform ExtractBlendSpaceRootTrackTransform(float Time, const TArray<FTransform>& AccumulatedRootTransform, int32 RootTransformSamplingRate)
{
	checkf(AccumulatedRootTransform.Num() > 0, TEXT("ProcessRootTransform must be run first"));

	const int32 Index = Time * RootTransformSamplingRate;
	const int32 FirstIndexClamped = FMath::Clamp(Index + 0, 0, AccumulatedRootTransform.Num() - 1);
	const int32 SecondIndexClamped = FMath::Clamp(Index + 1, 0, AccumulatedRootTransform.Num() - 1);
	const float Alpha = FMath::Fmod(Time * RootTransformSamplingRate, 1.0f);
	FTransform OutputTransform;
	OutputTransform.Blend(AccumulatedRootTransform[FirstIndexClamped], AccumulatedRootTransform[SecondIndexClamped], Alpha);
	return OutputTransform;
}

static FTransform ExtractBlendSpaceRootMotionFromRange(float StartTrackPosition, float EndTrackPosition, const TArray<FTransform>& AccumulatedRootTransform, int32 RootTransformSamplingRate)
{
	checkf(AccumulatedRootTransform.Num() > 0, TEXT("ProcessRootTransform must be run first"));

	FTransform RootTransformRefPose = ExtractBlendSpaceRootTrackTransform(0.f, AccumulatedRootTransform, RootTransformSamplingRate);

	FTransform StartTransform = ExtractBlendSpaceRootTrackTransform(StartTrackPosition, AccumulatedRootTransform, RootTransformSamplingRate);
	FTransform EndTransform = ExtractBlendSpaceRootTrackTransform(EndTrackPosition, AccumulatedRootTransform, RootTransformSamplingRate);

	// Transform to Component Space
	const FTransform RootToComponent = RootTransformRefPose.Inverse();
	StartTransform = RootToComponent * StartTransform;
	EndTransform = RootToComponent * EndTransform;

	return EndTransform.GetRelativeTransform(StartTransform);
}

static FTransform ExtractBlendSpaceRootMotion(float StartTime, float DeltaTime, bool bAllowLooping, float CachedPlayLength, const TArray<FTransform>& AccumulatedRootTransform, int32 RootTransformSamplingRate)
{
	FRootMotionMovementParams RootMotionParams;

	if (DeltaTime != 0.f)
	{
		bool const bPlayingBackwards = (DeltaTime < 0.f);

		float PreviousPosition = StartTime;
		float CurrentPosition = StartTime;
		float DesiredDeltaMove = DeltaTime;

		do
		{
			// Disable looping here. Advance to desired position, or beginning / end of animation 
			const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, CachedPlayLength);

			// Verify position assumptions
			//ensureMsgf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animation %s(Skeleton %s) : bPlayingBackwards(%d), PreviousPosition(%0.2f), Current Position(%0.2f)"),
			//	*GetName(), *GetNameSafe(GetSkeleton()), bPlayingBackwards, PreviousPosition, CurrentPosition);

			RootMotionParams.Accumulate(ExtractBlendSpaceRootMotionFromRange(PreviousPosition, CurrentPosition, AccumulatedRootTransform, RootTransformSamplingRate));

			// If we've hit the end of the animation, and we're allowed to loop, keep going.
			if ((AdvanceType == ETAA_Finished) && bAllowLooping)
			{
				const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
				DesiredDeltaMove -= ActualDeltaMove;

				PreviousPosition = bPlayingBackwards ? CachedPlayLength : 0.f;
				CurrentPosition = PreviousPosition;
			}
			else
			{
				break;
			}
		} while (true);
	}

	return RootMotionParams.GetRootMotionTransform();
}

static void ProcessRootTransform(const UBlendSpace* BlendSpace, const FVector& BlendParameters, float CachedPlayLength, const FBoneContainer& BoneContainer,
	int32 RootTransformSamplingRate, bool bIsLoopable, TArray<FTransform>& AccumulatedRootTransform)
{
	// Pre-compute root motion
	int32 NumRootSamples = FMath::Max(CachedPlayLength * RootTransformSamplingRate + 1, 1);
	AccumulatedRootTransform.SetNumUninitialized(NumRootSamples);

	TArray<FBlendSampleData> BlendSamples;
	int32 TriangulationIndex = 0;
	if (BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true))
	{
		FTransform RootMotionAccumulation = FTransform::Identity;

		AccumulatedRootTransform[0] = RootMotionAccumulation;

		for (int32 SampleIdx = 1; SampleIdx < NumRootSamples; ++SampleIdx)
		{
			float PreviousTime = float(SampleIdx - 1) / RootTransformSamplingRate;
			float CurrentTime = float(SampleIdx - 0) / RootTransformSamplingRate;

			FDeltaTimeRecord DeltaTimeRecord;
			DeltaTimeRecord.Set(PreviousTime, CurrentTime - PreviousTime);
			FAnimExtractContext ExtractionCtx(static_cast<double>(CurrentTime), true, DeltaTimeRecord, bIsLoopable);

			for (int32 BlendSampleIdex = 0; BlendSampleIdex < BlendSamples.Num(); BlendSampleIdex++)
			{
				float Scale = BlendSamples[BlendSampleIdex].Animation->GetPlayLength() / CachedPlayLength;

				FDeltaTimeRecord BlendSampleDeltaTimeRecord;
				BlendSampleDeltaTimeRecord.Set(DeltaTimeRecord.GetPrevious() * Scale, DeltaTimeRecord.Delta * Scale);

				BlendSamples[BlendSampleIdex].DeltaTimeRecord = BlendSampleDeltaTimeRecord;
				BlendSamples[BlendSampleIdex].PreviousTime = PreviousTime * Scale;
				BlendSamples[BlendSampleIdex].Time = CurrentTime * Scale;
			}

			FCompactPose Pose;
			FBlendedCurve BlendedCurve;
			UE::Anim::FStackAttributeContainer StackAttributeContainer;
			FAnimationPoseData AnimPoseData(Pose, BlendedCurve, StackAttributeContainer);

			Pose.SetBoneContainer(&BoneContainer);
			BlendedCurve.InitFrom(BoneContainer);

			BlendSpace->GetAnimationPose(BlendSamples, ExtractionCtx, AnimPoseData);

			const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();

			if (RootMotionProvider)
			{
				if (RootMotionProvider->HasRootMotion(StackAttributeContainer))
				{
					FTransform RootMotionDelta;
					RootMotionProvider->ExtractRootMotion(StackAttributeContainer, RootMotionDelta);
					RootMotionAccumulation = RootMotionDelta * RootMotionAccumulation;
				}
				else
				{
					UE_LOG(LogPoseSearch, Error, TEXT("ProcessRootTransform: Blend Space '%s' has no Root Motion Attribute"), *BlendSpace->GetName());
				}
			}
			else
			{
				UE_LOG(LogPoseSearch, Error, TEXT("ProcessRootTransform: Could not get Root Motion Provider for BlendSpace '%s'"), *BlendSpace->GetName());
			}

			AccumulatedRootTransform[SampleIdx] = RootMotionAccumulation;
		}
	}
}

static int32 GetHighestWeightSample(const TArray<struct FBlendSampleData>& SampleDataList)
{
	check(!SampleDataList.IsEmpty());
	int32 HighestWeightIndex = 0;
	float HighestWeight = SampleDataList[HighestWeightIndex].GetClampedWeight();
	for (int32 I = 1; I < SampleDataList.Num(); I++)
	{
		if (SampleDataList[I].GetClampedWeight() > HighestWeight)
		{
			HighestWeightIndex = I;
			HighestWeight = SampleDataList[I].GetClampedWeight();
		}
	}
	return HighestWeightIndex;
}

//////////////////////////////////////////////////////////////////////////
// FAssetSamplerBase
FAnimationAssetSampler::FAnimationAssetSampler(TObjectPtr<const UAnimationAsset> InAnimationAsset, const FVector& InBlendParameters, int32 InRootTransformSamplingRate)
{
	Init(InAnimationAsset, InBlendParameters, InRootTransformSamplingRate);
}

void FAnimationAssetSampler::Init(TObjectPtr<const UAnimationAsset> InAnimationAsset, const FVector& InBlendParameters, int32 InRootTransformSamplingRate)
{
	AnimationAsset = InAnimationAsset;
	BlendParameters = InBlendParameters;
	RootTransformSamplingRate = InRootTransformSamplingRate;
	CachedPlayLength = GetPlayLength(AnimationAsset.Get(), BlendParameters);
}

bool FAnimationAssetSampler::IsInitialized() const
{
	return AnimationAsset != nullptr;
}

float FAnimationAssetSampler::GetPlayLength(const UAnimationAsset* AnimAsset, const FVector& BlendParameters)
{
	float PlayLength = 0.f;
	if (AnimAsset)
	{
		if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimAsset))
		{
			FMemMark Mark(FMemStack::Get());

			TArray<FBlendSampleData> BlendSamples;
			int32 TriangulationIndex = 0;
			if (BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true))
			{
				PlayLength = BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);
			}

#if !NO_LOGGING
			const TArray<FName>* UniqueMarkerNames = const_cast<UBlendSpace*>(BlendSpace)->GetUniqueMarkerNames();
			if (UniqueMarkerNames && !UniqueMarkerNames->IsEmpty())
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("FAnimationAssetSampler::Init: sampling blend space (%s) with synch markers is currently not supported"), *BlendSpace->GetName());
			}
#endif // !NO_LOGGING
		}
		else
		{
			PlayLength = AnimAsset->GetPlayLength();
		}
	}

	return PlayLength;
}

const UAnimationAsset* FAnimationAssetSampler::GetAsset() const
{
	return AnimationAsset.Get();
}

float FAnimationAssetSampler::ToRealTime(float NormalizedTime) const
{
	// Asset player time for blend spaces is normalized [0, 1] so we convert the sampling / animation time to asset time by multiplying it by CachedPlayLength
	if (CachedPlayLength > UE_KINDA_SMALL_NUMBER && Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		check(NormalizedTime >= 0.f && NormalizedTime <= 1.f);
		const float RealTime = NormalizedTime * CachedPlayLength;
		return RealTime;
	}

	return NormalizedTime;
}

float FAnimationAssetSampler::ToNormalizedTime(float RealTime) const
{
	// Asset player time for blend spaces is normalized [0, 1] so we convert the sampling / animation time to asset time by dividing it by CachedPlayLength
	if (CachedPlayLength > UE_KINDA_SMALL_NUMBER && Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		const float NormalizedTime = RealTime / CachedPlayLength;
		check(NormalizedTime >= 0.f && NormalizedTime <= 1.f);
		return NormalizedTime;
	}

	return RealTime;
}

float FAnimationAssetSampler::GetPlayLength() const
{
	return CachedPlayLength;
}

bool FAnimationAssetSampler::IsLoopable() const
{
	if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset.Get()))
	{
		return SequenceBase->bLoop;
	}

	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		return BlendSpace->bLoop;
	}

	checkNoEntry();
	return false;
}

FTransform FAnimationAssetSampler::GetTotalRootTransform() const
{
	if (Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		const FTransform InitialRootTransform = ExtractBlendSpaceRootTrackTransform(0.f, AccumulatedRootTransform, RootTransformSamplingRate);
		const FTransform LastRootTransform = ExtractBlendSpaceRootTrackTransform(CachedPlayLength, AccumulatedRootTransform, RootTransformSamplingRate);
		const FTransform TotalRootTransform = LastRootTransform.GetRelativeTransform(InitialRootTransform);
		return TotalRootTransform;
	}

	if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimationAsset.Get()))
	{
		// @todo: add support for SlotName / multiple SlotAnimTracks
		if (AnimMontage->SlotAnimTracks.Num() != 1)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FAssetSamplerBase::GetTotalRootTransform: so far we support only montages with one SlotAnimTracks. %s has %d"), *AnimMontage->GetName(), AnimMontage->SlotAnimTracks.Num());
			return FTransform::Identity;
		}

		// @todo: optimize me
		const FTransform InitialRootTransform = ExtractRootTransform(0.f);
		const FTransform LastRootTransform = ExtractRootTransform(GetPlayLength());
		const FTransform TotalRootTransform = LastRootTransform.GetRelativeTransform(InitialRootTransform);
		return TotalRootTransform;
	}

	const FTransform InitialRootTransform = ExtractRootTransform(0.f);
	const FTransform LastRootTransform = ExtractRootTransform(GetPlayLength());
	const FTransform TotalRootTransform = LastRootTransform.GetRelativeTransform(InitialRootTransform);
	return TotalRootTransform;
}

void FAnimationAssetSampler::ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const
{
	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		if (BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true))
		{
			for (int32 BlendSampleIdex = 0; BlendSampleIdex < BlendSamples.Num(); BlendSampleIdex++)
			{
				float Scale = BlendSamples[BlendSampleIdex].Animation->GetPlayLength() / CachedPlayLength;

				FDeltaTimeRecord BlendSampleDeltaTimeRecord;
				BlendSampleDeltaTimeRecord.Set(ExtractionCtx.DeltaTimeRecord.GetPrevious() * Scale, ExtractionCtx.DeltaTimeRecord.Delta * Scale);

				BlendSamples[BlendSampleIdex].DeltaTimeRecord = BlendSampleDeltaTimeRecord;
				BlendSamples[BlendSampleIdex].PreviousTime = ExtractionCtx.DeltaTimeRecord.GetPrevious() * Scale;
				BlendSamples[BlendSampleIdex].Time = ExtractionCtx.CurrentTime * Scale;
			}

			BlendSpace->GetAnimationPose(BlendSamples, ExtractionCtx, OutAnimPoseData);
		}
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimationAsset.Get()))
	{
		// @todo: add support for SlotName / multiple SlotAnimTracks
		if (AnimMontage->SlotAnimTracks.Num() != 1)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("FAnimMontageSampler::ExtractPose: so far we support only montages with one SlotAnimTracks. %s has %d"), *AnimMontage->GetName(), AnimMontage->SlotAnimTracks.Num());
			OutAnimPoseData.GetPose().ResetToRefPose();
			return;
		}

		AnimMontage->SlotAnimTracks[0].AnimTrack.GetAnimationPose(OutAnimPoseData, ExtractionCtx);
	}
	else if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset.Get()))
	{
		SequenceBase->GetAnimationPose(OutAnimPoseData, ExtractionCtx);
	}
	else
	{
		checkNoEntry();
	}
}

void FAnimationAssetSampler::ExtractPose(float Time, FCompactPose& OutPose) const
{
	using namespace UE::Anim;

	FBlendedCurve UnusedCurve;
	FStackAttributeContainer UnusedAtrribute;
	FAnimationPoseData AnimPoseData = { OutPose, UnusedCurve, UnusedAtrribute };

	check(OutPose.IsValid());
	FBoneContainer& BoneContainer = OutPose.GetBoneContainer();
	UnusedCurve.InitFrom(BoneContainer);

	FDeltaTimeRecord DeltaTimeRecord;
	DeltaTimeRecord.Set(Time, 0.f);
	FAnimExtractContext ExtractionCtx(double(Time), false, DeltaTimeRecord, false);

	ExtractPose(ExtractionCtx, AnimPoseData);
}

FTransform FAnimationAssetSampler::ExtractRootTransform(float Time) const
{
	FTransform RootTransform = FTransform::Identity;
	
	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		if (IsLoopable())
		{
			RootTransform = ExtractBlendSpaceRootMotion(0.0f, Time, true, CachedPlayLength, AccumulatedRootTransform, RootTransformSamplingRate);
		}
		else
		{
			const float ClampedTime = FMath::Clamp(Time, 0.0f, CachedPlayLength);
			const float ExtrapolationTime = Time - ClampedTime;

			// If Time is less than zero, ExtrapolationTime will be negative. In this case, we extrapolate the beginning of the 
			// animation to estimate where the root would be at Time
			if (ExtrapolationTime < -SMALL_NUMBER)
			{
				FTransform SampleToExtrapolate = ExtractBlendSpaceRootMotionFromRange(0.0f, ExtrapolationSampleTime, AccumulatedRootTransform, RootTransformSamplingRate);

				const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
					SampleToExtrapolate,
					0.0f, ExtrapolationSampleTime,
					ExtrapolationTime);
				RootTransform = ExtrapolatedRootMotion;
			}
			else
			{
				RootTransform = ExtractBlendSpaceRootMotionFromRange(0.0f, ClampedTime, AccumulatedRootTransform, RootTransformSamplingRate);

				// If Time is greater than PlayLength, ExtrapolationTime will be a positive number. In this case, we extrapolate
				// the end of the animation to estimate where the root would be at Time
				if (ExtrapolationTime > SMALL_NUMBER)
				{
					FTransform SampleToExtrapolate = ExtractBlendSpaceRootMotionFromRange(CachedPlayLength - ExtrapolationSampleTime, CachedPlayLength, AccumulatedRootTransform, RootTransformSamplingRate);

					const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
						SampleToExtrapolate,
						CachedPlayLength - ExtrapolationSampleTime, CachedPlayLength,
						ExtrapolationTime);
					RootTransform = ExtrapolatedRootMotion * RootTransform;
				}
			}
		}
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimationAsset.Get()))
	{
		if (IsLoopable())
		{
			RootTransform = ExtractRootTransformInternal(AnimMontage, 0.f, Time);
		}
		else
		{
			const float PlayLength = GetPlayLength();
			const float ClampedTime = FMath::Clamp(Time, 0.f, PlayLength);
			const float ExtrapolationTime = Time - ClampedTime;

			// If Time is less than zero, ExtrapolationTime will be negative. In this case, we extrapolate the beginning of the 
			// animation to estimate where the root would be at Time
			if (ExtrapolationTime < -SMALL_NUMBER)
			{
				FTransform SampleToExtrapolate = ExtractRootTransformInternal(AnimMontage, 0.f, ExtrapolationSampleTime);

				const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
					SampleToExtrapolate,
					0.0f, ExtrapolationSampleTime,
					ExtrapolationTime);
				RootTransform = ExtrapolatedRootMotion;
			}
			else
			{
				RootTransform = ExtractRootTransformInternal(AnimMontage, 0.f, ClampedTime);

				// If Time is greater than PlayLength, ExtrapolationTime will be a positive number. In this case, we extrapolate
				// the end of the animation to estimate where the root would be at Time
				if (ExtrapolationTime > SMALL_NUMBER)
				{
					FTransform SampleToExtrapolate = ExtractRootTransformInternal(AnimMontage, PlayLength - ExtrapolationSampleTime, PlayLength);

					const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
						SampleToExtrapolate,
						PlayLength - ExtrapolationSampleTime, PlayLength,
						ExtrapolationTime);
					RootTransform = ExtrapolatedRootMotion * RootTransform;
				}
			}
		}
	}
	else if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset.Get()))
	{
		if (IsLoopable())
		{
			RootTransform = SequenceBase->ExtractRootMotion(0.0f, Time, true);
		}
		else
		{
			const float PlayLength = GetPlayLength();
			const float ClampedTime = FMath::Clamp(Time, 0.0f, PlayLength);
			const float ExtrapolationTime = Time - ClampedTime;

			// If Time is less than zero, ExtrapolationTime will be negative. In this case, we extrapolate the beginning of the 
			// animation to estimate where the root would be at Time
			if (ExtrapolationTime < -SMALL_NUMBER)
			{
				FTransform SampleToExtrapolate = SequenceBase->ExtractRootMotionFromRange(0.0f, ExtrapolationSampleTime);

				const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
					SampleToExtrapolate,
					0.0f, ExtrapolationSampleTime,
					ExtrapolationTime);
				RootTransform = ExtrapolatedRootMotion;
			}
			else
			{
				RootTransform = SequenceBase->ExtractRootMotionFromRange(0.0f, ClampedTime);

				// If Time is greater than PlayLength, ExtrapolationTime will be a positive number. In this case, we extrapolate
				// the end of the animation to estimate where the root would be at Time
				if (ExtrapolationTime > SMALL_NUMBER)
				{
					FTransform SampleToExtrapolate = SequenceBase->ExtractRootMotionFromRange(PlayLength - ExtrapolationSampleTime, PlayLength);

					const FTransform ExtrapolatedRootMotion = UE::PoseSearch::ExtrapolateRootMotion(
						SampleToExtrapolate,
						PlayLength - ExtrapolationSampleTime, PlayLength,
						ExtrapolationTime);
					RootTransform = ExtrapolatedRootMotion * RootTransform;
				}
			}
		}
	}
	else
	{
		checkNoEntry();
	}

	return RootTransform;
}

void FAnimationAssetSampler::Process(const FBoneContainer& BoneContainer)
{
	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		FMemMark Mark(FMemStack::Get());
		ProcessRootTransform(BlendSpace, BlendParameters, CachedPlayLength, BoneContainer, RootTransformSamplingRate, IsLoopable(), AccumulatedRootTransform);
	}
}

void FAnimationAssetSampler::ExtractPoseSearchNotifyStates(float Time, TArray<UAnimNotifyState_PoseSearchBase*>& NotifyStates) const
{
	float SampleTime = Time;
	FAnimNotifyContext NotifyContext;
	if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset.Get()))
	{
		if (BlendSpace->NotifyTriggerMode == ENotifyTriggerMode::HighestWeightedAnimation)
		{
			// Set up blend samples
			TArray<FBlendSampleData> BlendSamples;
			int32 TriangulationIndex = 0;
			if (BlendSpace->GetSamplesFromBlendInput(BlendParameters, BlendSamples, TriangulationIndex, true))
			{
				// Find highest weighted
				const int32 HighestWeightIndex = GetHighestWeightSample(BlendSamples);

				// getting pose search notifies in an interval of size ExtractionInterval, centered on Time
				SampleTime = Time * (BlendSamples[HighestWeightIndex].Animation->GetPlayLength() / CachedPlayLength);

				// Get notifies for highest weighted
				BlendSamples[HighestWeightIndex].Animation->GetAnimNotifies(
					(SampleTime - (ExtractionInterval * 0.5f)),
					ExtractionInterval,
					NotifyContext);
			}
		}
	}
	else if (const UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset.Get()))
	{
		// getting pose search notifies in an interval of size ExtractionInterval, centered on Time
		SequenceBase->GetAnimNotifies(Time - (ExtractionInterval * 0.5f), ExtractionInterval, NotifyContext);
	}
	else
	{
		checkNoEntry();
	}

	// check which notifies actually overlap Time and are of the right base type
	for (const FAnimNotifyEventReference& EventReference : NotifyContext.ActiveNotifies)
	{
		const FAnimNotifyEvent* NotifyEvent = EventReference.GetNotify();
		if (!NotifyEvent)
		{
			continue;
		}

		// @todo: is this condition necessary? can we just rely on the ExtractionInterval?
		if (NotifyEvent->GetTriggerTime() > SampleTime || NotifyEvent->GetEndTriggerTime() < SampleTime)
		{
			continue;
		}

		if (UAnimNotifyState_PoseSearchBase* PoseSearchAnimNotify = Cast<UAnimNotifyState_PoseSearchBase>(NotifyEvent->NotifyStateClass))
		{
			NotifyStates.Add(PoseSearchAnimNotify);
		}
	}
}

} // namespace UE::PoseSearch
