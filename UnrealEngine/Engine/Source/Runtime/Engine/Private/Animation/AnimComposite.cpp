// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimComposite.cpp: Composite classes that contains sequence for each section
=============================================================================*/ 

#include "Animation/AnimComposite.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimComposite)

UAnimComposite::UAnimComposite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

#if WITH_EDITOR
bool UAnimComposite::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive /*= true*/) 
{
	return AnimationTrack.GetAllAnimationSequencesReferred(AnimationAssets, bRecursive);
}

void UAnimComposite::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap)
{
	AnimationTrack.ReplaceReferredAnimations(ReplacementMap);
}
#endif

bool UAnimComposite::IsNotifyAvailable() const
{
	return (GetPlayLength() > 0.f && (Super::IsNotifyAvailable() || AnimationTrack.IsNotifyAvailable()));
}

void UAnimComposite::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, FAnimNotifyContext& NotifyContext) const
{
	const bool bMovingForward = (RateScale >= 0.f);

	Super::GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, NotifyContext);

	if (bMovingForward)
	{
		if (PreviousPosition <= CurrentPosition)
		{
			AnimationTrack.GetAnimNotifiesFromTrackPositions(PreviousPosition, CurrentPosition, NotifyContext);
		}
		else
		{
			AnimationTrack.GetAnimNotifiesFromTrackPositions(PreviousPosition, GetPlayLength(), NotifyContext);
			AnimationTrack.GetAnimNotifiesFromTrackPositions(0.f, CurrentPosition, NotifyContext);
		}
	}
	else
	{
		if (PreviousPosition >= CurrentPosition)
		{
			AnimationTrack.GetAnimNotifiesFromTrackPositions(PreviousPosition, CurrentPosition, NotifyContext);
		}
		else
		{
			AnimationTrack.GetAnimNotifiesFromTrackPositions(PreviousPosition, 0.f, NotifyContext);
			AnimationTrack.GetAnimNotifiesFromTrackPositions(GetPlayLength(), CurrentPosition, NotifyContext);
		}
	}
}

void UAnimComposite::HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const
{
	Super::HandleAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, NotifyQueue);

	ExtractRootMotionFromTrack(AnimationTrack, PreviousTime, PreviousTime + MoveDelta, Context.RootMotionMovementParams);
}

void UAnimComposite::GetAnimationPose(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const
{
	AnimationTrack.GetAnimationPose(OutAnimationPoseData, ExtractionContext);

	FBlendedCurve& OutCurve = OutAnimationPoseData.GetCurve();

	FBlendedCurve CompositeCurve;
	CompositeCurve.InitFrom(OutCurve);
	EvaluateCurveData(CompositeCurve, ExtractionContext.CurrentTime);

	// combine both curve
	OutCurve.Combine(CompositeCurve);
}

EAdditiveAnimationType UAnimComposite::GetAdditiveAnimType() const
{
	int32 AdditiveType = AnimationTrack.GetTrackAdditiveType();

	if (AdditiveType != -1)
	{
		return (EAdditiveAnimationType)AdditiveType;
	}

	return AAT_None;
}

void UAnimComposite::EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock)
{
	AnimationTrack.EnableRootMotionSettingFromMontage(bInEnableRootMotion, InRootMotionRootLock);
}

bool UAnimComposite::HasRootMotion() const
{
	return AnimationTrack.HasRootMotion();
}

#if WITH_EDITOR
class UAnimSequence* UAnimComposite::GetAdditiveBasePose() const
{
	// @todo : for now it just picks up the first sequence
	return AnimationTrack.GetAdditiveBasePose();
}
#endif 

void UAnimComposite::InvalidateRecursiveAsset()
{
	// unfortunately we'll have to do this all the time
	// we don't know whether or not the nested assets are modified or not
	AnimationTrack.InvalidateRecursiveAsset(this);
}

bool UAnimComposite::ContainRecursive(TArray<UAnimCompositeBase*>& CurrentAccumulatedList)
{
	// am I included already?
	if (CurrentAccumulatedList.Contains(this))
	{
		return true;
	}

	// otherwise, add myself to it
	CurrentAccumulatedList.Add(this);

	// otherwise send to animation track
	if (AnimationTrack.ContainRecursive(CurrentAccumulatedList))
	{
		return true;
	}

	return false;
}

void UAnimComposite::SetCompositeLength(float InLength)
{
#if WITH_EDITOR		
	Controller->SetPlayLength(InLength);
#else
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetSequenceLength(InLength);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif	
}

void UAnimComposite::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	for (const FAnimSegment& AnimSegment : AnimationTrack.AnimSegments)
	{
		if(AnimSegment.IsPlayLengthOutOfDate())
		{
			UE_LOG(LogAnimation, Warning, TEXT("AnimComposite (%s) contains a Segment for which the playable length %f is out-of-sync with the represented AnimationSequence its length %f (%s). Please up-date the segment and resave."), *GetFullName(), (AnimSegment.AnimEndTime - AnimSegment.AnimStartTime), AnimSegment.GetAnimReference()->GetPlayLength(), *AnimSegment.GetAnimReference()->GetFullName());
		}
	}
#endif
}

